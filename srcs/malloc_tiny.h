/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   malloc_tiny.h                                      :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: rotrojan <rotrojan@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/12/16 19:52:54 by rotrojan          #+#    #+#             */
/*   Updated: 2026/01/13 20:55:14 by rotrojan         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef MALLOC_TINY_H
#define MALLOC_TINY_H

#include "zone_type.h"

#include <stdint.h>

#define TINY_SIZE_MAX 128

/*
 * Most frequently, sysconf(_SC_PAGESIZE) = 4096.
 * We want to hold up to a hundred of 128 bytes TINY chunks. We can store 128
 * of these in a 4 pages zone.
 */
#define TINY_ZONE_SIZE (sysconf(_SC_PAGESIZE) * 4)

/*
 * The granularity of the TINY chunks is 16 bytes. We can store 1024 of them in
 * a TINY zone. These can be represented by a 16 uint64_t bitmap.
 */
typedef struct tiny_zone {
	struct tiny_zone *next;
	enum zone_type    zone_type;
	uint8_t           available_chunks;
	uint64_t          bitmap[16];
	/* 16, 32, 48, 64, 80, 96, 112, 128 */
	uint8_t alloc_size[1024];
} s_tiny_zone;

void *malloc_tiny(void);

#endif /* MALLOC_TINY_H */
