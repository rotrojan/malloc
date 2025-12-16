/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   malloc.c                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: rotrojan <rotrojan@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/11/19 14:36:30 by rotrojan          #+#    #+#             */
/*   Updated: 2025/12/16 21:40:22 by rotrojan         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "malloc.h"

#include <stddef.h> /* for NULL */

void *malloc(size_t size)
{
	(void)size;

	return NULL;
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
