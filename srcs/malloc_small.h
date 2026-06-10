/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   malloc_small.h                                     :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: rotrojan <rotrojan@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/05/12 20:08:29 by rotrojan          #+#    #+#             */
/*   Updated: 2026/06/10 21:15:08 by rotrojan         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef MALLOC_SMALL_H
#define MALLOC_SMALL_H

#include "helpers.h"
#include "malloc_state.h"
#include "zone.h"

#include <stddef.h>

typedef enum bin_idx {
	BIN_256,
	BIN_512,
	BIN_768,
	BIN_1024,
	BIN_1280,
	BIN_1536,
	BIN_1792,
	BIN_2048,
	BIN_OVERSIZED,
	NB_BINS
} e_bin_idx;

#define BIN_IDX(size) (MIN(((size) / SMALL_QUANTUM) - 1, BIN_OVERSIZED))

typedef struct free_list {
	struct free_list *next;
} s_free_list;

typedef size_t t_boundary_tag;
#define TAG_SIZE sizeof(t_boundary_tag)

#define TAG(size, in_use) (t_boundary_tag)((size) | (in_use))
#define GET_TAG(tag) (*(t_boundary_tag *)(tag))
#define PUT_TAG(tag, val) (*(t_boundary_tag *)(tag) = val)

#define GET_SIZE(tag) (GET_TAG(tag) & ~(0x8 - 1))
#define GET_STATE(tag) (GET_TAG(tag) & 0x1)

#define CHUNK_HDR(chunk) ((char *)(chunk) - TAG_SIZE)
#define CHUNK_FTR(chunk) \
	((char *)(chunk) + GET_SIZE(CHUNK_HDR(chunk)) - 2 * TAG_SIZE)

#define NEXT_CHUNK(chunk) \
	((char *)(chunk) + GET_SIZE(((char *)(chunk) - TAG_SIZE)))
#define PREV_CHUNK(chunk) \
	((char *)(chunk) - GET_SIZE(((char *)(chunk) - 2 * TAG_SIZE)))

#define IN_USE 1
#define FREE 0

#define SMALL_QUANTUM 256
#define NB_PAGE_SMALL_ZONE 64
#define SMALL_SIZE_MAX (8 * SMALL_QUANTUM - 2 * TAG_SIZE)
#define SMALL_ZONE_SIZE \
	(PAGE_SIZE * NB_PAGE_SMALL_ZONE) /* Enough to hold 128 2048B chunks */

typedef struct small_zone {
	s_zone_hdr         zone_hdr;
	struct small_zone *next;
	s_free_list       *bin[NB_BINS];

} s_small_zone;

void        *malloc_small(size_t size);
void        *coalesce(void *chunk, s_free_list **bin);
s_free_list *add_to_bin(s_free_list *chunk, s_free_list **free_list);
s_free_list *pop_from_bin(s_free_list **free_list);

#endif /* MALLOC_SMALL_H */
