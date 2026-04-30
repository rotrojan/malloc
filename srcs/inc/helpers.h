/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   helpers.h                                          :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: rotrojan <rotrojan@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/04/17 16:13:10 by rotrojan          #+#    #+#             */
/*   Updated: 2026/04/25 14:43:45 by rotrojan         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef HELPERS_H
#define HELPERS_H

#include <limits.h> /* for CHAR_BIT */

/* Helper macros. */
#define ARRAY_SIZE(x) ((sizeof(x)) / (sizeof(*x)))
#define BIT_ARRAY_SIZE(x) ((sizeof(x)) * (CHAR_BIT))
#define DIV_CEIL(x, y) (((x) + (y) - 1) / (y))
#define MAX(x, y) ((x) > (y) ? (x) : (y))

#endif /* HELPERS_H */
