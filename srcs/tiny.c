/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   tiny.c                                             :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: rotrojan <rotrojan@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/12/16 19:57:52 by rotrojan          #+#    #+#             */
/*   Updated: 2026/06/22 21:02:12 by rotrojan         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "tiny.h"

#include "arena.h"
#include "bitmap.h"
#include "helpers.h"
#include "libft.h"
#include "malloc.h"
#include "zone.h"

#include <pthread.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/mman.h>
#include <unistd.h>

/* Sentinel returned by the bitmap search when no run of free chunks fits. */
#define NO_USABLE_INDEX SIZE_MAX

/**
 * Create and initialize a fresh TINY zone in `arena`. new_zone has already
 * zeroed nothing in particular, so clear both bitmaps and seed the free-chunk
 * hint past the header chunks (the header occupies the first NB_CHUNKS_TINY_HDR
 * chunks and must never be allocated over).
 */
static s_tiny_zone *new_tiny_zone(s_arena *arena)
{
	s_tiny_zone *zone = new_zone(TINY_ZONE, TINY_ZONE_SIZE, arena);

	if (zone == NULL)
		return NULL;

	ft_memset(zone->in_use, 0, sizeof(zone->in_use));
	ft_memset(zone->is_start, 0, sizeof(zone->is_start));
	zone->index_next_free_chunk = NB_CHUNKS_TINY_HDR;

	return zone;
}

/**
 * First-fit search for `needed_chunks` consecutive free chunks in `zone`,
 * starting from the zone's lower-bound hint. Returns the start chunk index or
 * NO_USABLE_INDEX if the zone cannot fit the run.
 */
static size_t get_usable_index(size_t needed_chunks, s_tiny_zone *zone)
{
	return bitmap_find_consecutive_zeros(zone->in_use,
					     ARRAY_SIZE(zone->in_use),
					     needed_chunks,
					     zone->index_next_free_chunk);
}

/**
 * Finds (or creates) a tiny zone in `arena` with room for `needed_chunks`.
 * The caller (malloc_tiny) already holds the arena mutex, so this does no
 * locking of its own.
 */
static s_tiny_zone *get_tiny_zone(size_t needed_chunks, size_t *index,
				  s_arena *arena)
{
	s_tiny_zone *zone;

	zone = arena->tiny_hot;
	if (zone == NULL)
		goto new_zone;

	/* Look into hot zone first. */
	*index = get_usable_index(needed_chunks, zone);
	if (*index != NO_USABLE_INDEX)
		return zone;

	/**
	 * If hot zone does not have enough space, try other zones in the list.
	 */
	for (s_tiny_zone *current = arena->tiny_list; current != NULL;
	     current              = current->next) {
		if (current == zone)
			continue;
		*index = get_usable_index(needed_chunks, current);
		if (*index != NO_USABLE_INDEX) {
			arena->tiny_hot = current;
			return current;
		}
	}

new_zone:
	/**
	 * No existing zone has room (or this is the first TINY allocation):
	 * mmap a new one. A fresh zone always has room past its header.
	 */
	zone = new_tiny_zone(arena);
	if (zone == NULL)
		return NULL;
	*index = zone->index_next_free_chunk;
	return zone;
}

/**
 * Count how many consecutive chunks the allocation starting at `ptr` spans.
 * Walk forward over in_use bits, stopping at the next is_start (the next
 * allocation), at a free chunk, at the 8-chunk class maximum, or at the end of
 * the bitmap. Shared by free_tiny and realloc_tiny, which need the old extent.
 */
static size_t get_nb_chunks_tiny_alloc(char *ptr, s_tiny_zone *zone)
{
	size_t nb_chunks = 1;
	size_t index     = (ptr - (char *)zone) / TINY_QUANTUM;

	while (nb_chunks < TINY_SIZE_MAX / TINY_QUANTUM &&
	       index + nb_chunks < BIT_ARRAY_SIZE(zone->in_use) &&
	       bitmap_get_bit(zone->in_use, index + nb_chunks) &&
	       !bitmap_get_bit(zone->is_start, index + nb_chunks))
		nb_chunks++;

	return nb_chunks;
}

void *malloc_tiny(size_t size)
{
	size_t       needed_chunks;
	size_t       index;
	s_tiny_zone *zone;
	s_arena     *arena = get_arena();

	/**
	 * If `size == 0`, we still allocate one chunk.
	 * From `man 3 malloc`: "If size is 0, then malloc() returns a
	 * unique pointer value that can later be successfully passed to
	 * free()".
	 */
	needed_chunks = MAX(1, DIV_CEIL(size, TINY_QUANTUM));
	zone          = get_tiny_zone(needed_chunks, &index, arena);
	if (zone == NULL) {
		pthread_mutex_unlock(&arena->mutex);
		return NULL;
	}

	/**
	 * Claim the run: mark every chunk in_use, flag the first as the
	 * allocation start. The hint can only move right past consumed space,
	 * but a smaller index may have been freed since, so keep the minimum.
	 */
	bitmap_set_range(zone->in_use, index, needed_chunks);
	bitmap_set_bit(zone->is_start, index);
	zone->index_next_free_chunk = MIN(zone->index_next_free_chunk, index);

	pthread_mutex_unlock(&arena->mutex);

	/* User pointer = zone base + chunk offset (already 16-aligned). */
	return (char *)zone + index * TINY_QUANTUM;
}

void free_tiny(void *ptr, s_zone_hdr *zone_hdr)
{
	uintptr_t    ptr_int  = (uintptr_t)ptr;
	s_tiny_zone *zone     = (s_tiny_zone *)zone_hdr;
	uintptr_t    zone_int = (uintptr_t)zone_hdr;
	size_t       index;
	size_t       nb_chunks;

	/* A real TINY pointer is chunk-aligned; anything else is foreign. */
	if (ptr_int & (TINY_QUANTUM - 1)) {
		return ft_dprintf(STDERR_FILENO,
				  "Fatal: invalid pointer passed to free!\n"
				  "%p: pointer is not aligned.\n",
				  ptr);
	}

	index = (ptr_int - zone_int) / TINY_QUANTUM;

	/**
	 * Must be both in use and the start of an allocation. This rejects
	 * double-frees (chunk no longer in_use) and interior pointers (in_use
	 * but not an is_start), the bitmap analog of SMALL's tag symmetry
	 * check.
	 */
	if (!bitmap_get_bit(zone->in_use, index) ||
	    !bitmap_get_bit(zone->is_start, index)) {
		return ft_dprintf(STDERR_FILENO,
				  "Fatal: invalid pointer passed to free!\n"
				  "%p: pointer was never malloced.\n",
				  ptr);
	}

	nb_chunks = get_nb_chunks_tiny_alloc(ptr, zone);

	bitmap_clear_range(zone->in_use, index, nb_chunks);
	bitmap_clear_bit(zone->is_start, index);

	/* Freed space may be the new leftmost gap, so pull the hint back. */
	zone->index_next_free_chunk = MIN(zone->index_next_free_chunk, index);

	/**
	 * If the whole zone is now free, hand it back to the OS. The zone is
	 * empty exactly when every in_use word is zero. Releasing eagerly keeps
	 * idle memory from accumulating; the arena mutex (held by the caller)
	 * makes this safe to do under the pin invariant.
	 */
	for (size_t i = 0; i < ARRAY_SIZE(zone->in_use); i++) {
		if (zone->in_use[i]) {
			return;
		}
	}

	release_zone(zone_hdr);
}

void *realloc_tiny(void *ptr, size_t size, s_zone_hdr *zone_hdr)
{
	void        *new_ptr;
	s_tiny_zone *zone  = (s_tiny_zone *)zone_hdr;
	s_arena     *arena = zone_hdr->arena;
	size_t       old_needed_chunks;
	size_t       new_needed_chunks;
	size_t       index;

	pthread_mutex_lock(&arena->mutex);
	old_needed_chunks = get_nb_chunks_tiny_alloc(ptr, (s_tiny_zone *)zone);

	/* Growing past the class ceiling cannot stay TINY: fall back to a move.
	 */
	if (size > TINY_SIZE_MAX)
		goto new_alloc;

	index             = ((char *)ptr - (char *)zone) / TINY_QUANTUM;
	new_needed_chunks = MAX(1, DIV_CEIL(size, TINY_QUANTUM));

	/**
	 * Shrink in place: clear the now-unused tail chunks (is_start stays
	 * set on the first chunk) and pull the hint back to the freed gap.
	 */
	if (new_needed_chunks < old_needed_chunks) {
		for (size_t i = index + new_needed_chunks;
		     i < index + old_needed_chunks; i++)
			bitmap_clear_bit(zone->in_use, i);
		zone->index_next_free_chunk = MIN(zone->index_next_free_chunk,
						  index + new_needed_chunks);
		pthread_mutex_unlock(&arena->mutex);
		return ptr;
	}

	/**
	 * Grow in place only if every extra chunk is currently free; if any is
	 * taken, we cannot extend here and must relocate.
	 */
	for (size_t i = index + old_needed_chunks;
	     i < index + new_needed_chunks; i++)
		if (bitmap_get_bit(zone->in_use, i))
			goto new_alloc;

	for (size_t i = index + old_needed_chunks;
	     i < index + new_needed_chunks; i++)
		bitmap_set_bit(zone->in_use, i);

	pthread_mutex_unlock(&arena->mutex);

	return ptr;

new_alloc:
	/**
	 * Relocate: drop the arena lock before re-entering the public API
	 * (malloc/free take their own locks), copy the old payload, free the
	 * original. We only reach here while growing, so the new block is
	 * always at least old_needed_chunks * TINY_QUANTUM bytes -- the full
	 * span of the old allocation -- making that copy length safe.
	 */
	pthread_mutex_unlock(&arena->mutex);
	new_ptr = malloc(size);
	if (new_ptr == NULL)
		return NULL;
	ft_memcpy(new_ptr, ptr, old_needed_chunks * TINY_QUANTUM);
	free(ptr);
	return new_ptr;
}
