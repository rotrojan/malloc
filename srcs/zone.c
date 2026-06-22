/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   zone.c                                             :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: rotrojan <rotrojan@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/04/21 15:26:13 by rotrojan          #+#    #+#             */
/*   Updated: 2026/06/22 21:02:12 by rotrojan         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "zone.h"

#include "arena.h"
#include "libft.h"
#include "malloc_state.h"

#include <pthread.h>
#include <sys/mman.h>
#include <unistd.h>

static void push_zone_ordered(s_zone_hdr *zone)
{
	s_zone_hdr *prev    = NULL;
	s_zone_hdr *current = g_malloc_state.zone_list;

	while (current != NULL && (uintptr_t)zone > (uintptr_t)current) {
		prev    = current;
		current = current->next;
	}

	zone->prev = prev;
	zone->next = current;
	if (current != NULL)
		current->prev = zone;
	if (prev != NULL)
		prev->next = zone;
	else
		g_malloc_state.zone_list = zone;
}

static void pop_zone(s_zone_hdr *zone)
{
	if (zone->prev != NULL)
		zone->prev->next = zone->next;
	else
		g_malloc_state.zone_list = zone->next;
	if (zone->next != NULL)
		zone->next->prev = zone->prev;
}

static s_zone_hdr *add_zone_to_arena(s_zone_hdr *zone, s_arena *arena)
{
	s_tiny_zone  *tiny_zone;
	s_small_zone *small_zone;

	zone->arena = arena;
	if (zone->type == TINY_ZONE) {
		tiny_zone       = (s_tiny_zone *)zone;
		tiny_zone->prev = NULL;
		tiny_zone->next = arena->tiny_list;
		if (arena->tiny_list != NULL)
			arena->tiny_list->prev = tiny_zone;
		arena->tiny_list = tiny_zone;
		arena->tiny_hot  = tiny_zone;
	} else if (zone->type == SMALL_ZONE) {
		small_zone       = (s_small_zone *)zone;
		small_zone->prev = NULL;
		small_zone->next = arena->small_list;
		if (arena->small_list != NULL)
			arena->small_list->prev = small_zone;
		arena->small_list = small_zone;
		arena->small_hot  = small_zone;
	}

	return zone;
}

static void remove_tiny(s_tiny_zone *zone)
{
	s_arena *arena = zone->zone_hdr.arena;

	if (zone->prev != NULL)
		zone->prev->next = zone->next;
	else
		arena->tiny_list = zone->next;
	if (zone->next != NULL)
		zone->next->prev = zone->prev;

	arena->tiny_hot = arena->tiny_list;
}

static void remove_small(s_small_zone *zone)
{
	s_arena *arena = zone->zone_hdr.arena;

	if (zone->prev != NULL)
		zone->prev->next = zone->next;
	else
		arena->small_list = zone->next;
	if (zone->next != NULL)
		zone->next->prev = zone->prev;

	arena->small_hot = arena->small_list;
}

static void remove_zone_from_arena(s_zone_hdr *zone)
{
	if (zone->type == TINY_ZONE)
		remove_tiny((s_tiny_zone *)zone);
	else if (zone->type == SMALL_ZONE)
		remove_small((s_small_zone *)zone);
}

void *new_zone(e_zone_type zone_type, size_t size, s_arena *arena)
{
	s_zone_hdr *new_zone =
		(s_zone_hdr *)mmap(NULL, size, PROT_READ | PROT_WRITE,
				   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (new_zone == MAP_FAILED) {
		ft_dprintf(STDERR_FILENO, "Fatal: cannot create zone!\n");
		return NULL;
	}

	new_zone->magic    = MAGIC;
	new_zone->type     = zone_type;
	new_zone->size     = size;
	new_zone->self     = (uintptr_t)new_zone;
	new_zone->checksum = compute_checksum(new_zone);
	new_zone->arena    = arena;

	pthread_mutex_lock(&g_malloc_state.list_mutex);
	push_zone_ordered(new_zone);
	pthread_mutex_unlock(&g_malloc_state.list_mutex);

	if (zone_type != LARGE_ZONE)
		add_zone_to_arena(new_zone, arena);

	return new_zone;
}

void release_zone(s_zone_hdr *zone_hdr)
{
	pthread_mutex_lock(&g_malloc_state.list_mutex);
	pop_zone(zone_hdr);
	pthread_mutex_unlock(&g_malloc_state.list_mutex);

	if (zone_hdr->type != LARGE_ZONE)
		remove_zone_from_arena(zone_hdr);

	if (munmap(zone_hdr, zone_hdr->size))
		ft_dprintf(STDERR_FILENO, "Fatal: cannot relase zone!\n");
}

static int zone_is_valid(void *ptr, s_zone_hdr *zone)
{
	uintptr_t ptr_int  = (uintptr_t)ptr;
	uintptr_t zone_int = (uintptr_t)zone;

	if (zone->magic != MAGIC)
		return 0;

	if (zone->self != zone_int)
		return 0;

	if (zone->type == TINY_ZONE) {
		if (zone->size != TINY_ZONE_SIZE)
			return 0;
		if (ptr_int < zone_int + NB_CHUNKS_TINY_HDR * TINY_QUANTUM ||
		    ptr_int >= zone_int + zone->size)
			return 0;
	} else if (zone->type == SMALL_ZONE) {
		if (zone->size != SMALL_ZONE_SIZE)
			return 0;
		if (ptr_int < zone_int + SMALL_QUANTUM ||
		    ptr_int >= zone_int + zone->size)
			return 0;
	} else /*if (zone->type == LARGE_ZONE)*/ {
		if (ptr_int != zone_int + sizeof(s_zone_hdr))
			return 0;
	}

	if (zone->checksum != compute_checksum(zone))
		return 0;

	return 1;
}

/*
 * NOTE: this function is an admission of failure!
 * The initial idea was to get the address of the page with a oneliner bitwise
 * operation (page_addr = ptr_addr & ~(PAGE_SIZE - 1)), then backward pagewalk
 * from there until either the zone header is found. This allows finding the
 * zone of any malloc'ed pointer without keeping track of it.
 * Unfortunately, before the LD_PRELOAD hook occurs, some allocation can be
 * performed by the glibc (for instance, when invoking `ls -l`), which
 * eventually results in a free being performed on a pointer malloc'ed by
 * another API (and a crash of `find_zone()`).
 * Using the g_malloc_state.zone_list (that was initially intended to be only
 * used by `show_alloc_mem()`) is a fairly dirty workaround since it requires to
 * dereference the zone address and will cause TLB misses upon every `free()`.
 * Other solutions were possible, but they either added some considerable
 * limitations and overhead (like a hash table indexing all zone headers),
 * required more syscalls (like making the zones zone-aligned instead of
 * page-aligned, which could have been done by performing a bigger`mmap()` and
 * then trimming the head and the tail with two `munmap()`) or used a forbidden
 * function to be sure that the page was resident in memory (`man mincore`).
 */
s_zone_hdr *find_zone(void *ptr)
{
	pthread_mutex_lock(&g_malloc_state.list_mutex);
	for (s_zone_hdr *current = g_malloc_state.zone_list; current != NULL;
	     current             = current->next) {
		if ((uintptr_t)current > (uintptr_t)ptr) {
			break;
		}
		if (zone_is_valid(ptr, current)) {
			pthread_mutex_unlock(&g_malloc_state.list_mutex);
			return current;
		}
	}
	pthread_mutex_unlock(&g_malloc_state.list_mutex);

	return NULL;
}
