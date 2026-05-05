/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   malloc_large.c                                     :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: rotrojan <rotrojan@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/04/30 15:45:23 by rotrojan          #+#    #+#             */
/*   Updated: 2026/05/05 21:25:55 by rotrojan         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "malloc_large.h"

#include "malloc_state.h" /* for PAGE_SIZE */
#include "zone.h"

#include <stddef.h>

void *malloc_large(size_t size)
{
	size_t real_size = (size + sizeof(s_zone_hdr) + PAGE_SIZE - 1) &
			   ~(PAGE_SIZE - 1);
	void  *zone      = new_zone(LARGE_ZONE, real_size);

	if (zone == NULL)
		return NULL;

	return zone + sizeof(s_zone_hdr);
}
