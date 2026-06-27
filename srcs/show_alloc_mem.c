/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   show_alloc_mem.c                                   :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: rotrojan <rotrojan@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/04/22 14:18:10 by rotrojan          #+#    #+#             */
/*   Updated: 2026/06/23 15:13:32 by rotrojan         ###   ########.fr       */
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

/**
 * Print the single allocation filling a LARGE zone: the payload runs from just
 * past the header to the end of the mapping. Accumulate its size into the
 * running total (subject format: "<start> - <end> : <n> bytes").
 */
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

/**
 * Walk a SMALL zone chunk by chunk using the boundary tags (NEXT_CHUNK follows
 * each header's size field), printing only the IN_USE chunks. The walk starts
 * at the first real chunk (past the prologue quantum) and stops at the
 * epilogue. Reported size is the chunk's total tagged size, matching the
 * subject's "allocated memory zone" reading. The zone header line is printed
 * lazily, before the first IN_USE chunk, so a retained empty zone (see arena.h)
 * prints nothing.
 */
static void print_small_zone(s_small_zone *zone, size_t *total_alloc)
{
	uintptr_t zone_addr = (uintptr_t)zone;
	size_t    size_alloc;
	uintptr_t ptr            = zone_addr + SMALL_QUANTUM;
	int       header_printed = 0;

	while (ptr < zone_addr + SMALL_ZONE_SIZE) {
		if (GET_STATE(CHUNK_HDR((char *)ptr)) == IN_USE) {
			if (!header_printed) {
				ft_printf("SMALL : %p\n", (void *)zone_addr);
				header_printed = 1;
			}
			size_alloc = GET_SIZE(CHUNK_HDR((char *)ptr));
			ft_printf("%p - %p : %zu bytes\n", (void *)ptr,
				  (void *)(ptr + size_alloc - 1), size_alloc);
			*total_alloc += size_alloc;
		}
		ptr = (uintptr_t)NEXT_CHUNK((char *)ptr);
	}
}

/**
 * Walk a TINY zone's bitmaps and print each allocation. Outer loop scans for an
 * is_start bit (the first chunk of an allocation), skipping past the header
 * chunks and any free gaps. The inner loop then measures the allocation's span
 * `j` by counting consecutive in_use chunks up to (but not including) the next
 * is_start, the 8-chunk class maximum, or the bitmap end -- the same extent
 * rule free_tiny uses. The printed range covers j whole 16-byte chunks. The
 * zone header line is printed lazily, before the first allocation, so a
 * retained empty zone (see arena.h) prints nothing.
 */
static void print_tiny_zone(s_tiny_zone *zone, size_t *total_alloc)
{
	size_t    i = NB_CHUNKS_TINY_HDR;
	size_t    j;
	uintptr_t zone_addr = (uintptr_t)zone;
	uintptr_t start_alloc;
	uintptr_t end_alloc;
	size_t    size_alloc;
	int       header_printed = 0;

	while (i < BIT_ARRAY_SIZE(zone->is_start)) {
		if (!bitmap_get_bit(zone->is_start, i)) {
			i++;
			continue;
		}
		if (!header_printed) {
			ft_printf("TINY : %p\n", (void *)zone_addr);
			header_printed = 1;
		}

		j = 1;
		while (i + j < BIT_ARRAY_SIZE(zone->in_use) &&
		       j < TINY_SIZE_MAX / TINY_QUANTUM &&
		       !bitmap_get_bit(zone->is_start, i + j) &&
		       bitmap_get_bit(zone->in_use, i + j))
			j++;
		start_alloc = zone_addr + i * TINY_QUANTUM;
		end_alloc   = zone_addr + (i + j) * TINY_QUANTUM - 1;
		size_alloc  = end_alloc - start_alloc + 1;
		ft_printf("%p - %p : %zu bytes\n", (void *)start_alloc,
			  (void *)end_alloc, size_alloc);
		*total_alloc += size_alloc;
		i += j;
	}
}

/**
 * The one mandated debug entry point. Walk the global, address-ordered zone
 * registry under the list mutex and print every live allocation grouped by zone
 * type, then the grand total, in the subject's format. Taking only the list
 * mutex (never an arena lock) is consistent with the lock order: this is a
 * read-only registry walk. Live bitmaps/tags may shift the instant the lock is
 * dropped, so the snapshot is best-effort, which is all a debug dump needs.
 */
void show_alloc_mem(void)
{
	s_zone_hdr *current;
	size_t      total_alloc;
	e_zone_type zone_type;

	pthread_mutex_lock(&g_malloc_state.list_mutex);
	current = g_malloc_state.zone_list;
	if (current == NULL) {
		pthread_mutex_unlock(&g_malloc_state.list_mutex);
		return;
	}

	total_alloc = 0;
	while (current != NULL) {
		zone_type = current->type;
		if (zone_type == TINY_ZONE)
			print_tiny_zone((s_tiny_zone *)current, &total_alloc);
		else if (zone_type == SMALL_ZONE)
			print_small_zone((s_small_zone *)current, &total_alloc);
		else /* if (zone_type == LARGE_ZONE) */
			print_large_zone((s_zone_hdr *)current, &total_alloc);
		current = current->next;
	}
	pthread_mutex_unlock(&g_malloc_state.list_mutex);

	ft_printf("Total : %zu bytes\n", total_alloc);
}
