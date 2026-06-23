/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   arena.h                                            :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: rotrojan <rotrojan@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/04/16 12:56:54 by rotrojan          #+#    #+#             */
/*   Updated: 2026/06/17 22:20:38 by rotrojan         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef ARENA_H
#define ARENA_H

#include "small.h"
#include "tiny.h"

#include <pthread.h>

/**
 * An arena is a shard of the allocator. There is a fixed, bounded pool of them
 * (g_arena); a thread merely derives a starting arena from its own id, it does
 * not own one. A single mutex per arena protects everything reachable from it:
 * its tiny/small zone lists, those zones' internal state, and the lifetime of
 * those zones (a zone is unlinked + munmap'd under this mutex). Because the
 * mutex lives in the arena (which outlives every zone), it can be held across a
 * zone's munmap.
 */
typedef struct arena {
	s_tiny_zone    *tiny_list; /* this arena's TINY zones (doubly-linked) */
	s_tiny_zone    *tiny_hot; /* TINY zone probed first on allocation */
	s_small_zone   *small_list; /* this arena's SMALL zones */
	s_small_zone   *small_hot; /* SMALL zone probed first on allocation */
	pthread_mutex_t mutex; /* guards everything reachable above */
} s_arena;

/**
 * @brief  Acquire an arena to allocate from, returned with its mutex *locked*.
 *
 * The caller (malloc_tiny / malloc_small) owns the lock and must unlock it on
 * every return path. Any arena will do for a fresh allocation, so this tries
 * the thread's home arena (hash of its id) first, hops to any uncontended arena
 * on a failed trylock, and only blocks on the home arena as a last resort.
 * free/realloc do *not* use this -- they must lock the zone's owning arena.
 *
 * @return A pointer to a locked arena (never NULL: the pool is static).
 */
s_arena *get_arena(void);

#endif /* ARENA_H */
