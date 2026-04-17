/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   _malloc.h                                          :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: rotrojan <rotrojan@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/12/28 22:14:24 by rotrojan          #+#    #+#             */
/*   Updated: 2026/01/13 20:54:51 by rotrojan         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef _MALLOC_H
#define _MALLOC_H

#include "malloc_tiny.h"

typedef struct zone {
	s_tiny_zone *tiny;
	/* struct small_zone small; */
	/* struct large_zone large; */
} s_zone;

extern s_zone g_zone;

#endif /* _MALLOC_H */
