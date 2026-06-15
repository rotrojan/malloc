/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   large.c                                            :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: rotrojan <rotrojan@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/04/30 15:45:23 by rotrojan          #+#    #+#             */
/*   Updated: 2026/06/15 15:59:05 by rotrojan         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "large.h"

#include "libft.h"
#include "malloc.h"
#include "malloc_state.h" /* for PAGE_SIZE */
#include "small.h"
#include "zone.h"

#include <stddef.h>
#include <sys/mman.h>
#include <unistd.h>

void *malloc_large(size_t size)
{
	size_t real_size = (size + sizeof(s_zone_hdr) + PAGE_SIZE - 1) &
			   ~(PAGE_SIZE - 1);
	void  *zone      = new_zone(LARGE_ZONE, real_size);

	if (zone == NULL)
		return NULL;

	return (char *)zone + sizeof(s_zone_hdr);
}

void *realloc_large(void *ptr, size_t size, s_zone_hdr *zone_hdr)
{
	void  *new_ptr;
	size_t new_real_size;

	if (size <= SMALL_SIZE_MAX)
		goto new_alloc;

	new_real_size = (size + sizeof(s_zone_hdr) + PAGE_SIZE - 1) &
			~(PAGE_SIZE - 1);

	if (new_real_size == zone_hdr->size)
		return ptr;

	if (new_real_size < zone_hdr->size) {
		if (munmap((char *)zone_hdr + new_real_size,
			   zone_hdr->size - new_real_size) == -1) {
			ft_dprintf(STDERR_FILENO,
				   "Fatal: cannot shrink zone!\n");
			return NULL;
		}
		zone_hdr->size = new_real_size;

		return ptr;
	}

new_alloc:
	new_ptr = malloc(size);
	if (new_ptr == NULL)
		return NULL;
	ft_memcpy(new_ptr, ptr, MIN(size, zone_hdr->size - sizeof(s_zone_hdr)));
	free(ptr);
	return new_ptr;
}
