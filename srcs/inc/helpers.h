/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   helpers.h                                          :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: rotrojan <rotrojan@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/04/17 16:13:10 by rotrojan          #+#    #+#             */
/*   Updated: 2026/05/08 14:47:37 by rotrojan         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef HELPERS_H
#define HELPERS_H

#include <limits.h> /* for CHAR_BIT */

/* Number of elements in a real array `x` (not valid on a decayed pointer). */
#define ARRAY_SIZE(x) ((sizeof(x)) / (sizeof(*x)))
/* Number of addressable bits in a real array `x` (e.g. a uint64_t bitmap). */
#define BIT_ARRAY_SIZE(x) ((sizeof(x)) * (CHAR_BIT))
/* Ceiling of x / y for non-negative integers, without floating point. */
#define DIV_CEIL(x, y) (((x) + (y) - 1) / (y))
/**
 * Classic macro caveat: the selected argument is evaluated twice (once in the
 * comparison, once when returned), so do not pass side-effecting expressions
 * such as MAX(i++, j).
 */
#define MAX(x, y) ((x) > (y) ? (x) : (y))
#define MIN(x, y) ((x) < (y) ? (x) : (y))

#endif /* HELPERS_H */
