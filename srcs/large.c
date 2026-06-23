/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   large.c                                            :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: rotrojan <rotrojan@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/04/30 15:45:23 by rotrojan          #+#    #+#             */
/*   Updated: 2026/06/17 22:52:20 by rotrojan         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "large.h"

#include "libft.h"
#include "malloc.h"
#include "malloc_state.h" /* for PAGE_SIZE */
#include "small.h"
#include "zone.h"

#include <pthread.h>
#include <stddef.h>
#include <stdint.h> /* for SIZE_MAX */
#include <sys/mman.h>
#include <unistd.h>

/**
 * Total mapping size for a LARGE payload of `size` bytes: header + payload
 * rounded up to a whole number of pages (the subject requires page-multiple
 * zones). Returns 0 when the request is so large the page rounding would
 * overflow size_t; callers treat 0 as failure. Without this guard an
 * overflowing size wraps to a tiny mapping the caller mistakes for a huge one.
 */
static size_t large_zone_size(size_t size)
{
	if (size > SIZE_MAX - sizeof(s_zone_hdr) - PAGE_SIZE)
		return 0;
	return (size + sizeof(s_zone_hdr) + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
}

void *malloc_large(size_t size)
{
	size_t real_size = large_zone_size(size);
	void  *zone;

	/* 0 means the request is unsatisfiable (the rounding would overflow). */
	if (real_size == 0)
		return NULL;

	/* NULL arena: LARGE zones live only in the global registry. */
	zone = new_zone(LARGE_ZONE, real_size, NULL);
	if (zone == NULL)
		return NULL;

	return (char *)zone + sizeof(s_zone_hdr);
}

void *realloc_large(void *ptr, size_t size, s_zone_hdr *zone_hdr)
{
	void  *new_ptr;
	size_t new_real_size;

	/* Shrinking below the LARGE floor changes class: relocate. */
	if (size <= SMALL_SIZE_MAX)
		goto new_alloc;

	new_real_size = large_zone_size(size);
	if (new_real_size == 0)
		return NULL;

	/* Same page count: the mapping already fits, nothing to do. */
	if (new_real_size == zone_hdr->size) {
		return ptr;
	}

	/**
	 * Tail-shrink in place: release the surplus pages and update the
	 * header. A LARGE zone has no arena, so the global list mutex is its
	 * serialization point against find_zone / show_alloc_mem walks; the
	 * header (size + checksum) is mutated under it.
	 */
	if (new_real_size < zone_hdr->size) {
		pthread_mutex_lock(&g_malloc_state.list_mutex);
		if (munmap((char *)zone_hdr + new_real_size,
			   zone_hdr->size - new_real_size) == -1) {
			pthread_mutex_unlock(&g_malloc_state.list_mutex);
			ft_dprintf(STDERR_FILENO,
				   "Fatal: cannot shrink zone!\n");
			return NULL;
		}
		zone_hdr->size     = new_real_size;
		zone_hdr->checksum = compute_checksum(zone_hdr);

		pthread_mutex_unlock(&g_malloc_state.list_mutex);
		return ptr;
	}

new_alloc:
	/**
	 * Grow (or shrink out of LARGE): no in-place grow for an anonymous
	 * mapping, so allocate fresh, copy min(new, old payload), free the old.
	 */
	new_ptr = malloc(size);
	if (new_ptr == NULL)
		return NULL;
	ft_memcpy(new_ptr, ptr, MIN(size, zone_hdr->size - sizeof(s_zone_hdr)));
	free(ptr);
	return new_ptr;
}
