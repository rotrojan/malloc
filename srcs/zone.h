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

#define MAGIC 0xdeadbeeff00dbabe

typedef enum zone_type {
	TINY_ZONE,
	SMALL_ZONE,
	LARGE_ZONE,
} e_zone_type;

struct arena;

typedef struct __attribute__((aligned(16))) zone_hdr {
	uint64_t         magic;
	e_zone_type      type;
	size_t           size;
	uintptr_t        self;
	uint64_t         checksum;
	struct arena    *arena;
	struct zone_hdr *prev;
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

void       *new_zone(e_zone_type zone_type, size_t size, struct arena *arena);
void        release_zone(s_zone_hdr *zone_hdr);
s_zone_hdr *find_zone(void *ptr);

#endif /* ZONE_H */
