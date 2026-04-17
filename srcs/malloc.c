/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   malloc.c                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: rotrojan <rotrojan@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/11/19 14:36:30 by rotrojan          #+#    #+#             */
/*   Updated: 2026/01/13 15:39:38 by rotrojan         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "malloc.h"

#include "_malloc.h"
#include "malloc_tiny.h"

#include <stddef.h>

s_zone g_zone;

void *malloc(size_t size)
{
	void *ret = NULL;

	if (size <= TINY_CHUNK_SIZE)
		ret = malloc_tiny();
	return ret;
}

void free(void *ptr)
{
	(void)ptr;
}

void *realloc(void *ptr, size_t size)
{
	(void)ptr;
	(void)size;

	return NULL;
}
