/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   malloc.c                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: rotrojan <rotrojan@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/11/19 14:36:30 by rotrojan          #+#    #+#             */
/*   Updated: 2026/04/30 11:49:14 by rotrojan         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

/* TODO: get rid of asserts ! */

#include "malloc.h"

#include "free.h"
#include "libft.h"
#include "malloc_state.h"
#include "malloc_tiny.h"

#include <pthread.h>
#include <stddef.h> /* for size_t */
#include <sys/resource.h>
#include <unistd.h> /* for STDERR_FILENO */

s_malloc_state g_malloc_state = { .once_control = PTHREAD_ONCE_INIT };

static void init_malloc_state(void)
{
	struct rlimit rlim;

	getrlimit(RLIMIT_AS, &rlim);

	g_malloc_state.max_memory =
		rlim.rlim_cur == RLIM_INFINITY ? SIZE_MAX : rlim.rlim_max;
	g_malloc_state.current_memory = 0;

	g_malloc_state.page_size = sysconf(_SC_PAGESIZE);
}

void *malloc(size_t size)
{
	void *ret = NULL;

	pthread_once(&g_malloc_state.once_control, &init_malloc_state);

	if (size <= TINY_SIZE_MAX)
		ret = malloc_tiny(size);
	return ret;
}

void free(void *ptr)
{
	s_zone_hdr *zone = find_zone(ptr);

	if (zone == NULL)
		ft_dprintf(STDERR_FILENO,
			     "Fatal: invalid pointer passed to free!\n"
			     "%p: pointer does not belong to any zone.\n", ptr);

	switch (zone->type) {
	case TINY_ZONE:
		free_tiny(ptr, zone);
	default:
		return;
	}
}

void *realloc(void *ptr, size_t size)
{
	pthread_once(&g_malloc_state.once_control, &init_malloc_state);

	(void)ptr;
	(void)size;

	return NULL;
}
