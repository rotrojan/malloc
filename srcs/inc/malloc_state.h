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

#define PAGE_SIZE (g_malloc_state.page_size)

/**
 * Upper bound on the arena pool. The actually-used count (nb_arenas) is
 * clamped to the number of online CPUs at init time. A thread picks its home
 * arena as hash(pthread_self()) % nb_arenas, so no per-thread state is needed.
 */
typedef struct malloc_state {
	pthread_once_t  once_control;
	size_t          max_memory;
	size_t          current_memory;
	size_t          page_size;
	s_zone_hdr     *zone_list;
	pthread_mutex_t list_mutex;
	pthread_mutex_t stat_mutex;
} s_malloc_state;

extern s_malloc_state g_malloc_state;

#endif /* MALLOC_STATE_H */
