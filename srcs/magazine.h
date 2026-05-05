/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   magazine.h                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: rotrojan <rotrojan@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/04/16 12:56:54 by rotrojan          #+#    #+#             */
/*   Updated: 2026/05/05 16:48:56 by rotrojan         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef MAGAZINE_H
#define MAGAZINE_H

#include "malloc_tiny.h"

#include <pthread.h>

typedef struct thread_local_storage {
	pthread_once_t once_control;
	pthread_key_t  key;
} s_thread_local_storage;

typedef struct magazine {
	s_tiny_zone *tiny_list;
	s_tiny_zone *tiny_hot;
	/* s_small_list small_list; */
	/* s_small_hot small_hot; */
} s_magazine;

s_magazine *get_magazine(void);

#endif /* MAGAZINE_H */
