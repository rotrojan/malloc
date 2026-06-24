/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   malloc.c                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: rotrojan <rotrojan@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/11/19 14:36:30 by rotrojan          #+#    #+#             */
/*   Updated: 2026/06/24 01:12:38 by rotrojan         ###   ########.fr       */
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

/**
 * The one global for allocation state; the mutexes are completed in
 * init_malloc_state, run once via this pthread_once control.
 */
s_malloc_state g_malloc_state = { .once_control = PTHREAD_ONCE_INIT };

/**
 * One-time initialization, fired by pthread_once on the first malloc/realloc.
 * Records the address-space limit (RLIMIT_AS, treated as unbounded when
 * infinite) that the mmap wrapper enforces and the page size every zone is
 * sized against, then explicitly initializes both leaf mutexes (list and stat).
 * The arena mutexes are initialized separately, in init_arenas under their own
 * pthread_once.
 */
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
	pthread_mutex_init(&g_malloc_state.stat_mutex, NULL);
}

void *malloc(size_t size)
{
	void *ret = NULL;

	pthread_once(&g_malloc_state.once_control, &init_malloc_state);

	/**
	 * Route by size class. The boundaries are the class ceilings; each
	 * worker acquires its own arena lock (alloc may use any arena).
	 */
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

	/**
	 * Locate the owning zone. A NULL result means the pointer belongs to no
	 * zone of ours (foreign pointer, e.g. a glibc allocation made before
	 * the LD_PRELOAD hook): report and return without touching anything.
	 */
	zone = find_zone(ptr);
	if (zone == NULL)
		return ft_dprintf(STDERR_FILENO,
				  "Fatal: invalid pointer passed to free!\n"
				  "%p: pointer does not belong to any zone.\n",
				  ptr);

	/**
	 * LARGE zones have no arena; release_zone takes the global lock itself.
	 */
	if (zone->type == LARGE_ZONE)
		return release_zone(zone);

	/**
	 * free must go to the zone's *owning* arena (never get_arena): the
	 * operation targets one specific existing zone. Cache zone->arena
	 * before dispatch -- the worker may munmap the zone, after which the
	 * header is gone. The pin invariant (a valid free holds a live pointer)
	 * guarantees no other thread can empty or release this zone before we
	 * lock it.
	 */
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

	/**
	 * Edge cases from `man malloc`: realloc(NULL, n) == malloc(n);
	 * realloc(p, 0) frees p and returns NULL.
	 */
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

	/**
	 * Each worker locks the owning arena itself (the realloc lock contract,
	 * symmetric to free); dispatch by the zone's class.
	 */
	if (zone->type == TINY_ZONE)
		return realloc_tiny(ptr, size, zone);
	else if (zone->type == SMALL_ZONE)
		return realloc_small(ptr, size, zone);
	else /* if (zone->type == LARGE_ZONE) */
		return realloc_large(ptr, size, zone);
}

#ifdef EXTRA
/**
 * calloc, reallocarray and malloc_usable_size are not part of the subject. They
 * are compiled in only under -DEXTRA (make EXTRA=1), so the default build
 * exports exactly the subject's malloc/free/realloc (+ show_alloc_mem); EXTRA
 * widens the surface enough to run real programs (e.g. ls, vim) on this
 * allocator.
 *
 * malloc_usable_size is the load-bearing one. Because we interpose
 * malloc/realloc, every live pointer in the process is ours, so any code that
 * then calls malloc_usable_size on one (e.g. systemd's nss greedy_realloc
 * during `ls -l` group lookup) would otherwise reach glibc's version, which
 * reads the bytes before our pointer as a glibc chunk header and walks off into
 * unmapped memory. Interposing it keeps glibc out of our heap;
 * calloc/reallocarray round out the surface for programs that call them.
 */

void *calloc(size_t n, size_t size)
{
	void  *ptr;
	size_t total;

	/**
	 * Reject n * size overflow before it wraps (n == 0 short-circuits the
	 * division). calloc, unlike malloc, must also zero the result.
	 */
	if (n != 0 && size > SIZE_MAX / n)
		return NULL;

	total = n * size;
	ptr   = malloc(total);
	if (ptr != NULL)
		ft_memset(ptr, 0, total);
	return ptr;
}

void *reallocarray(void *ptr, size_t n, size_t size)
{
	/* Same overflow guard, then defer to realloc on the product. */
	if (n != 0 && size > SIZE_MAX / n)
		return NULL;

	return realloc(ptr, n * size);
}

/**
 * Usable bytes in the allocation at `ptr` -- the per-class chunk capacity,
 * which is always >= the original request. Dispatches by zone type like
 * free/realloc (LARGE needs no arena lock); returns 0 for NULL or a pointer we
 * do not own.
 */
size_t malloc_usable_size(void *ptr)
{
	s_zone_hdr *zone;
	s_arena    *arena;
	size_t      ret;

	if (ptr == NULL)
		return 0;
	zone = find_zone(ptr);
	if (zone == NULL)
		return 0;

	if (zone->type == LARGE_ZONE)
		return zone->size - sizeof(s_zone_hdr);

	arena = zone->arena;
	pthread_mutex_lock(&arena->mutex);
	if (zone->type == TINY_ZONE)
		ret = get_nb_chunks_tiny_alloc(ptr, (s_tiny_zone *)zone) *
		      TINY_QUANTUM;
	else /* if (zone->type == SMALL_ZONE) */
		ret = GET_SIZE(CHUNK_HDR(ptr)) - 2 * TAG_SIZE;
	pthread_mutex_unlock(&arena->mutex);

	return ret;
}
#endif
