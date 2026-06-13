/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   malloc.c                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: rotrojan <rotrojan@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/11/19 14:36:30 by rotrojan          #+#    #+#             */
/*   Updated: 2026/06/13 16:08:36 by rotrojan         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

/* TODO: get rid of asserts ! */

#include "malloc.h"

#include "bitmap.h"
#include "free.h"
#include "helpers.h"
#include "libft.h"
#include "malloc_large.h"
#include "malloc_small.h"
#include "malloc_state.h"
#include "malloc_tiny.h"

#include <pthread.h>
#include <stddef.h> /* for size_t */
#include <sys/mman.h>
#include <sys/resource.h>
#include <unistd.h> /* for STDERR_FILENO */

s_malloc_state g_malloc_state = { .once_control = PTHREAD_ONCE_INIT };

static void init_malloc_state(void)
{
	struct rlimit rlim;

	if (getrlimit(RLIMIT_AS, &rlim) == 0 && rlim.rlim_cur != RLIM_INFINITY)
		g_malloc_state.max_memory = rlim.rlim_cur;
	else
		g_malloc_state.max_memory = SIZE_MAX;

	g_malloc_state.current_memory = 0;

	g_malloc_state.page_size = sysconf(_SC_PAGESIZE);
}

void *malloc(size_t size)
{
	void *ret = NULL;

	pthread_once(&g_malloc_state.once_control, &init_malloc_state);

	if (size <= TINY_SIZE_MAX)
		ret = malloc_tiny(size);
	else if (size <= SMALL_SIZE_MAX)
		ret = malloc_small(size);
	else
		ret = malloc_large(size);

	return ret;
}

void free(void *ptr)
{
	s_zone_hdr *zone;

	if (ptr == NULL)
		return;

	zone = find_zone(ptr);

	if (zone == NULL)
		return ft_dprintf(STDERR_FILENO,
				  "Fatal: invalid pointer passed to free!\n"
				  "%p: pointer does not belong to any zone.\n",
				  ptr);

	switch (zone->type) {
	case TINY_ZONE:
		return free_tiny(ptr, zone);
	case SMALL_ZONE:
		return free_small(ptr, zone);
	default:
		return release_zone(zone);
	}
}

void *realloc_tiny(void *ptr, size_t size, s_zone_hdr *zone_hdr)
{
	void        *new_ptr;
	s_tiny_zone *zone = (s_tiny_zone *)zone_hdr;
	size_t       old_needed_chunks =
		get_nb_chunks_tiny_alloc(ptr, (s_tiny_zone *)zone);
	size_t new_needed_chunks;
	size_t index;

	if (size > TINY_SIZE_MAX)
		goto new_alloc;

	index             = ((char *)ptr - (char *)zone) / TINY_SIZE_MIN;
	new_needed_chunks = MAX(1, DIV_CEIL(size, TINY_SIZE_MIN));

	if (new_needed_chunks < old_needed_chunks) {
		for (size_t i = index + new_needed_chunks;
		     i < index + old_needed_chunks; i++)
			bitmap_clear_bit(zone->in_use, i);
		zone->index_next_free_chunk = MIN(zone->index_next_free_chunk,
						  index + new_needed_chunks);
		return ptr;
	}

	for (size_t i = index + old_needed_chunks;
	     i < index + new_needed_chunks; i++)
		if (bitmap_get_bit(zone->in_use, i))
			goto new_alloc;

	for (size_t i = index + old_needed_chunks;
	     i < index + new_needed_chunks; i++)
		bitmap_set_bit(zone->in_use, i);

	return ptr;

new_alloc:
	new_ptr = malloc(size);
	if (new_ptr == NULL)
		return NULL;
	ft_memcpy(new_ptr, ptr, old_needed_chunks * TINY_SIZE_MIN);
	free(ptr);
	return new_ptr;
}

void *realloc_small(void *ptr, size_t size, s_zone_hdr *zone_hdr)
{
	void         *new_ptr;
	size_t        old_size  = GET_SIZE(CHUNK_HDR(ptr));
	size_t        real_size = (size + 2 * TAG_SIZE + SMALL_QUANTUM - 1) &
				  ~(SMALL_QUANTUM - 1);
	s_small_zone *zone      = (s_small_zone *)zone_hdr;

	if (size <= TINY_SIZE_MAX || size > SMALL_SIZE_MAX)
		goto new_alloc;

	if (old_size == real_size)
		return ptr;

	if (resize_chunk(ptr, real_size, zone->bin) != NULL)
		/* Because `resize_chunk()` returns ptr on success.*/
		return ptr;

new_alloc:
	new_ptr = malloc(size);
	if (new_ptr == NULL)
		return NULL;
	ft_memcpy(new_ptr, ptr,
		  MIN(size, GET_SIZE(CHUNK_HDR(ptr)) - 2 * TAG_SIZE));
	free(ptr);
	return new_ptr;
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

void *realloc(void *ptr, size_t size)
{
	s_zone_hdr *zone;

	pthread_once(&g_malloc_state.once_control, &init_malloc_state);

	/* Edge cases defined in `man malloc`.*/
	if (ptr == NULL)
		return malloc(size);
	if (size == 0) {
		free(ptr);
		return NULL;
	}

	zone = find_zone(ptr);
	if (zone == NULL) {
		ft_dprintf(STDERR_FILENO,
			   "Fatal: invalid pointer passed to realloc!\n"
			   "%p: pointer does not belong to any zone.\n",
			   ptr);
		return NULL;
	}

	switch (zone->type) {
	case TINY_ZONE:
		return realloc_tiny(ptr, size, zone);
	case SMALL_ZONE:
		return realloc_small(ptr, size, zone);
	default:
		return realloc_large(ptr, size, zone);
	}
}
