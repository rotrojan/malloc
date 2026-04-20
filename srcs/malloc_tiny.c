/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   malloc_tiny.c                                      :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: rotrojan <rotrojan@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/12/16 19:57:52 by rotrojan          #+#    #+#             */
/*   Updated: 2026/04/20 15:03:00 by rotrojan         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "malloc_tiny.h"

#include "bitmap.h"
#include "magazine.h"
#include "zone_type.h"

#include "helpers.h"
#include "libft.h"

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

static struct tiny_zone *new_tiny_zone(void)
{
	s_tiny_zone *new_zone;

	new_zone = (struct tiny_zone *)mmap(NULL, TINY_ZONE_SIZE,
					    PROT_READ | PROT_WRITE,
					    MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	if (new_zone == NULL)
		return NULL;

	ft_memset(new_zone, 0, sizeof(*new_zone));
	new_zone->zone_type             = TINY_ZONE;
	new_zone->index_next_free_chunk = NB_CHUNKS_TINY_HDR;

	add_zone_to_magazine(new_zone);

	return new_zone;
}

s_tiny_zone *get_tiny_zone()
{
	s_magazine  *mag  = NULL;
	s_tiny_zone *zone = NULL;

	mag = get_magazine();

	if (mag->tiny_hot == NULL) {
		zone          = new_tiny_zone();
		mag->tiny_hot = zone;
		/* push_zone(mag->tiny_list); */
	}

	return mag->tiny_hot;
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
