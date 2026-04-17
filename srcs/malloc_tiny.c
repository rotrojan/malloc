/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   malloc_tiny.c                                      :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: rotrojan <rotrojan@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/12/16 19:57:52 by rotrojan          #+#    #+#             */
<<<<<<< HEAD
/*   Updated: 2025/12/17 02:16:29 by rotrojan         ###   ########.fr       */
=======
/*   Updated: 2025/12/30 18:36:43 by rotrojan         ###   ########.fr       */
>>>>>>> ff13cb3 (Dirty push to transfer from larchtop to 42.)
/*                                                                            */
/* ************************************************************************** */

#include "malloc_tiny.h"

#include "_malloc.h"
#include "bitmap.h"

#include <stddef.h>
#include <sys/mman.h>
#include <unistd.h>

static struct tiny_zone *new_tiny_zone(void)
{
	s_tiny_zone *new_zone = (struct tiny_zone *)mmap(
		NULL, TINY_ZONE_SIZE, PROT_READ | PROT_WRITE,
		MAP_SHARED | MAP_ANONYMOUS, -1, 0);

	/* We keep the first chunk for metadatas. */
	new_zone->available_chunks = 127;
	new_zone->bitmap[0]        = 1;
	new_zone->bitmap[1]        = 0;
	new_zone->next             = NULL;

	return new_zone;
}

static struct tiny_zone *get_usable_zone(void)
{
	s_tiny_zone *current_zone;

	if (g_zone.tiny == NULL)
		g_zone.tiny = new_tiny_zone();
	current_zone = g_zone.tiny;

	while (current_zone->available_chunks == 0) {
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
static uint8_t get_index_first_free_chunk(s_tiny_zone *zone)
{
	unsigned int index = 0;

	while (index < BITS_PER_WORD && bitmap_is_set(zone->bitmap, index))
		++index;

	return index;
}

void *malloc_tiny(void)
{
	s_tiny_zone *current_zone = get_usable_zone();
	unsigned int index        = get_index_first_free_chunk(current_zone);

	bitmap_set(current_zone->bitmap, index);
	--current_zone->available_chunks;

	return (void *)current_zone + index * TINY_CHUNK_SIZE;
}
