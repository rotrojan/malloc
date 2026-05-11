/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   free.c                                             :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: rotrojan <rotrojan@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/04/21 17:37:42 by rotrojan          #+#    #+#             */
/*   Updated: 2026/05/11 14:20:16 by rotrojan         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "free.h"
#include "bitmap.h"
#include "helpers.h"
#include "libft.h"
#include "malloc_state.h"
#include "malloc_tiny.h"
#include "zone.h"

#include <unistd.h>

/*TODO: adapt nb_page*/
#define NB_PAGE_SMALL_ZONE 4

static int zone_is_valid(void *ptr, s_zone_hdr *zone, e_zone_type zone_type)
{
	uintptr_t ptr_int  = (uintptr_t)ptr;
	uintptr_t zone_int = (uintptr_t)zone;

	if (zone->magic != MAGIC)
		return 0;

	if (zone->self != zone_int)
		return 0;

	if (zone->type != zone_type)
		return 0;

	if (zone_type == TINY_ZONE) {
		if (zone->size != TINY_ZONE_SIZE)
			return 0;
		if (ptr_int < zone_int + NB_CHUNKS_TINY_HDR * TINY_SIZE_MIN ||
		    ptr_int >= zone_int + zone->size)
			return 0;
	}
	/* else if (zone_type == SMALL_ZONE) { */
	/* 	if (zone->size != SMALL_ZONE_SIZE) */
	/* 		return 0; */
	/*  } */

	if (zone->checksum != compute_checksum(zone))
		return 0;

	return 1;
}

s_zone_hdr *find_zone(void *ptr)
{
	s_zone_hdr *page;

	/* Get the address of the page. */
	page = (s_zone_hdr *)((uintptr_t)ptr & ~(PAGE_SIZE - 1));
	/* Check if ptr belongs to a LARGE zone. */

	if (zone_is_valid(ptr, page, LARGE_ZONE))
		return page;

	/* Check if ptr belongs to a TINY or a SMALL zone. */
	for (int i = 0; i < NB_PAGE_SMALL_ZONE; i++) {
		if (zone_is_valid(ptr, page, TINY_ZONE))
			return page;
		page = (s_zone_hdr *)((uintptr_t)page - PAGE_SIZE);
	}

	return NULL;
}

void free_tiny(void *ptr, s_zone_hdr *zone_hdr)
{
	uintptr_t    ptr_int  = (uintptr_t)ptr;
	s_tiny_zone *zone     = (s_tiny_zone *)zone_hdr;
	uintptr_t    zone_int = (uintptr_t)zone_hdr;
	size_t       index;
	size_t       size;

	if (ptr_int & (TINY_SIZE_MIN - 1))
		return ft_dprintf(STDERR_FILENO,
				  "Fatal: invalid pointer passed to free!\n"
				  "%p: pointer is not alligned.\n",
				  ptr);

	index = (ptr_int - zone_int) / TINY_SIZE_MIN;

	if (!bitmap_get_bit(zone->in_use, index) ||
	    !bitmap_get_bit(zone->is_start, index))
		return ft_dprintf(STDERR_FILENO,
				  "Fatal: invalid pointer passed to free!\n"
				  "%p: pointer was never malloced.\n",
				  ptr);

	size = 1;
	while (size < TINY_SIZE_MAX / TINY_SIZE_MIN &&
	       index + size < BIT_ARRAY_SIZE(zone->in_use) &&
	       bitmap_get_bit(zone->in_use, index + size) &&
	       !bitmap_get_bit(zone->is_start, index + size))
		size++;

	bitmap_clear_range(zone->in_use, index, size);
	bitmap_clear_bit(zone->is_start, index);

	zone->index_next_free_chunk = MIN(zone->index_next_free_chunk, index);

	/* Check if zone is empty. If so, release it. */
	for (size_t i = 0; i < ARRAY_SIZE(zone->in_use); i++) {
		if (zone->in_use[i])
			return;
	}
	release_zone(zone_hdr);
}
