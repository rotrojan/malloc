/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   malloc_tiny.c                                      :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: rotrojan <rotrojan@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/12/16 19:57:52 by rotrojan          #+#    #+#             */
/*   Updated: 2026/04/21 17:04:27 by rotrojan         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "malloc_tiny.h"

#include "bitmap.h"
#include "helpers.h"
#include "libft.h"
#include "magazine.h"
#include "zone.h"

#include <stddef.h>
#include <stdint.h>
#include <sys/mman.h>
#include <unistd.h>

static s_tiny_zone *add_zone_to_magazine(s_tiny_zone *zone)
{
	s_magazine *mag = get_magazine();

	zone->next     = mag->tiny_list;
	mag->tiny_list = zone;

	mag->tiny_hot = zone;

	return zone;
}

static s_tiny_zone *new_tiny_zone()
{
	s_tiny_zone *zone = new_zone(TINY_ZONE, TINY_ZONE_SIZE);

	add_zone_to_magazine(zone);

	ft_memset(zone->in_use, 0, ARRAY_SIZE(zone->in_use));
	ft_memset(zone->is_start, 0, ARRAY_SIZE(zone->is_start));
	zone->index_next_free_chunk = NB_CHUNKS_TINY_HDR;

	return zone;
}

size_t try_zone(size_t needed_chunks, s_tiny_zone *zone)
{
	return bitmap_find_consecutive_zeros(zone->in_use,
					     ARRAY_SIZE(zone->in_use),
					     needed_chunks,
					     zone->index_next_free_chunk);
}

void *malloc_tiny(size_t size)
{
	size_t       needed_chunks;
	size_t       index;
	s_tiny_zone *zone;
	s_magazine  *mag;

	/**
	 * If `size == 0`, we still allocate one chunk.
	 * From `man 3 malloc`: "If size is 0, then malloc() returns a unique
	 * pointer value that can later be successfully passed to free()".
	 */
	needed_chunks = MAX(1, DIV_CEIL(size, TINY_SIZE_MIN));

	mag  = get_magazine();
	zone = mag->tiny_hot;
	if (zone == NULL)
		goto new_zone;

	/* Try to allocate in hot zone first. */
	index = try_zone(needed_chunks, zone);
	if (index != SIZE_MAX)
		goto out;

	/**
	 * If hot zone does not have enough space, try other zones in the list.
	 */
	for (s_tiny_zone *current = mag->tiny_list; current != NULL;
	     current              = current->next) {
		if (current == zone)
			continue;
		if (try_zone(needed_chunks, current) != SIZE_MAX) {
			mag->tiny_hot = current;
			goto out;
		}
	}

new_zone:
	/* If no zone has enough space, mmap() a new one. */
	zone = new_tiny_zone();
	if (zone == NULL)
		return NULL;
	index = zone->index_next_free_chunk;

out:
	bitmap_set_range(zone->in_use, index, needed_chunks);
	bitmap_set_range(zone->is_start, index, 1);
	if (index == zone->index_next_free_chunk)
		zone->index_next_free_chunk += needed_chunks;

	return (void *)zone + index * TINY_SIZE_MIN;
}
