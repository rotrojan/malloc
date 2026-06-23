/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   malloc.h                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: rotrojan <rotrojan@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/12/16 19:52:54 by rotrojan          #+#    #+#             */
/*   Updated: 2026/06/22 22:30:13 by rotrojan         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef MALLOC_H
#define MALLOC_H

#include <stddef.h> /* for size_t */

/**
 * Public surface of the allocator. These three interpose the libc functions of
 * the same name when the library is loaded via LD_PRELOAD; they are the only
 * symbols a client program links against (show_alloc_mem is declared in its own
 * header). They follow the standard malloc(3) contract.
 *
 * calloc and reallocarray are not part of the subject. They are declared and
 * compiled only under -DEXTRA (make EXTRA=1), where they are also exported, so
 * real programs can run fully on this allocator -- see the note in malloc.c on
 * why exporting calloc by default is unsafe under LD_PRELOAD.
 */
void *malloc(size_t size);
void  free(void *ptr);
void *realloc(void *ptr, size_t size);
#ifdef EXTRA
void *calloc(size_t nmemb, size_t size);
void *reallocarray(void *ptr, size_t n, size_t size);
#endif

#endif /* MALLOC_H */
