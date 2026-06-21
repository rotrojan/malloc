/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   show_alloc_mem.c                                   :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: rotrojan <rotrojan@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/04/22 14:18:10 by rotrojan          #+#    #+#             */
/*   Updated: 2026/06/17 23:13:55 by rotrojan         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "show_alloc_mem.h"

#include "bitmap.h"
#include "helpers.h"
#include "libft.h"
#include "malloc_state.h"
#include "small.h"
#include "tiny.h"

#include <pthread.h>
#include <stddef.h>

static void print_large_zone(s_zone_hdr *zone, size_t *total_alloc)
{
	uintptr_t zone_addr = (uintptr_t)zone;
	uintptr_t start_alloc;
	uintptr_t end_alloc;
	size_t    size_alloc;

	ft_printf("LARGE : %p\n", (void *)zone_addr);

	start_alloc = zone_addr + sizeof(s_zone_hdr);
	end_alloc   = zone_addr + zone->size - 1;
	size_alloc  = end_alloc - start_alloc + 1;
	ft_printf("%p - %p : %zu bytes\n", (void *)start_alloc,
		  (void *)end_alloc, size_alloc);

	*total_alloc += size_alloc;
}

static void print_small_zone(s_small_zone *zone, size_t *total_alloc)
{
	uintptr_t zone_addr = (uintptr_t)zone;
	size_t    size_alloc;
	uintptr_t ptr = zone_addr + SMALL_QUANTUM;

	ft_printf("SMALL : %p\n", (void *)zone_addr);

	while (ptr < zone_addr + SMALL_ZONE_SIZE) {
		if (GET_STATE(CHUNK_HDR((char *)ptr)) == IN_USE) {
			size_alloc = GET_SIZE(CHUNK_HDR((char *)ptr));
			ft_printf("%p - %p : %zu bytes\n", (void *)ptr,
				  (void *)(ptr + size_alloc - 1), size_alloc);
			*total_alloc += size_alloc;
		}
		ptr = (uintptr_t)NEXT_CHUNK((char *)ptr);
	}
}

static void print_tiny_zone(s_tiny_zone *zone, size_t *total_alloc)
{
	size_t    i = NB_CHUNKS_TINY_HDR;
	size_t    j;
	uintptr_t zone_addr = (uintptr_t)zone;
	uintptr_t start_alloc;
	uintptr_t end_alloc;
	size_t    size_alloc;

	ft_printf("TINY : %p\n", (void *)zone_addr);

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
		ft_printf("%p - %p : %zu bytes\n", (void *)start_alloc,
			  (void *)end_alloc, size_alloc);
		*total_alloc += size_alloc;
		i += j;
	}
}

void show_alloc_mem(void)
{
	s_zone_hdr *current;
	size_t      total_alloc = 0;

	pthread_mutex_lock(&g_malloc_state.list_mutex);
	current = g_malloc_state.zone_list;
	if (current == NULL) {
		pthread_mutex_unlock(&g_malloc_state.list_mutex);
		return;
	}

	while (current != NULL) {
		switch (current->type) {
		case TINY_ZONE:
			print_tiny_zone((s_tiny_zone *)current, &total_alloc);
			break;
		case SMALL_ZONE:
			print_small_zone((s_small_zone *)current, &total_alloc);
			break;
		default: /*LARGE_ZONE*/
			print_large_zone((s_zone_hdr *)current, &total_alloc);
			break;
		}
		current = current->next;
	}
	pthread_mutex_unlock(&g_malloc_state.list_mutex);

	ft_printf("Total : %zu bytes\n", total_alloc);
}
