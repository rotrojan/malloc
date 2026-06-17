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

#define NB_ARENA_MAX 8

s_arena g_arena[NB_ARENA_MAX];

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

	if (pthread_mutex_trylock(&arena->mutex) == 0)
		return arena;

	for (int i = 1; i < NB_ARENA_MAX; i++) {
		if (pthread_mutex_trylock(
			    &g_arena[(index + i) % NB_ARENA_MAX].mutex) == 0)
			return &g_arena[(index + i) % NB_ARENA_MAX];
	}

	pthread_mutex_lock(&arena->mutex);

	return arena;
}
