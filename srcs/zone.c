/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   zone.c                                             :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: rotrojan <rotrojan@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/04/21 15:26:13 by rotrojan          #+#    #+#             */
/*   Updated: 2026/05/05 19:13:01 by rotrojan         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "zone.h"
#include "libft.h"
#include "magazine.h"
#include "malloc_state.h"

#include <sys/mman.h>
#include <unistd.h>

static void push_zone_ordered(s_zone_hdr *zone)
{
	s_zone_hdr **zone_list = &g_malloc_state.zone_list;
	s_zone_hdr **current   = zone_list;

	while (*current != NULL && (uintptr_t)zone > (uintptr_t)(*current))
		current = &((*current)->next);
	zone->next = *current;
	*current   = zone;
}

static void pop_zone(s_zone_hdr *zone)
{
	s_zone_hdr **zone_list = &g_malloc_state.zone_list;
	s_zone_hdr **current = zone_list;

	while (*current != NULL) {
		if (*current == zone) {
			*current = (*current)->next;
			return;
		}
		current = &((*current)->next);
	}
}

static s_zone_hdr *add_zone_to_magazine(s_zone_hdr *zone)
{
	s_magazine  *mag = get_magazine();
	s_tiny_zone *tiny_zone;
	/* s_small_zone *small_zone; */

	if (zone->type == TINY_ZONE) {
		tiny_zone       = (s_tiny_zone *)zone;
		tiny_zone->next = mag->tiny_list;
		mag->tiny_list  = tiny_zone;
		mag->tiny_hot   = tiny_zone;
	}
	/* else if (zone->type == SMALL_ZONE) { */
	/* 	small_zone = (s_small_zone *)zone; */
	/* 	small_zone->next = *small_list; */
	/* 	*small_list      = small_zone; */
	/* 	*small_hot = small_zone; */
	/* } */

	return zone;
}

static void remove_tiny(s_tiny_zone *zone)
{
	s_magazine   *mag     = get_magazine();
	s_tiny_zone **current = &mag->tiny_list;

	while (*current != NULL) {
		if (*current == zone) {
			*current = (*current)->next;
			break;
		}
		current = &((*current)->next);
	}

	mag->tiny_hot = mag->tiny_list;
}

static void remove_zone_from_magazine(s_zone_hdr *zone)
{
	if (zone->type == TINY_ZONE)
		remove_tiny((s_tiny_zone *)zone);

	/* else if (zone->type == SMALL_ZONE) { */
	/* 	small_zone = (s_small_zone *)zone; */
	/* 	small_zone->next = *small_list; */
	/* 	*small_list      = small_zone; */
	/* 	*small_hot = small_zone; */
	/* } */
}

void *new_zone(e_zone_type zone_type, size_t size)
{
	s_zone_hdr *new_zone;

	new_zone = (s_zone_hdr *)mmap(NULL, size, PROT_READ | PROT_WRITE,
				      MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	if (new_zone == MAP_FAILED)
		return NULL;

	new_zone->magic    = MAGIC;
	new_zone->type     = zone_type;
	new_zone->size     = size;
	new_zone->self     = (uintptr_t)new_zone;
	new_zone->checksum = compute_checksum(new_zone);
	new_zone->next     = NULL;

	push_zone_ordered(new_zone);

	if (zone_type != LARGE_ZONE)
		add_zone_to_magazine(new_zone);

	return new_zone;
}

void release_zone(s_zone_hdr *zone_hdr)
{
	pop_zone(zone_hdr);

	if (zone_hdr->type != LARGE_ZONE)
		remove_zone_from_magazine(zone_hdr);

	if (munmap(zone_hdr, zone_hdr->size))
		ft_dprintf(STDERR_FILENO, "Fatal: cannot relase zone!\n");
}
