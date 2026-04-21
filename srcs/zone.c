/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   zone.c                                             :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: rotrojan <rotrojan@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/04/21 15:26:13 by rotrojan          #+#    #+#             */
/*   Updated: 2026/04/21 16:36:34 by rotrojan         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "zone.h"

#include <sys/mman.h>
#include <unistd.h>

static inline uint64_t compute_checksum(s_zone_hdr *zone)
{
	return zone->magic ^ zone->type ^ zone->self ^ zone->size;
}

void *new_zone(e_zone_type zone_type, size_t size)
{
	s_zone_hdr *new_zone;

	new_zone = (s_zone_hdr *)mmap(NULL, size, PROT_READ | PROT_WRITE,
				      MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	if (new_zone == NULL)
		return NULL;

	new_zone->magic    = MAGIC;
	new_zone->type     = zone_type;
	new_zone->size     = size;
	new_zone->self     = (uintptr_t)new_zone;
	new_zone->checksum = compute_checksum(new_zone);

	return new_zone;
}
