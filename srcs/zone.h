/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   zone.h                                             :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: rotrojan <rotrojan@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/04/21 22:25:53 by rotrojan          #+#    #+#             */
/*   Updated: 2026/06/23 03:14:33 by rotrojan         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef ZONE_H
#define ZONE_H

#include <stddef.h> /* for size_t */
#include <stdint.h> /* for uint64_t */

/**
 * Sentinel stamped at the start of every zone header. A pointer handed to
 * free()/realloc() is only trusted if the zone it lands in carries this magic
 * (plus a matching self-pointer and checksum). It is the first cheap filter
 * against foreign pointers -- e.g. a block glibc allocated before our
 * LD_PRELOAD hook took effect.
 */
#define MAGIC 0xdeadbeeff00dbabe

typedef enum zone_type {
	TINY_ZONE,
	SMALL_ZONE,
	LARGE_ZONE,
} e_zone_type;

/**
 * Forward declaration only: a zone records its owning arena but never needs the
 * arena's layout here, so zone.h does not include arena.h (which would close an
 * include cycle -- see the note in arena.h).
 */
struct arena;

/**
 * Every mmap'd region begins with this header. The struct is 16-byte aligned so
 * the first usable byte after it is also suitably aligned for any type.
 *
 * Two independent doubly-linked lists thread through these headers:
 *   - prev/next here chain the *global*, address-ordered registry
 *     (g_malloc_state.zone_list), walked by find_zone / show_alloc_mem.
 *   - TINY/SMALL zones are *also* linked into their owning arena via the
 *     prev/next fields of s_tiny_zone / s_small_zone (distinct pointers that
 *     embed this header as their first member).
 * Both links are doubly-linked so a zone already located can be unlinked in
 * O(1) on release instead of re-walking to find its predecessor.
 */
typedef struct __attribute__((aligned(16))) zone_hdr {
	uint64_t         magic; /* == MAGIC on a live zone */
	e_zone_type      type; /* TINY / SMALL / LARGE */
	size_t           size; /* total mmap'd bytes, multiple of PAGE_SIZE */
	uintptr_t        self; /* == (uintptr_t)this header; detects copies */
	uint64_t         checksum; /* XOR canary, see compute_checksum */
	struct arena    *arena; /* owning arena, immutable (NULL for LARGE) */
	struct zone_hdr *prev; /* global registry links (address-ordered) */
	struct zone_hdr *next;
} s_zone_hdr;

/**
 * @brief      Compute a zone header's corruption canary.
 *
 * XOR of the per-zone-immutable identity fields -- magic, type, self, size and
 * the owning arena. The link pointers are deliberately excluded: they change as
 * zones are linked/unlinked, so folding them in would force a checksum rewrite
 * on every list mutation. `zone_is_valid` recomputes this and compares it to
 * the stored `checksum`; a mismatch means the header was overwritten (e.g. a
 * heap overflow from the preceding chunk) and the pointer is rejected.
 *
 * Callers that mutate a covered field (`new_zone`, `realloc_large`'s
 * tail-shrink) must recompute and restore the checksum afterwards, and must set
 * `arena` before the call.
 *
 * @param zone The zone header to checksum.
 * @return     The XOR canary value.
 */
uint64_t compute_checksum(s_zone_hdr *zone);

/**
 * @brief         mmap a new zone, stamp its header and register it.
 *
 * Maps `size` bytes, fills in the header (magic, type, size, self, checksum,
 * arena), inserts it into the address-ordered global registry, and -- for
 * TINY/SMALL -- links it into `arena`'s zone list and marks it hot.
 *
 * @param zone_type TINY_ZONE, SMALL_ZONE or LARGE_ZONE.
 * @param size      Total mapping size in bytes (already a PAGE_SIZE multiple).
 * @param arena     Owning arena for TINY/SMALL; NULL for LARGE.
 * @return          The zone header, or NULL if the mmap failed.
 */
void *new_zone(e_zone_type zone_type, size_t size, struct arena *arena);

/**
 * @brief          Unlink a zone everywhere and munmap it.
 *
 * Pops the zone from the global registry and, for TINY/SMALL, from its arena
 * list, then releases the mapping. The caller must hold the owning arena's
 * mutex for TINY/SMALL (the mutex outlives the zone, so it is safe to hold
 * across the munmap); LARGE zones have no arena and serialize on the global
 * list mutex taken internally.
 *
 * @param zone_hdr The zone to release.
 */
void release_zone(s_zone_hdr *zone_hdr);

/**
 * @brief     Find the registered zone a pointer belongs to.
 *
 * Walks the address-ordered global registry under the list mutex, validating
 * each candidate with zone_is_valid (magic, self, per-type bounds, checksum).
 *
 * @param ptr A pointer previously returned by malloc/realloc.
 * @return    The owning zone header, or NULL if `ptr` belongs to no live zone
 *            (e.g. a foreign or already-freed pointer).
 */
s_zone_hdr *find_zone(void *ptr);

#endif /* ZONE_H */
