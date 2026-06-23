/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   arena.c                                            :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: rotrojan <rotrojan@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/04/16 12:58:34 by rotrojan          #+#    #+#             */
/*   Updated: 2026/06/23 14:34:02 by rotrojan         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "arena.h"

#include <pthread.h>

/* The second (and last) permitted global: the thread-safety arena pool. */
s_arenas g_arenas = { .once_control = PTHREAD_ONCE_INIT };

/**
 * Initialize every arena mutex explicitly, fired once through
 * g_arenas.once_control on the first get_arena -- the arena-pool analogue of
 * init_malloc_state. The rest of g_arenas is zero-initialized at load time.
 */
static void init_arenas(void)
{
	for (size_t i = 0; i < NB_ARENA_MAX; i++)
		pthread_mutex_init(&g_arenas.arena[i].mutex, NULL);
}

/**
 * Map a thread id to a home arena index. pthread_t on glibc is the address of
 * the thread control block, whose entropy sits in the middle bits while the low
 * bits are often alignment zeros -- a plain `% NB_ARENA_MAX` would cluster. So
 * fold the high half into the low half (then quarter, then eighth) to mix the
 * bits down before the caller takes the modulo. This is only a starting hint:
 * correctness never depends on which arena a thread lands in.
 */
static uintptr_t get_home_arena_idx(pthread_t thread_id)
{
	uintptr_t ret = (uintptr_t)thread_id;

	ret ^= (ret >> (sizeof(pthread_t) * CHAR_BIT / 2));
	ret ^= (ret >> (sizeof(pthread_t) * CHAR_BIT / 4));
	ret ^= (ret >> (sizeof(pthread_t) * CHAR_BIT / 8));

	return ret;
}

s_arena *get_arena(void)
{
	s_arena *arena = NULL;
	size_t   idx;

	pthread_once(&g_arenas.once_control, &init_arenas);

	idx   = get_home_arena_idx(pthread_self()) % NB_ARENA_MAX;
	arena = &g_arenas.arena[idx];

	/* Fast path: home arena is uncontended. */
	if (pthread_mutex_trylock(&arena->mutex) == 0)
		return arena;

	/**
	 * Contended: hop the rest of the pool, taking the first free arena
	 * without blocking. Any arena can satisfy a fresh allocation.
	 */
	for (int i = 1; i < NB_ARENA_MAX; i++) {
		if (pthread_mutex_trylock(
			    &g_arenas.arena[(idx + i) % NB_ARENA_MAX].mutex) ==
		    0)
			return &g_arenas.arena[(idx + i) % NB_ARENA_MAX];
	}

	/* Whole pool is busy: block on the home arena rather than spin. */
	pthread_mutex_lock(&arena->mutex);

	return arena;
}
