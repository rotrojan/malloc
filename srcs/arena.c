/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   arena.c                                            :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: rotrojan <rotrojan@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/04/16 12:58:34 by rotrojan          #+#    #+#             */
/*   Updated: 2026/06/17 22:54:19 by rotrojan         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "arena.h"

#include <pthread.h>

/**
 * Fixed, bounded pool of arenas. Eight is a pragmatic compromise: enough to cut
 * contention on common core counts, small enough that the trylock-hop fallback
 * stays cheap. The array lives in BSS, so every mutex starts zero-initialized
 * -- a valid PTHREAD_MUTEX_INITIALIZER on glibc -- and needs no explicit init.
 */
#define NB_ARENA_MAX 8

/* The second (and last) permitted global: the thread-safety shard array. */
s_arena g_arena[NB_ARENA_MAX];

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
	size_t   index = get_home_arena_idx(pthread_self()) % NB_ARENA_MAX;

	arena = &g_arena[index];

	/* Fast path: home arena is uncontended. */
	if (pthread_mutex_trylock(&arena->mutex) == 0)
		return arena;

	/**
	 * Contended: hop the rest of the pool, taking the first free arena
	 * without blocking. Any arena can satisfy a fresh allocation.
	 */
	for (int i = 1; i < NB_ARENA_MAX; i++) {
		if (pthread_mutex_trylock(
			    &g_arena[(index + i) % NB_ARENA_MAX].mutex) == 0)
			return &g_arena[(index + i) % NB_ARENA_MAX];
	}

	/* Whole pool is busy: block on the home arena rather than spin. */
	pthread_mutex_lock(&arena->mutex);

	return arena;
}
