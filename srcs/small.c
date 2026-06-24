/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   small.c                                            :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: rotrojan <rotrojan@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/05/13 12:29:46 by rotrojan          #+#    #+#             */
/*   Updated: 2026/06/21 17:12:19 by rotrojan         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "small.h"

#include "arena.h"
#include "libft.h"
#include "malloc.h"
#include "zone.h"

#include <pthread.h>
#include <stddef.h>
#include <unistd.h>

/**
 * Create and lay out a fresh SMALL zone. Layout, low address to high:
 *
 *   [ zone struct + prologue ][ wild chunk (free) ......... ][ epilogue ]
 *   |<---- SMALL_QUANTUM --->||<---------- rest of the zone ----------->|
 *
 * The prologue and epilogue are zero-payload sentinels borrowed from CS:APP
 * ("Computer Systems: A Programmer's Perspective"): both are permanently
 * IN_USE so coalesce never walks off either end of the zone (a free-neighbour
 * check always meets an in-use wall). The prologue's header/footer sit at the
 * tail of the first quantum; the epilogue is a lone header (no footer) in the
 * final tag word. Everything between is one big free "wild" chunk, parked in
 * BIN_OVERSIZED, that malloc carves allocations from.
 */
static s_small_zone *new_small_zone(s_arena *arena)
{
	s_small_zone *zone;
	char         *prologue;
	char         *epilogue;
	char         *wild_chunk;

	zone = new_zone(SMALL_ZONE, SMALL_ZONE_SIZE, arena);
	if (zone == NULL)
		return NULL;

	/**
	 * Heap start is shifted past the first quantum to leave room for the
	 * zone struct and the prologue's header+footer.
	 */
	prologue   = (char *)zone + SMALL_QUANTUM - 2 * TAG_SIZE;
	epilogue   = (char *)zone + SMALL_ZONE_SIZE;
	wild_chunk = (char *)zone + SMALL_QUANTUM;

	ft_memset(zone->bin, 0, sizeof(zone->bin));

	/* prologue */
	PUT_TAG(CHUNK_HDR(prologue), TAG(2 * TAG_SIZE, IN_USE));
	PUT_TAG(CHUNK_FTR(prologue), TAG(2 * TAG_SIZE, IN_USE));
	/* epilogue header (epilogue has no footer) */
	PUT_TAG(CHUNK_HDR(epilogue), TAG(0, IN_USE));
	/* first free chunk (wild chunk) */
	PUT_TAG(CHUNK_HDR(wild_chunk),
		TAG(SMALL_ZONE_SIZE - SMALL_QUANTUM, FREE));
	PUT_TAG(CHUNK_FTR(wild_chunk),
		TAG(SMALL_ZONE_SIZE - SMALL_QUANTUM, FREE));

	zone->bin[BIN_OVERSIZED] = (s_free_list *)wild_chunk;

	return zone;
}

/**
 * Does `zone` hold a free chunk that can satisfy a request of total `size`?
 * A request can be served by its own size class or any larger one (a bigger
 * chunk is split), so scan from BIN_IDX(size) up through BIN_OVERSIZED.
 */
static int zone_has_chunk(size_t size, s_small_zone *zone)
{
	for (int i = BIN_IDX(size); i < NB_BINS; i++)
		if (zone->bin[i])
			return 1;

	return 0;
}

/**
 * Finds (or creates) a small zone in `arena` with a chunk big enough for
 * `size`. The caller (malloc_small) already holds the arena mutex, so this
 * does no locking of its own.
 */
static s_small_zone *get_small_zone(size_t size, s_arena *arena)
{
	s_small_zone *zone;

	zone = arena->small_hot;
	if (zone == NULL)
		goto new_zone;

	/* Look into hot zone first. */
	if (zone_has_chunk(size, zone))
		return zone;

	/**
	 * If hot zone does not have enough space, try other zones in the list.
	 */
	for (s_small_zone *current = arena->small_list; current != NULL;
	     current               = current->next) {
		if (current == zone)
			continue;
		if (zone_has_chunk(size, current)) {
			arena->small_hot = current;
			return current;
		}
	}

new_zone:
	zone = new_small_zone(arena);
	if (zone == NULL)
		return NULL;

	return zone;
}

/**
 * Detach `chunk` from its size class's free list. Bins are singly-linked, so
 * walk to find the predecessor and splice it out. Reaching the end without a
 * match means the bin/tag state is corrupt -- coalesce relies on a free
 * neighbour really being in its bin -- so report it rather than loop forever.
 */
static void remove_from_bin(s_free_list *chunk, s_free_list **bins)
{
	size_t        size    = GET_SIZE(CHUNK_HDR(chunk));
	s_free_list **bin     = &bins[BIN_IDX(size)];
	s_free_list **current = bin;

	while (*current != NULL) {
		if (*current == chunk)
			break;
		current = &(*current)->next;
	}

	if (*current == NULL)
		return ft_dprintf(STDERR_FILENO,
				  "Fatal: cannot coalesce chunks!\n");

	*current = ((*current)->next);
}

/**
 * Merge a just-freed `chunk` with any free immediate neighbours and return the
 * start of the resulting chunk. Boundary tags make this O(1): the previous
 * chunk's state is read from its footer (the word just before our header), the
 * next chunk's from its header. Four cases by neighbour state:
 *
 *   prev used, next used  nothing to merge, return chunk as-is;
 *   prev used, next free   absorb next: grow rightward, start stays;
 *   prev free, next used   absorb prev: grow leftward, start becomes prev;
 *   prev free, next free   absorb both, start becomes prev.
 *
 * Absorbed neighbours are pulled out of their bins first (their size, hence
 * bin, is about to change); only the header/footer tags bracketing the new
 * combined extent are rewritten. The caller re-inserts the result into the
 * right bin (coalesce does not, because some callers release the zone instead).
 */
static void *coalesce(void *chunk, s_free_list **bins)
{
	int    prev_state = GET_STATE(CHUNK_HDR(PREV_CHUNK(chunk)));
	int    next_state = GET_STATE(CHUNK_HDR(NEXT_CHUNK(chunk)));
	size_t size;

	if (prev_state == IN_USE && next_state == IN_USE)
		return chunk;

	if (prev_state == IN_USE && next_state == FREE) {
		remove_from_bin((s_free_list *)NEXT_CHUNK(chunk), bins);
		size = GET_SIZE(CHUNK_HDR(chunk)) +
		       GET_SIZE(CHUNK_HDR(NEXT_CHUNK(chunk)));
		PUT_TAG(CHUNK_HDR(chunk), TAG(size, FREE));
		PUT_TAG(CHUNK_FTR(chunk), TAG(size, FREE));
	} else if (prev_state == FREE && next_state == IN_USE) {
		remove_from_bin((s_free_list *)PREV_CHUNK(chunk), bins);
		size = GET_SIZE(CHUNK_HDR(chunk)) +
		       GET_SIZE(CHUNK_HDR(PREV_CHUNK(chunk)));
		PUT_TAG(CHUNK_FTR(chunk), TAG(size, FREE));
		PUT_TAG(CHUNK_HDR(PREV_CHUNK(chunk)), TAG(size, FREE));
		chunk = PREV_CHUNK(chunk);
	} else /* if(prev_state == FREE && next_state == FREE) */ {
		remove_from_bin((s_free_list *)NEXT_CHUNK(chunk), bins);
		remove_from_bin((s_free_list *)PREV_CHUNK(chunk), bins);
		size = GET_SIZE(CHUNK_HDR(chunk)) +
		       GET_SIZE(CHUNK_HDR(PREV_CHUNK(chunk))) +
		       GET_SIZE(CHUNK_HDR(NEXT_CHUNK(chunk)));
		PUT_TAG(CHUNK_HDR(PREV_CHUNK(chunk)), TAG(size, FREE));
		PUT_TAG(CHUNK_FTR(NEXT_CHUNK(chunk)), TAG(size, FREE));
		chunk = PREV_CHUNK(chunk);
	}

	return chunk;
}

/* Push a free chunk onto the head of its bin (LIFO, O(1)). */
static s_free_list *add_to_bin(s_free_list *chunk, s_free_list **free_list)
{
	chunk->next = *free_list;
	*free_list  = chunk;

	return chunk;
}

/* Pop the head chunk off a bin. Caller has checked the bin is non-empty. */
static s_free_list *pop_from_bin(s_free_list **free_list)
{
	s_free_list *chunk = *free_list;

	*free_list = (*free_list)->next;

	return chunk;
}

/**
 * Resize `chunk` to `new_size` total bytes in place, marking it IN_USE, and
 * return it -- or NULL if an in-place grow is impossible. Two directions:
 *
 *   shrink (new < old): always succeeds; the tail becomes the remainder.
 *   grow   (new > old): only if the immediately following chunk is free and
 *                       big enough; that neighbour is consumed.
 *
 * Either way, any leftover bytes beyond new_size become a free remainder chunk.
 * The remainder is coalesced with whatever follows (a shrink can butt against a
 * pre-existing free chunk) and dropped into the correct bin. Shared by
 * malloc_small (split an oversized chunk) and realloc_small (in-place resize).
 */
static void *resize_chunk(s_free_list *chunk, size_t new_size,
			  s_free_list **bin)
{
	size_t       old_size = GET_SIZE(CHUNK_HDR(chunk));
	size_t       remainder_size;
	size_t       combined_size;
	s_free_list *remainder_chunk;

	if (new_size < old_size) {
		remainder_size = old_size - new_size;
		goto resize;
	}

	/* Grow: need a free right neighbour, and the two together must fit. */
	if (GET_STATE(CHUNK_HDR(NEXT_CHUNK(chunk))) == IN_USE)
		return NULL;

	combined_size = GET_SIZE(CHUNK_HDR(chunk)) +
			GET_SIZE(CHUNK_HDR(NEXT_CHUNK(chunk)));
	if (combined_size < new_size)
		return NULL;

	remove_from_bin((s_free_list *)NEXT_CHUNK(chunk), bin);
	remainder_size = combined_size - new_size;

resize:
	PUT_TAG(CHUNK_HDR(chunk), TAG(new_size, IN_USE));
	PUT_TAG(CHUNK_FTR(chunk), TAG(new_size, IN_USE));

	/**
	 * Carve the leftover into its own free chunk. Coalescing it first folds
	 * in any free chunk that already followed (only possible on shrink),
	 * then it is filed in the bin matching its final size.
	 */
	if (remainder_size != 0) {
		remainder_chunk = (s_free_list *)NEXT_CHUNK(chunk);
		PUT_TAG(CHUNK_HDR(remainder_chunk), TAG(remainder_size, FREE));
		PUT_TAG(CHUNK_FTR(remainder_chunk), TAG(remainder_size, FREE));

		remainder_chunk = coalesce(remainder_chunk, bin);
		add_to_bin((s_free_list *)remainder_chunk,
			   &bin[BIN_IDX(GET_SIZE(CHUNK_HDR(remainder_chunk)))]);
	}
	return chunk;
}

void *malloc_small(size_t size)
{
	size_t        real_size;
	e_bin_idx     bin_idx;
	s_small_zone *zone;
	s_arena      *arena = get_arena();
	s_free_list **bin;
	void         *ptr;

	/**
	 * Total chunk size = payload + header + footer, rounded up to the next
	 * SMALL_QUANTUM multiple (adding QUANTUM-1 before masking off the low
	 * bits rounds up). This is the size class the request maps to.
	 */
	real_size = (size + 2 * TAG_SIZE + SMALL_QUANTUM - 1) &
		    ~(SMALL_QUANTUM - 1);

	zone = get_small_zone(real_size, arena);
	if (zone == NULL) {
		pthread_mutex_unlock(&arena->mutex);
		return NULL;
	}

	bin     = zone->bin;
	bin_idx = BIN_IDX(real_size);

	/**
	 * If the matching bin has an available chunk, pop it, mark it as
	 * IN_USE, and return it.
	 */
	if (bin[bin_idx] != NULL) {
		PUT_TAG(CHUNK_HDR(bin[bin_idx]), TAG(real_size, IN_USE));
		PUT_TAG(CHUNK_FTR(bin[bin_idx]), TAG(real_size, IN_USE));
		ptr = pop_from_bin(&bin[bin_idx]);
		pthread_mutex_unlock(&arena->mutex);
		return ptr;
	}

	/**
	 * Otherwise, look into the bigger bins. When the chunk is found, pop
	 * it, split it, mark the desired part as IN_USE, return it and put the
	 * remaining chunk in the proper bin.
	 */
	bin_idx++;
	while (bin_idx < NB_BINS) {
		if (bin[bin_idx]) {
			ptr = resize_chunk(pop_from_bin(&bin[bin_idx]),
					   real_size, bin);
			pthread_mutex_unlock(&arena->mutex);
			return ptr;
		}
		bin_idx++;
	}

	pthread_mutex_unlock(&arena->mutex);
	return NULL;
}

int small_ptr_is_valid(void *ptr)
{
	/* A real SMALL pointer is 256-aligned; anything else is foreign. */
	if ((uintptr_t)ptr & (SMALL_QUANTUM - 1))
		return 0;

	/**
	 * A live allocation's header reads IN_USE and matches its footer. A
	 * FREE header is a double-free (or never-malloced); a header/footer
	 * mismatch means the pointer is not a real chunk start (interior or
	 * foreign) -- the boundary-tag analog of TINY's is_start check.
	 */
	return GET_STATE(CHUNK_HDR(ptr)) != FREE &&
	       GET_TAG(CHUNK_HDR(ptr)) == GET_TAG(CHUNK_FTR(ptr));
}

void free_small(void *ptr, s_zone_hdr *zone_hdr)
{
	size_t        size;
	s_small_zone *zone  = (s_small_zone *)zone_hdr;
	s_free_list  *freed = NULL;

	/* Reject misaligned, interior, double-freed and foreign pointers. */
	if (!small_ptr_is_valid(ptr)) {
		return ft_dprintf(STDERR_FILENO,
				  "Fatal: invalid pointer passed to free!\n"
				  "%p: pointer was never malloced.\n",
				  ptr);
	}

	/* Flip the chunk to FREE, then merge with free neighbours. */
	size = GET_SIZE(CHUNK_HDR(ptr));
	PUT_TAG(CHUNK_HDR(ptr), TAG(size, FREE));
	PUT_TAG(CHUNK_FTR(ptr), TAG(size, FREE));

	freed = coalesce(ptr, zone->bin);
	size  = GET_SIZE(CHUNK_HDR(freed));

	/**
	 * If coalescing reconstituted the whole wild chunk, the zone is empty:
	 * hand it back to the OS instead of re-binning. Otherwise file the
	 * freed chunk in its size class.
	 */
	if (size == SMALL_ZONE_SIZE - SMALL_QUANTUM) {
		return release_zone(zone_hdr);
	}

	add_to_bin(freed, &zone->bin[BIN_IDX(size)]);
}

void *realloc_small(void *ptr, size_t size, s_zone_hdr *zone_hdr)
{
	void         *new_ptr;
	size_t        old_real_size;
	size_t        real_size = (size + 2 * TAG_SIZE + SMALL_QUANTUM - 1) &
				  ~(SMALL_QUANTUM - 1);
	s_small_zone *zone      = (s_small_zone *)zone_hdr;
	s_arena      *arena     = zone_hdr->arena;

	pthread_mutex_lock(&arena->mutex);

	/* Reject misaligned, interior, double-freed and foreign pointers. */
	if (!small_ptr_is_valid(ptr)) {
		pthread_mutex_unlock(&arena->mutex);
		ft_dprintf(STDERR_FILENO,
			   "Fatal: invalid pointer passed to realloc!\n"
			   "%p: pointer was never malloced.\n",
			   ptr);
		return NULL;
	}

	old_real_size = GET_SIZE(CHUNK_HDR(ptr));

	/**
	 * Leaving the SMALL class (shrinking into TINY or growing into LARGE)
	 * cannot be done in place: relocate.
	 */
	if (size <= TINY_SIZE_MAX || size > SMALL_SIZE_MAX)
		goto new_alloc;

	/* Same size class: nothing to do. */
	if (old_real_size == real_size) {
		pthread_mutex_unlock(&arena->mutex);
		return ptr;
	}

	/**
	 * Try to shrink or grow in place. resize_chunk returns ptr on success
	 * (always for a shrink, for a grow only if the right neighbour is free
	 * and big enough) and NULL when an in-place grow is impossible.
	 */
	if (resize_chunk(ptr, real_size, zone->bin) != NULL) {
		pthread_mutex_unlock(&arena->mutex);
		return ptr;
	}

new_alloc:
	/**
	 * Relocate. Drop the arena lock first: malloc/free re-enter the public
	 * API and take their own locks. Copy min(new payload, old payload):
	 * old_real_size includes the two tags, so subtract them to get the old
	 * payload size.
	 */
	pthread_mutex_unlock(&arena->mutex);
	new_ptr = malloc(size);
	if (new_ptr == NULL)
		return NULL;
	ft_memcpy(new_ptr, ptr, MIN(size, old_real_size - 2 * TAG_SIZE));
	free(ptr);
	return new_ptr;
}
