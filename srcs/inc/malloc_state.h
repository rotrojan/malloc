/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   malloc_state.h                                     :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: rotrojan <rotrojan@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/12/28 22:14:24 by rotrojan          #+#    #+#             */
/*   Updated: 2026/06/18 00:10:29 by rotrojan         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef MALLOC_STATE_H
#define MALLOC_STATE_H

#include "zone.h"

#include <pthread.h>

/**
 * The subject mandates zone sizes be multiples of the page size; every caller
 * reads it through this macro rather than calling sysconf repeatedly.
 */
#define PAGE_SIZE (g_malloc_state.page_size)

/**
 * The single permitted global for allocation *state* (the second permitted
 * global, g_arenas, holds the thread-safety shards). One instance,
 * g_malloc_state, is initialized exactly once via pthread_once on the first
 * malloc/realloc.
 *
 * It owns the global, address-ordered registry of every live zone and the two
 * leaf mutexes of the locking design:
 *   - list_mutex guards the zone_list registry. It also serializes LARGE-zone
 *     header mutation: a LARGE zone has no arena (hence no per-arena mutex), so
 *     when realloc_large shrinks one in place it mutates the header under this
 *     same lock the find_zone / show_alloc_mem walks hold;
 *   - stat_mutex guards the current_memory accounting in the mmap wrappers.
 * Neither is ever taken while holding an arena mutex in a way that inverts the
 * arena -> list -> stat lock order.
 */
typedef struct malloc_state {
	pthread_once_t  once_control; /* one-shot init guard */
	size_t          max_memory; /* RLIMIT_AS ceiling, or SIZE_MAX */
	size_t          current_memory; /* bytes currently mmap'd by us */
	size_t          page_size; /* sysconf(_SC_PAGESIZE) */
	s_zone_hdr     *zone_list; /* head of address-ordered registry */
	pthread_mutex_t list_mutex; /* guards zone_list + LARGE headers */
	pthread_mutex_t stat_mutex; /* guards current_memory (leaf lock) */
} s_malloc_state;

extern s_malloc_state g_malloc_state;

#endif /* MALLOC_STATE_H */
