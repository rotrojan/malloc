/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   malloc.h                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: rotrojan <rotrojan@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/12/16 19:52:54 by rotrojan          #+#    #+#             */
/*   Updated: 2026/06/15 17:53:21 by rotrojan         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef MALLOC_H
#define MALLOC_H

#include <stddef.h> /* for size_t */

void *malloc(size_t size);
void  free(void *ptr);
void *realloc(void *ptr, size_t size);
/* void *calloc(size_t nmemb, size_t size); */
/* void *reallocarray(void *ptr, size_t n, size_t size); */

#endif /* MALLOC_H */
