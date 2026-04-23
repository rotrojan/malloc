/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   zone.c                                             :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: rotrojan <rotrojan@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/04/21 15:26:13 by rotrojan          #+#    #+#             */
/*   Updated: 2026/04/23 17:14:30 by rotrojan         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "zone.h"
#include "libft.h"
#include "magazine.h"
#include "malloc_state.h"

#include <sys/mman.h>
#include <unistd.h>

static void push_zone_ordered(s_zone_hdr **zone_list, s_zone_hdr *zone)
{
	s_zone_hdr **current = zone_list;

	while (*current != NULL && (uintptr_t)zone > (uintptr_t)(*current))
		current = &((*current)->next);
	zone->next = *current;
	*current   = zone;
}

static s_zone_hdr *add_zone_to_magazine(s_zone_hdr *zone)
{
	s_magazine  *mag  = get_magazine();
	s_zone_hdr **list = NULL;
	void       **hot  = NULL;

	if (zone->type == TINY_ZONE) {
		list = &mag->tiny_list;
		hot  = (void **)&mag->tiny_hot;
	}
	/* else if (zone->type == SMALL_ZONE) {*/
	/* list = mag->small_list; */
	/* hot = (s_zone_hdr *)mag->small_hot; */
	/* } */
	zone->next = *list;
	*list      = zone;

	*hot = zone;

	return zone;
}

void *new_zone(e_zone_type zone_type, size_t size)
{
	s_zone_hdr *new_zone;

	new_zone = (s_zone_hdr *)mmap(NULL, size, PROT_READ | PROT_WRITE,
				      MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	if (new_zone == NULL)
		return NULL;

	new_zone->magic    = MAGIC;
	new_zone->type     = zone_type;
	new_zone->size     = size;
	new_zone->self     = (uintptr_t)new_zone;
	new_zone->checksum = compute_checksum(new_zone);
	new_zone->next     = NULL;

	push_zone_ordered(&g_malloc_state.zone_list, new_zone);

	if (zone_type != LARGE_ZONE)
		add_zone_to_magazine(new_zone);

	return new_zone;
}

void release_zone(s_zone_hdr *zone_hdr)
{
	if (munmap(zone_hdr, zone_hdr->size))
		ft_dprintf(STDERR_FILENO, "Fatal: cannot relase zone!\n");
}
