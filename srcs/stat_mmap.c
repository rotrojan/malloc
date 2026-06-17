/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   stat_mmap.c                                        :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: rotrojan <rotrojan@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/05/07 14:22:49 by rotrojan          #+#    #+#             */
/*   Updated: 2026/06/17 23:18:45 by rotrojan         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "malloc_state.h"

#include <sys/mman.h>
#include <unistd.h> /* for STDERR_FILENO */

/**
 * The linker flags `-Wl,--wrap=mmap -Wl,--wrap=munmap` redirect every call to
 * mmap/munmap made within this library to __wrap_mmap/__wrap_munmap below.
 * The real syscall wrappers are still reachable via __real_mmap/__real_munmap,
 * which the linker resolves automatically.
 *
 * This lets us maintain an accurate current_memory counter without touching
 * every call site: __wrap_mmap enforces the OOM limit and increments the
 * counter on success; __wrap_munmap decrements it when the kernel reclaims
 * the mapping.
 */
extern void *__real_mmap(void *addr, size_t len, int prot, int flags,
			 int fildes, off_t off);
extern int   __real_munmap(void *addr, size_t len);

void *__wrap_mmap(void *addr, size_t len, int prot, int flags, int fildes,
		  off_t off)
{
	void *ptr;

	pthread_mutex_lock(&g_malloc_state.stat_mutex);
	if (g_malloc_state.current_memory + len > g_malloc_state.max_memory) {
		pthread_mutex_unlock(&g_malloc_state.stat_mutex);
		return MAP_FAILED;
	}

	ptr = __real_mmap(addr, len, prot, flags, fildes, off);

	if (ptr != MAP_FAILED)
		g_malloc_state.current_memory += len;
	pthread_mutex_unlock(&g_malloc_state.stat_mutex);

	return ptr;
}

int __wrap_munmap(void *addr, size_t len)
{
	int ret;

	pthread_mutex_lock(&g_malloc_state.stat_mutex);
	ret = __real_munmap(addr, len);

	if (ret == 0)
		g_malloc_state.current_memory -= len;
	pthread_mutex_unlock(&g_malloc_state.stat_mutex);

	return ret;
}
