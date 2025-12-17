/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   malloc_tiny.c                                      :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: rotrojan <rotrojan@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/12/16 19:57:52 by rotrojan          #+#    #+#             */
/*   Updated: 2025/12/17 02:16:29 by rotrojan         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "malloc_tiny.h"

#include "malloc.h"

#include <stddef.h>
#include <sys/mman.h>
#include <unistd.h>

static struct tiny_zone *new_tiny_zone(void)
{

	struct tiny_zone *new_zone = mmap(NULL,
				       TINY_ZONE_SIZE,
				       PROT_READ | PROT_WRITE,
				       MAP_SHARED | MAP_PRIVATE,
				       0, 0);

	/* We keep the first chunk for metadatas. */
	new_zone->available_chunks = 127;
	new_zone->bitmap[0] = 0;
	new_zone->bitmap[1] = 1;
	new_zone->next = NULL;

	return new_zone;
}

static struct tiny_zone *get_usable_zone(void)
{
	struct tiny_zone *current_zone;

	if (g_zone[TINY] == NULL)
		g_zone[TINY]->tiny = new_tiny_zone();
	current_zone = g_zone[TINY]->tiny;

	while (current_zone->available_chunks == 0)
	{
		if (current_zone->next == NULL)
			current_zone->next = new_tiny_zone();
		current_zone = current_zone->next;
	}

	return current_zone;
}

/* 
 * `__builtin_ctzll()` would have been very handy here, bit I am not sure the
 * assignation allows the use of the compiler builtin fucntions.
 */
static uint8_t get_first_free_chunk_idx(struct tiny_zone *zone)
{
	uint8_t idx = 0;
	uint64_t *bitmap =zone->bitmap;

	if (*bitmap == UINT64_MAX)
		++bitmap;

	while (idx < sizeof(*bitmap))
	{
		if (~(*bitmap) & (1 << idx))
			return idx;
		++idx;
	}

	return idx;
}

void *malloc_tiny(void)
{
	struct tiny_zone *current_zone = get_usable_zone();
	uint8_t first_free_chunk_idx = get_first_free_chunk_idx(current_zone);

	return current_zone + first_free_chunk_idx * TINY_SIZE_MAX;
}
