/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   show_alloc_mem.c                                   :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: rotrojan <rotrojan@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/04/22 14:18:10 by rotrojan          #+#    #+#             */
/*   Updated: 2026/04/30 14:37:56 by rotrojan         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "show_alloc_mem.h"

#include "bitmap.h"
#include "helpers.h"
#include "libft.h"
#include "malloc_state.h"
#include "malloc_tiny.h"

#include <stddef.h>

static void print_tiny_zone(s_tiny_zone *zone, ptrdiff_t *total_alloc)
{
	size_t    i = NB_CHUNKS_TINY_HDR;
	size_t    j;
	uintptr_t zone_addr = (uintptr_t)zone;
	uintptr_t start_alloc;
	uintptr_t end_alloc;
	ptrdiff_t size_alloc;

	ft_printf("TINY: %p\n", zone_addr);

	while (i < BIT_ARRAY_SIZE(zone->is_start)) {
		if (!bitmap_get_bit(zone->is_start, i)) {
			i++;
			continue;
		}

		j = 1;
		while (i + j < BIT_ARRAY_SIZE(zone->in_use) &&
		       j <= TINY_SIZE_MAX / TINY_SIZE_MIN &&
		       !bitmap_get_bit(zone->is_start, i + j) &&
		       bitmap_get_bit(zone->in_use, i + j))
			j++;
		start_alloc = zone_addr + i * TINY_SIZE_MIN;
		end_alloc   = zone_addr + (i + j) * TINY_SIZE_MIN - 1;
		size_alloc  = end_alloc - start_alloc + 1;
		/* TODO: handle end_alloc - start alloc > INTMAX */
		ft_printf("%p - %p : %d bytes\n", start_alloc, end_alloc,
			  size_alloc);
		*total_alloc += size_alloc;
		i += j;
	}
}

void show_alloc_mem(void)
{
	s_zone_hdr *current     = g_malloc_state.zone_list;
	ptrdiff_t   total_alloc = 0;

	if (current == NULL)
		return;

	while (current != NULL) {
		switch (current->type) {
		case TINY_ZONE:
			print_tiny_zone((s_tiny_zone *)current, &total_alloc);
			break;
		default:
			return;
		}
		current = current->next;
	}

	ft_printf("Total : %d bytes\n", total_alloc);
}
