/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   large.h                                            :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: rotrojan <rotrojan@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/04/30 15:44:25 by rotrojan          #+#    #+#             */
/*   Updated: 2026/06/15 16:07:18 by rotrojan         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef LARGE_H
#define LARGE_H

#include "zone.h"

#include <stddef.h>

/**
 * @brief      Allocate `size` bytes (> SMALL_SIZE_MAX) in a dedicated zone.
 *
 * Each LARGE allocation is its own mmap'd zone, sized to fit the header plus
 * payload rounded up to a page boundary. LARGE zones belong to no arena.
 *
 * @param size Requested payload size.
 * @return     A pointer just past the zone header, or NULL on failure.
 */
void *malloc_large(size_t size);

/**
 * @brief          Resize a LARGE allocation.
 *
 * Grows by allocating a new zone and copying (no in-place grow, since the
 * mapping cannot be extended in general). Shrinks in place by munmap-ing the
 * surplus tail pages and updating the header under the global list mutex.
 *
 * @param ptr      Pointer returned by malloc_large.
 * @param size     New requested size.
 * @param zone_hdr The owning LARGE zone.
 * @return         ptr if resized in place, a new pointer if it moved (and `ptr`
 *                 is freed), or NULL on failure.
 */
void *realloc_large(void *ptr, size_t size, s_zone_hdr *zone_hdr);

#endif /* LARGE_H */
