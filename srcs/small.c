/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   small.c                                            :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: rotrojan <rotrojan@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/05/13 12:29:46 by rotrojan          #+#    #+#             */
/*   Updated: 2026/06/17 21:49:45 by rotrojan         ###   ########.fr       */
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

static s_small_zone *new_small_zone(s_arena *arena)
{
	s_small_zone *zone;
	char         *prologue;
	char         *epilogue;
	char         *wild_chunk;

	zone = new_zone(SMALL_ZONE, SMALL_ZONE_SIZE, arena);
	if (zone == NULL)
		return NULL;

	prologue   = (char *)zone + SMALL_QUANTUM - 2 * TAG_SIZE;
	epilogue   = (char *)zone + SMALL_ZONE_SIZE;
	wild_chunk = (char *)zone + SMALL_QUANTUM;

	ft_memset(zone->bin, 0, sizeof(zone->bin));

	/**
	 * The start of the heap is shifted to take account for the prologue
	 * chunk (e.g one header and one footer) and the epilogue (one footer).
	 */

	/* prologue */
	PUT_TAG(CHUNK_HDR(prologue), TAG(2 * TAG_SIZE, IN_USE));
	PUT_TAG(CHUNK_FTR(prologue), TAG(2 * TAG_SIZE, IN_USE));
	/* epilogue header (epilogue has no footer)*/
	PUT_TAG(CHUNK_HDR(epilogue), TAG(0, IN_USE));
	/* first free chunk (wild chunk)*/
	PUT_TAG(CHUNK_HDR(wild_chunk),
		TAG(SMALL_ZONE_SIZE - SMALL_QUANTUM, FREE));
	PUT_TAG(CHUNK_FTR(wild_chunk),
		TAG(SMALL_ZONE_SIZE - SMALL_QUANTUM, FREE));

	zone->bin[BIN_OVERSIZED] = (s_free_list *)wild_chunk;

	return zone;
}

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

static s_free_list *add_to_bin(s_free_list *chunk, s_free_list **free_list)
{
	chunk->next = *free_list;
	*free_list  = chunk;

	return chunk;
}

static s_free_list *pop_from_bin(s_free_list **free_list)
{
	s_free_list *chunk = *free_list;

	*free_list = (*free_list)->next;

	return chunk;
}

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
	 * Take account of boundary tags and round up to the upper SMALL_QUANTUM
	 * multiple.
	 * */
	real_size = (size + 2 * TAG_SIZE + SMALL_QUANTUM - 1) &
		    ~(SMALL_QUANTUM - 1);

	zone = get_small_zone(real_size, arena);
	if (zone == NULL) {
		pthread_mutex_unlock(&arena->mutex);
		return NULL;
	}

	bin     = zone->bin;
	bin_idx = BIN_IDX(real_size);

	/** If the matching bin has an available chunk, pop it, mark it as
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

void free_small(void *ptr, s_zone_hdr *zone_hdr)
{
	size_t        size  = GET_SIZE(CHUNK_HDR(ptr));
	s_small_zone *zone  = (s_small_zone *)zone_hdr;
	s_free_list  *freed = NULL;

	/* TODO: add checks alike to free_tiny (check state, check modulo 256 */
	PUT_TAG(CHUNK_HDR(ptr), TAG(size, FREE));
	PUT_TAG(CHUNK_FTR(ptr), TAG(size, FREE));

	freed = coalesce(ptr, zone->bin);
	size  = GET_SIZE(CHUNK_HDR(freed));
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
	old_real_size = GET_SIZE(CHUNK_HDR(ptr));
	if (size <= TINY_SIZE_MAX || size > SMALL_SIZE_MAX)
		goto new_alloc;

	if (old_real_size == real_size) {
		pthread_mutex_unlock(&arena->mutex);
		return ptr;
	}

	if (resize_chunk(ptr, real_size, zone->bin) != NULL) {
		pthread_mutex_unlock(&arena->mutex);
		/* Because `resize_chunk()` returns ptr on success.*/
		return ptr;
	}

new_alloc:
	pthread_mutex_unlock(&arena->mutex);
	new_ptr = malloc(size);
	if (new_ptr == NULL)
		return NULL;
	ft_memcpy(new_ptr, ptr, MIN(size, old_real_size - 2 * TAG_SIZE));
	free(ptr);
	return new_ptr;
}
