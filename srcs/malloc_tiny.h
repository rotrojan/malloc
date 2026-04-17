/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   malloc_tiny.h                                      :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: rotrojan <rotrojan@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/12/16 19:52:54 by rotrojan          #+#    #+#             */
/*   Updated: 2026/01/13 15:39:23 by rotrojan         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef MALLOC_TINY_H
#define MALLOC_TINY_H

#include <stdint.h>

#define TINY_CHUNK_SIZE 256

/*
 * Most frequently, sysconf(_SC_PAGESIZE) = 4096.
 * We want to work with 256 bytes TINY chunks. We can store 128 of those in an
 * 8 pages zone (the first one is reserved for metadata).
 */
#define TINY_ZONE_SIZE (sysconf(_SC_PAGESIZE) * 8)

typedef struct tiny_zone {
	struct tiny_zone *next;
	uint8_t           available_chunks;
	uint64_t          bitmap[2];
} s_tiny_zone;

void *malloc_tiny(void);

#endif /* MALLOC_TINY_H */
