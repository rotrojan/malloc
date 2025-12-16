/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   malloc_tiny.c                                      :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: rotrojan <rotrojan@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/12/16 19:57:52 by rotrojan          #+#    #+#             */
/*   Updated: 2025/12/16 21:32:50 by rotrojan         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "malloc_tiny.h"

#include "malloc.h"

#include <stddef.h>

static struct tiny_zone *new_tiny_zone(void)
{
	return NULL;
}
static struct tiny_zone *get_usable_zone(void)
{
	if (g_zone[TINY] == NULL)
		new_tiny_zone();
	return NULL;
}

void *malloc_tiny(void)
{
	struct tiny_zone *current_zone = get_usable_zone();
	uint8_t index_first_free_block = 1;

	return current_zone + index_first_free_block * TINY_SIZE_MAX;
}
