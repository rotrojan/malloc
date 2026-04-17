/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   bitmap.c                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: rotrojan <rotrojan@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/12/28 16:15:20 by rotrojan          #+#    #+#             */
/*   Updated: 2026/04/17 18:49:41 by rotrojan         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "bitmap.h"

#include <assert.h> /* for CHAR_BIT */
#include <limits.h> /* for CHAR_BIT */
#include <stddef.h> /* for size_t*/
#include <stdint.h> /* for uint64_t */

#define BITS_PER_WORD (sizeof(uint64_t) * CHAR_BIT)

static inline size_t bitmap_skip_eight(uint64_t window)
{
	static const uint8_t skip_map[] = {
		[0] = 8,         [1ULL << 0] = 1, [1ULL << 1] = 2,
		[1ULL << 2] = 3, [1ULL << 3] = 4, [1ULL << 4] = 5,
		[1ULL << 5] = 6, [1ULL << 6] = 7, [1ULL << 7] = 8,
	};
	uint64_t inverted;
	uint64_t least_significant_bit;

	/* We want to find the index of the least significant 0, not 1. */
	inverted = ~window;

	/* Little bitwise trick to isolate lowest set bit. */
	least_significant_bit = inverted & -inverted;

	/* Masking the 8 last bits avoid out-of-bounds indexes. */
	return skip_map[least_significant_bit & 0xFF];
}

size_t bitmap_find_consecutive_zeros(uint64_t *bitmap, size_t size,
				     size_t requested, size_t hint)
{
	size_t const   total_bits = size * BITS_PER_WORD;
	uint64_t const mask       = (1ULL << requested) - 1;
	size_t         word_index;
	size_t         offset;
	uint8_t        window;
	size_t         i = hint;

	assert(requested > 0 && requested <= 8);

	if (hint >= total_bits)
		return SIZE_MAX;

	while (i <= total_bits - requested) {
		word_index = i / BITS_PER_WORD;
		offset     = i % BITS_PER_WORD;

		if (offset == 0 && bitmap[word_index] == UINT64_MAX) {
			i += BITS_PER_WORD;
			continue;
		}

		window = bitmap[word_index] >> offset;
		if (offset + requested > BITS_PER_WORD)
			window |= bitmap[word_index + 1]
				  << (BITS_PER_WORD - offset);

		if ((window & mask) == 0)
			return i;

		/* Little optimization to avoid redundant checks. */
		i += bitmap_skip_eight(window);
	}

	return SIZE_MAX;
}

void bitmap_set_range(uint64_t *bitmap, size_t index, size_t range)
{
	size_t   word_index;
	size_t   offset;
	uint64_t mask = (1ULL << range) - 1;

	assert(range > 0 && range <= 8);

	word_index = index / BITS_PER_WORD;
	offset     = index % BITS_PER_WORD;

	bitmap[word_index] |= mask << offset;
	if (offset + range > BITS_PER_WORD)
		bitmap[word_index + 1] |= (mask >> (BITS_PER_WORD - offset));
}

void bitmap_clear_range(uint64_t *bitmap, size_t index, size_t range)
{
	size_t   word_index;
	size_t   offset;
	uint64_t mask = (1ULL << range) - 1;

	assert(range > 0 && range <= 8);

	word_index = index / BITS_PER_WORD;
	offset     = index % BITS_PER_WORD;

	bitmap[word_index] &= ~(mask << offset);
	if (offset + range > BITS_PER_WORD)
		bitmap[word_index + 1] &= ~(mask >> (BITS_PER_WORD - offset));
}
