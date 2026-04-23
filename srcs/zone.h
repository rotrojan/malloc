/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   zone.h                                             :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: rotrojan <rotrojan@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/04/21 22:25:53 by rotrojan          #+#    #+#             */
/*   Updated: 2026/04/23 11:35:48 by rotrojan         ###   ########.fr       */
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

typedef struct zone_hdr {
	uint64_t         magic;
	e_zone_type      type;
	size_t           size;
	uintptr_t        self;
	uint64_t         checksum;
	struct zone_hdr *next;
} s_zone_hdr;

static inline uint64_t compute_checksum(s_zone_hdr *zone)
{
	return zone->magic ^ zone->type ^ zone->self ^ zone->size;
}

void *new_zone(e_zone_type zone_type, size_t size);
void  release_zone(s_zone_hdr *zone_hdr);

#endif /* ZONE_H */
