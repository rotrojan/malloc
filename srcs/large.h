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

void *malloc_large(size_t size);
void *realloc_large(void *ptr, size_t size, s_zone_hdr *zone_hdr);

#endif /* LARGE_H */
