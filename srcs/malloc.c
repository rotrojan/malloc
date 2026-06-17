/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   malloc.c                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: rotrojan <rotrojan@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/11/19 14:36:30 by rotrojan          #+#    #+#             */
/*   Updated: 2026/06/17 22:51:44 by rotrojan         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "malloc.h"

#include "arena.h"
#include "large.h"
#include "libft.h"
#include "malloc_state.h"
#include "small.h"
#include "tiny.h"
#include "zone.h"

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

	pthread_mutex_init(&g_malloc_state.list_mutex, NULL);
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
	s_arena    *arena;

	if (ptr == NULL)
		return;

	zone = find_zone(ptr);
	if (zone == NULL)
		return ft_dprintf(STDERR_FILENO,
				  "Fatal: invalid pointer passed to free!\n"
				  "%p: pointer does not belong to any zone.\n",
				  ptr);

	if (zone->type == LARGE_ZONE)
		return release_zone(zone);

	arena = zone->arena;
	pthread_mutex_lock(&arena->mutex);
	if (zone->type == TINY_ZONE)
		free_tiny(ptr, zone);
	else /* if (zone->type == SMALL_ZONE) */
		free_small(ptr, zone);
	pthread_mutex_unlock(&arena->mutex);
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

/*
 * calloc and reallocarray are intentionally not implemented.
 *
 * Exporting either symbol via LD_PRELOAD causes any library in the process
 * (e.g. nss-systemd during `ls -la` group-name resolution) that calls
 * `calloc()` to receive a pointer from our heap. When glibc later inspects that
 * pointer through `malloc_usable_size()` — which it calls internally before
 * deciding whether to realloc — it tries to parse it as a glibc chunk header
 * and crashes. The subject only requires `malloc()`, `free()` and `realloc()`,
 * so the simplest correct fix is to leave these symbols unimplemented and let
 * the dynamic linker fall through to glibc.
 *
 * void *calloc(size_t n, size_t size)
 * {
 * 	void  *ptr;
 * 	size_t total;
 *
 * 	if (n != 0 && size > SIZE_MAX / n)
 * 		return NULL;
 *
 * 	total = n * size;
 * 	ptr   = malloc(total);
 * 	if (ptr != NULL)
 * 		ft_memset(ptr, 0, total);
 * 	return ptr;
 * }
 *
 * void *reallocarray(void *ptr, size_t n, size_t size)
 * {
 * 	if (n != 0 && size > SIZE_MAX / n)
 * 		return (NULL);
 *
 * 	return (realloc(ptr, n * size));
 * }
 */
