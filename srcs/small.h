/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   small.h                                            :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: rotrojan <rotrojan@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/05/12 20:08:29 by rotrojan          #+#    #+#             */
/*   Updated: 2026/06/15 16:07:43 by rotrojan         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef SMALL_H
#define SMALL_H

#include "helpers.h"
#include "malloc_state.h"
#include "zone.h"

#include <stddef.h>

/**
 * Segregated free lists. The eight exact classes hold chunks of exactly
 * 256..2048 bytes (one per SMALL_QUANTUM step); everything larger -- only the
 * initial whole-zone "wild" chunk and transient coalesced runs -- goes in
 * BIN_OVERSIZED. NB_BINS is the array length (it is not a real bin).
 */
typedef enum bin_idx {
	BIN_256,
	BIN_512,
	BIN_768,
	BIN_1024,
	BIN_1280,
	BIN_1536,
	BIN_1792,
	BIN_2048,
	BIN_OVERSIZED,
	NB_BINS
} e_bin_idx;

/**
 * Bin holding chunks of total size `size` (header+footer+payload, a multiple of
 * SMALL_QUANTUM). size/QUANTUM-1 maps 256->0, 512->1, ...; anything past 2048
 * is clamped to BIN_OVERSIZED.
 */
#define BIN_IDX(size) (MIN(((size) / SMALL_QUANTUM) - 1, BIN_OVERSIZED))

/**
 * Free chunks are threaded into singly-linked bins through their payload area
 * (free chunks need no user data), so a bin entry overlays the first bytes of
 * a free chunk. Allocated chunks carry no link.
 */
typedef struct free_list {
	struct free_list *next;
} s_free_list;

/**
 * Boundary tag (CS:APP / "Computer Systems: A Programmer's Perspective"
 * style). Each chunk is bracketed by an identical header and footer tag, each
 * one t_boundary_tag (size_t) wide. The footer of a chunk is what lets its
 * right neighbour find the chunk's size when coalescing leftward.
 *
 * A tag packs size and state into one word: chunk sizes are SMALL_QUANTUM
 * (256) multiples, so the low bits are always zero and free to hold flags. Bit
 * 0 is the in-use flag; the size is the word with the low 3 bits masked off.
 */
typedef size_t t_boundary_tag;
#define TAG_SIZE sizeof(t_boundary_tag)

/* Pack: OR the (8-aligned) size with the in-use bit. */
#define TAG(size, in_use) (t_boundary_tag)((size) | (in_use))
/* Read / write the tag word at address `tag`. */
#define GET_TAG(tag) (*(t_boundary_tag *)(tag))
#define PUT_TAG(tag, val) (*(t_boundary_tag *)(tag) = val)

/* Unpack: size = tag with low 3 bits cleared; state = bit 0 (IN_USE/FREE). */
#define GET_SIZE(tag) (GET_TAG(tag) & ~(0x8 - 1))
#define GET_STATE(tag) (GET_TAG(tag) & 0x1)

/**
 * Navigation from a user pointer `chunk` (which points just past its header):
 *   CHUNK_HDR   the chunk's own header tag, one word before it;
 *   CHUNK_FTR   the chunk's own footer tag, the last word of the chunk;
 *   NEXT_CHUNK  the user pointer of the following chunk (this size away);
 *   PREV_CHUNK  the user pointer of the preceding chunk, reached via that
 *               chunk's footer (the word two tags before us).
 */
#define CHUNK_HDR(chunk) ((char *)(chunk) - TAG_SIZE)
#define CHUNK_FTR(chunk) \
	((char *)(chunk) + GET_SIZE(CHUNK_HDR(chunk)) - 2 * TAG_SIZE)

#define NEXT_CHUNK(chunk) \
	((char *)(chunk) + GET_SIZE(((char *)(chunk) - TAG_SIZE)))
#define PREV_CHUNK(chunk) \
	((char *)(chunk) - GET_SIZE(((char *)(chunk) - 2 * TAG_SIZE)))

#define IN_USE 1
#define FREE 0

/**
 * Size-class step and alignment. A SMALL user pointer is 256-aligned, which
 * free uses as its first cheap validity check.
 */
#define SMALL_QUANTUM 256
/* Zone span in pages: 64 pages == 256 KB. */
#define NB_PAGE_SMALL_ZONE 64
/**
 * Largest payload served by the SMALL class. A max chunk is 8 quanta (2048
 * bytes) total, of which two tags (16 bytes) are overhead, leaving 2032 usable.
 * Requests of 129..2032 bytes route here; above that go to LARGE.
 */
#define SMALL_SIZE_MAX (8 * SMALL_QUANTUM - 2 * TAG_SIZE)
/**
 * 64 pages == 256 KB. After the first SMALL_QUANTUM (zone struct + prologue) a
 * zone fits 127 max-size 2048-byte chunks ((262144 - 256) / 2048 = 127.8),
 * which clears the subject's >= 100 allocations-per-zone requirement.
 */
#define SMALL_ZONE_SIZE (PAGE_SIZE * NB_PAGE_SMALL_ZONE)

typedef struct small_zone {
	s_zone_hdr         zone_hdr; /* must be first: a zone IS its header */
	struct small_zone *prev; /* arena SMALL-list links */
	struct small_zone *next;
	s_free_list       *bin[NB_BINS]; /* per-class free chunk lists */
} s_small_zone;

/**
 * @brief      Allocate `size` bytes (TINY_SIZE_MAX < size <= SMALL_SIZE_MAX).
 * @param size Requested payload size.
 * @return     A 256-aligned pointer, or NULL on out-of-memory.
 */
void *malloc_small(size_t size);

/**
 * @brief          Free a SMALL allocation, coalescing with free neighbours;
 *                 releases the zone if it becomes wholly free.
 * @param ptr      Pointer returned by malloc_small. Misaligned, double-freed,
 *                 or interior/foreign pointers are rejected to stderr.
 * @param zone_hdr The owning zone.
 * @note Assumes the owning arena's mutex is held by the caller.
 */
void free_small(void *ptr, s_zone_hdr *zone_hdr);

/**
 * @brief          Resize a SMALL allocation, in place when possible.
 * @param ptr      Pointer returned by malloc_small.
 * @param size     New requested size.
 * @param zone_hdr The owning zone.
 * @return         ptr if resized in place, a new pointer if it had to move
 *                 (and `ptr` is freed), or NULL on failure.
 */
void *realloc_small(void *ptr, size_t size, s_zone_hdr *zone_hdr);

/**
 * @brief     Reject a pointer not returned by malloc_small.
 *
 * Valid means SMALL_QUANTUM-aligned, its header reads IN_USE, and its header
 * and footer tags agree (the boundary-tag analog of TINY's is_start check).
 * Rejects misaligned, interior, double-freed and foreign pointers -- the
 * shared check behind free_small, realloc_small and malloc_usable_size.
 *
 * @param ptr Candidate pointer, already located in its zone by find_zone.
 * @return    1 if @p ptr is a live chunk start, 0 otherwise.
 * @note Assumes the owning arena's mutex is held by the caller.
 */
int small_ptr_is_valid(void *ptr);

#endif /* SMALL_H */
