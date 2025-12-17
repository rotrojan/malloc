/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   malloc.h                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: rotrojan <rotrojan@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/12/16 19:52:54 by rotrojan          #+#    #+#             */
/*   Updated: 2025/12/17 01:13:39 by rotrojan         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef MALLOC_H
#define MALLOC_H

#include <stddef.h> /* for size_t */

enum e_zone_type
{
	TINY,
	SMALL,
	LARGE,
	ZONE_TYPE_MAX
};

union u_zone
{
	struct tiny_zone *tiny;
	/* struct small_zone small; */
	/* struct large_zone large; */
};

union u_zone *g_zone[ZONE_TYPE_MAX] = { 0 };

void *malloc(size_t size);
void free(void *ptr);
void *realloc(void *ptr, size_t size);

#endif /* MALLOC_H */
