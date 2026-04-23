/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   malloc_tiny.h                                      :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: rotrojan <rotrojan@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/12/16 19:52:54 by rotrojan          #+#    #+#             */
/*   Updated: 2026/04/23 11:39:13 by rotrojan         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef MALLOC_TINY_H
#define MALLOC_TINY_H

#include "zone.h"

#include <stddef.h> /* For size_t */
#include <stdint.h>

#define TINY_SIZE_MAX 128
#define TINY_SIZE_MIN 16
#define NB_CHUNKS_TINY_HDR DIV_CEIL(sizeof(s_tiny_zone), TINY_SIZE_MIN)

/**
 * Most frequently, sysconf(_SC_PAGESIZE) = 4096.
 * We want to hold up to a hundred of 128 bytes TINY chunks. We can store 128
 * of these in a 4 pages zone.
 */
#define TINY_ZONE_SIZE (size_t)(sysconf(_SC_PAGESIZE) * 4)

typedef struct tiny_zone {
	s_zone_hdr zone_hdr;
	/**
	 * The granularity of the TINY chunks is 16 bytes (TINY_SIZE_MIN). We
	 * can store 1024 of them in a TINY zone. These can be represented by a
	 * 16 uint64_t bitmap.
	 * 1 -> chunk is in use.
	 * 0 -> chunk is free to use.
	 */
	uint64_t in_use[16];
	uint64_t is_start[16];
	size_t   index_next_free_chunk;
} s_tiny_zone;

void *malloc_tiny(size_t size);

#endif /* MALLOC_TINY_H */
