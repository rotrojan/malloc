/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   malloc_small.c                                     :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: rotrojan <rotrojan@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/05/13 12:29:46 by rotrojan          #+#    #+#             */
/*   Updated: 2026/06/10 20:32:04 by rotrojan         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "malloc_small.h"

#include "libft.h"
#include "magazine.h"
#include "zone.h"

#include <stddef.h>
#include <unistd.h>

static s_small_zone *new_small_zone(void)
{
	s_small_zone *zone;
	char         *prologue;
	char         *epilogue;
	char         *wild_chunk;

	zone = new_zone(SMALL_ZONE, SMALL_ZONE_SIZE);
	if (zone == NULL)
		return NULL;

	prologue   = (char *)zone + SMALL_QUANTUM - 2 * TAG_SIZE;
	epilogue   = (char *)zone + SMALL_ZONE_SIZE;
	wild_chunk = (char *)zone + SMALL_QUANTUM;

	ft_memset(zone->bin, 0, sizeof(zone->bin));

	/**
	 * The start of the heap is shifted to take account for the prologue
	 * chunk (e.g one header and one footer) and the epilogue (one footer).
	 */

	/* prologue */
	PUT_TAG(CHUNK_HDR(prologue), TAG(2 * TAG_SIZE, IN_USE));
	PUT_TAG(CHUNK_FTR(prologue), TAG(2 * TAG_SIZE, IN_USE));
	/* epilogue header (epilogue has no footer)*/
	PUT_TAG(CHUNK_HDR(epilogue), TAG(0, IN_USE));
	/* first free chunk (wild chunk)*/
	PUT_TAG(CHUNK_HDR(wild_chunk),
		TAG(SMALL_ZONE_SIZE - SMALL_QUANTUM, FREE));
	PUT_TAG(CHUNK_FTR(wild_chunk),
		TAG(SMALL_ZONE_SIZE - SMALL_QUANTUM, FREE));

	zone->bin[BIN_OVERSIZED] = (s_free_list *)wild_chunk;

	return zone;
}

static int zone_has_chunk(size_t size, s_small_zone *zone)
{
	for (int i = BIN_IDX(size); i < NB_BINS; i++)
		if (zone->bin[i])
			return 1;

	return 0;
}

static s_small_zone *get_small_zone(size_t size)
{
	s_magazine   *mag;
	s_small_zone *zone;

	mag = get_magazine();
	if (mag == NULL)
		return NULL;
	zone = mag->small_hot;
	if (zone == NULL)
		goto new_zone;

	/* Look into hot zone first. */
	if (zone_has_chunk(size, zone))
		return zone;

	/**
	 * If hot zone does not have enough space, try other zones in the list.
	 */
	for (s_small_zone *current = mag->small_list; current != NULL;
	     current               = current->next) {
		if (zone_has_chunk(size, current))
			return current;
	}

new_zone:
	zone = new_small_zone();
	if (zone == NULL)
		return NULL;

	return zone;
}

static void remove_from_bin(s_free_list *chunk, s_free_list **bins)
{
	size_t        size    = GET_SIZE(CHUNK_HDR(chunk));
	s_free_list **bin     = &bins[BIN_IDX(size)];
	s_free_list **current = bin;

	while (*current != NULL) {
		if (*current == chunk)
			break;
		current = &(*current)->next;
	}

	if (*current == NULL)
		return ft_dprintf(STDERR_FILENO,
				  "Fatal: cannot coalesce chunks!\n");

	*current = ((*current)->next);
}

void *coalesce(void *chunk, s_free_list **bins)
{
	int    prev_state = GET_STATE(CHUNK_HDR(PREV_CHUNK(chunk)));
	int    next_state = GET_STATE(CHUNK_HDR(NEXT_CHUNK(chunk)));
	size_t size;

	if (prev_state == IN_USE && next_state == IN_USE)
		return chunk;

	if (prev_state == IN_USE && next_state == FREE) {
		remove_from_bin((s_free_list *)NEXT_CHUNK(chunk), bins);
		size = GET_SIZE(CHUNK_HDR(chunk)) +
		       GET_SIZE(CHUNK_HDR(NEXT_CHUNK(chunk)));
		PUT_TAG(CHUNK_HDR(chunk), TAG(size, FREE));
		PUT_TAG(CHUNK_FTR(chunk), TAG(size, FREE));
	} else if (prev_state == FREE && next_state == IN_USE) {
		remove_from_bin((s_free_list *)PREV_CHUNK(chunk), bins);
		size = GET_SIZE(CHUNK_HDR(chunk)) +
		       GET_SIZE(CHUNK_HDR(PREV_CHUNK(chunk)));
		PUT_TAG(CHUNK_FTR(chunk), TAG(size, FREE));
		PUT_TAG(CHUNK_HDR(PREV_CHUNK(chunk)), TAG(size, FREE));
		chunk = PREV_CHUNK(chunk);
	} else /* if(prev_state == FREE && next_state == FREE) */ {
		remove_from_bin((s_free_list *)NEXT_CHUNK(chunk), bins);
		remove_from_bin((s_free_list *)PREV_CHUNK(chunk), bins);
		size = GET_SIZE(CHUNK_HDR(chunk)) +
		       GET_SIZE(CHUNK_HDR(PREV_CHUNK(chunk))) +
		       GET_SIZE(CHUNK_HDR(NEXT_CHUNK(chunk)));
		PUT_TAG(CHUNK_HDR(PREV_CHUNK(chunk)), TAG(size, FREE));
		PUT_TAG(CHUNK_FTR(NEXT_CHUNK(chunk)), TAG(size, FREE));
		chunk = PREV_CHUNK(chunk);
	}

	return chunk;
}

s_free_list *add_to_bin(s_free_list *chunk, s_free_list **free_list)
{
	chunk->next = *free_list;
	*free_list  = chunk;

	return chunk;
}

s_free_list *pop_from_bin(s_free_list **free_list)
{
	s_free_list *chunk = *free_list;

	*free_list = (*free_list)->next;

	return chunk;
}

static void *split_chunk(s_free_list *chunk, size_t size, s_free_list **bin)
{
	size_t prev_size = GET_SIZE(CHUNK_HDR(chunk));
	size_t remainder_size;

	PUT_TAG(CHUNK_HDR(chunk), TAG(size, IN_USE));
	PUT_TAG(CHUNK_FTR(chunk), TAG(size, IN_USE));

	remainder_size = prev_size - size;
	PUT_TAG(CHUNK_HDR(NEXT_CHUNK(chunk)), TAG(remainder_size, FREE));
	PUT_TAG(CHUNK_FTR(NEXT_CHUNK(chunk)), TAG(remainder_size, FREE));

	add_to_bin((s_free_list *)NEXT_CHUNK(chunk),
		   &bin[BIN_IDX(remainder_size)]);

	return chunk;
}

void *malloc_small(size_t size)
{
	size_t        real_size;
	e_bin_idx     bin_idx;
	s_small_zone *zone;
	s_free_list **bin;

	/**
	 * Take account of boundary tags and round up to the upper SMALL_QUANTUM
	 * multiple.
	 * */
	real_size = (size + 2 * TAG_SIZE + SMALL_QUANTUM - 1) &
		    ~(SMALL_QUANTUM - 1);

	zone = get_small_zone(real_size);
	if (zone == NULL)
		return NULL;

	bin     = zone->bin;
	bin_idx = BIN_IDX(real_size);

	/** If the matching bin has an available chunk, pop it, mark it as
	 * IN_USE, and return it.
	 */
	if (bin[bin_idx] != NULL) {
		PUT_TAG(CHUNK_HDR(bin[bin_idx]), TAG(real_size, IN_USE));
		PUT_TAG(CHUNK_FTR(bin[bin_idx]), TAG(real_size, IN_USE));
		return pop_from_bin(&bin[bin_idx]);
	}

	/**
	 * Otherwise, look into the bigger bins. When the chunk is found, pop
	 * it, split it, mark the desired part as IN_USE, return it and put the
	 * remaining chunk in the proper bin.
	 */
	bin_idx++;
	while (bin_idx < NB_BINS) {
		if (bin[bin_idx])
			return split_chunk(pop_from_bin(&bin[bin_idx]),
					   real_size, bin);
		bin_idx++;
	}

	return NULL;
}
