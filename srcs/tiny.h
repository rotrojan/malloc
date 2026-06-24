/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   tiny.h                                             :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: rotrojan <rotrojan@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/12/16 19:52:54 by rotrojan          #+#    #+#             */
/*   Updated: 2026/06/22 21:02:12 by rotrojan         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef TINY_H
#define TINY_H

#include "helpers.h"
#include "malloc_state.h"
#include "zone.h"

#include <stddef.h> /* For size_t */
#include <stdint.h>

/**
 * Largest request served by the TINY class; above this malloc routes to SMALL.
 * 128 / 16 = 8 chunks, so any TINY allocation spans at most 8 quanta.
 */
#define TINY_SIZE_MAX 128
/**
 * Allocation granularity: every TINY request rounds up to a multiple of this,
 * and the 16-byte value also satisfies the required 16-byte user alignment.
 */
#define TINY_QUANTUM 16
/**
 * Number of leading 16-byte chunks the zone header (s_tiny_zone) occupies;
 * those chunks are never handed out. DIV_CEIL rounds the header up to a whole
 * chunk so allocations start cleanly aligned.
 */
#define NB_CHUNKS_TINY_HDR DIV_CEIL(sizeof(s_tiny_zone), TINY_QUANTUM)

/**
 * Four pages (16 KB where the page is 4 KB). The subject requires a zone to fit
 * at least 100 allocations at the class's max size: 16384 / 128 = 128 max-size
 * chunks, comfortably above 100 even after the header is deducted (~1002 of the
 * smallest 16-byte chunks remain usable).
 */
#define TINY_ZONE_SIZE (PAGE_SIZE * 4)

typedef struct tiny_zone {
	s_zone_hdr        zone_hdr; /* must be first: a zone IS its header */
	struct tiny_zone *prev; /* arena TINY-list links (doubly-linked) */
	struct tiny_zone *next;
	/**
	 * Two parallel bitmaps describe the zone's 1024 16-byte chunks (1024
	 * bits == 16 uint64_t words):
	 *   in_use[i]   == 1  the chunk is allocated;
	 *   is_start[i] == 1  the chunk begins an allocation.
	 * The pair lets free locate an allocation's extent without storing a
	 * size: scan in_use forward from the is_start bit until the next
	 * is_start (or a free chunk). A lone is_start check also rejects
	 * interior pointers in free.
	 */
	uint64_t in_use[16];
	uint64_t is_start[16];
	/**
	 * First-fit lower-bound hint: no free chunk exists below this index, so
	 * the bitmap search starts here instead of at 0.
	 */
	size_t index_next_free_chunk;
} s_tiny_zone;

/**
 * @brief      Allocate `size` bytes (<= TINY_SIZE_MAX) from a TINY zone.
 * @param size Requested size; 0 still yields one chunk (a freeable pointer).
 * @return     A 16-aligned pointer, or NULL on out-of-memory.
 */
void *malloc_tiny(size_t size);

/**
 * @brief          Free a TINY allocation; releases the zone if it empties.
 * @param ptr      Pointer returned by malloc_tiny. Misaligned, interior, or
 *                 never-allocated pointers are rejected to stderr without
 * abort.
 * @param zone_hdr The owning zone (already located by find_zone).
 * @note Assumes the owning arena's mutex is held by the caller.
 */
void free_tiny(void *ptr, s_zone_hdr *zone_hdr);

/**
 * @brief          Resize a TINY allocation, in place when possible.
 * @param ptr      Pointer returned by malloc_tiny.
 * @param size     New requested size.
 * @param zone_hdr The owning zone.
 * @return         ptr if resized in place, a new pointer if it had to move
 *                 (and `ptr` is freed), or NULL on failure.
 */
void *realloc_tiny(void *ptr, size_t size, s_zone_hdr *zone_hdr);

/**
 * @brief      Reject a pointer not returned by malloc_tiny.
 *
 * Valid means TINY_QUANTUM-aligned and pointing at an allocation's first chunk
 * (its bit is set in both in_use and is_start). Rejects misaligned, interior
 * and double-freed pointers -- the shared check behind free_tiny, realloc_tiny
 * and malloc_usable_size.
 *
 * @param ptr  Candidate pointer, already located in @p zone by find_zone.
 * @param zone The owning TINY zone.
 * @return     1 if @p ptr is a live allocation start, 0 otherwise.
 * @note Assumes the owning arena's mutex is held by the caller.
 */
int tiny_ptr_is_valid(void *ptr, s_tiny_zone *zone);

/**
 * @brief      Count the 16-byte chunks the allocation at `ptr` spans (1..8).
 *
 * Walks the in_use bitmap from `ptr`'s chunk, stopping at the next is_start, a
 * free chunk, the class maximum, or the bitmap end. Used by free_tiny,
 * realloc_tiny (the old extent) and malloc_usable_size.
 *
 * @param ptr  A pointer returned by malloc_tiny (an allocation's first chunk).
 * @param zone The owning TINY zone.
 * @return     The chunk count (1..8).
 */
size_t get_nb_chunks_tiny_alloc(char *ptr, s_tiny_zone *zone);

#endif /* TINY_H */
