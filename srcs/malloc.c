/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   malloc.c                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: rotrojan <rotrojan@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/11/19 14:36:30 by rotrojan          #+#    #+#             */
/*   Updated: 2026/04/17 22:32:35 by rotrojan         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

/* TODO: get rid of asserts ! */

#include "malloc.h"

#include "malloc_state.h"
#include "malloc_tiny.h"

#include <pthread.h>
#include <stddef.h>
#include <sys/resource.h>

s_malloc_state g_malloc_state = { .once_control = PTHREAD_ONCE_INIT };

static void init_malloc_state(void)
{
	struct rlimit rlim;

	getrlimit(RLIMIT_AS, &rlim);

	g_malloc_state.max_memory =
		rlim.rlim_cur == RLIM_INFINITY ? SIZE_MAX : rlim.rlim_max;
	g_malloc_state.current_memory = 0;
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
	(void)ptr;
}

void *realloc(void *ptr, size_t size)
{
	pthread_once(&g_malloc_state.once_control, &init_malloc_state);

	(void)ptr;
	(void)size;

	return NULL;
}
