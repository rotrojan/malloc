/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   malloc_tiny.c                                      :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: rotrojan <rotrojan@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/12/16 19:57:52 by rotrojan          #+#    #+#             */
/*   Updated: 2026/05/11 13:24:44 by rotrojan         ###   ########.fr       */
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

#define NO_USABLE_INDEX SIZE_MAX

static s_tiny_zone *new_tiny_zone(void)
{
	s_tiny_zone *zone = new_zone(TINY_ZONE, TINY_ZONE_SIZE);

	if (zone == NULL)
		return NULL;

	ft_memset(zone->in_use, 0, sizeof(zone->in_use));
	ft_memset(zone->is_start, 0, sizeof(zone->is_start));
	zone->index_next_free_chunk = NB_CHUNKS_TINY_HDR;

	return zone;
}

static size_t get_usable_index(size_t needed_chunks, s_tiny_zone *zone)
{
	return bitmap_find_consecutive_zeros(zone->in_use,
					     ARRAY_SIZE(zone->in_use),
					     needed_chunks,
					     zone->index_next_free_chunk);
}

static s_tiny_zone *get_tiny_zone(size_t needed_chunks, size_t *index)
{
	s_magazine  *mag;
	s_tiny_zone *zone;

	mag = get_magazine();
	if (mag == NULL)
		return NULL;
	zone = mag->tiny_hot;
	if (zone == NULL)
		goto new_zone;

	/* Look into hot zone first. */
	*index = get_usable_index(needed_chunks, zone);
	if (*index != NO_USABLE_INDEX)
		return zone;

	/**
	 * If hot zone does not have enough space, try other zones in the list.
	 */
	for (s_tiny_zone *current = mag->tiny_list; current != NULL;
	     current              = current->next) {
		if (current == zone)
			continue;
		*index = get_usable_index(needed_chunks, current);
		if (*index != NO_USABLE_INDEX) {
			mag->tiny_hot = current;
			return current;
		}
	}

new_zone:
	/* If no zone has enough space, or if it is the first call to
	 * malloc_tiny, `mmap()` a new zone. */
	zone = new_tiny_zone();
	if (zone == NULL)
		return NULL;
	*index = zone->index_next_free_chunk;
	return zone;
}

void *malloc_tiny(size_t size)
{
	size_t       needed_chunks;
	size_t       index;
	s_tiny_zone *zone;

	/**
	 * If `size == 0`, we still allocate one chunk.
	 * From `man 3 malloc`: "If size is 0, then malloc() returns a unique
	 * pointer value that can later be successfully passed to free()".
	 */
	needed_chunks = MAX(1, DIV_CEIL(size, TINY_SIZE_MIN));
	zone          = get_tiny_zone(needed_chunks, &index);
	if (zone == NULL)
		return NULL;

	bitmap_set_range(zone->in_use, index, needed_chunks);
	bitmap_set_bit(zone->is_start, index);
	zone->index_next_free_chunk = MIN(zone->index_next_free_chunk, index);

	return (void *)((uintptr_t)zone + index * TINY_SIZE_MIN);
}
