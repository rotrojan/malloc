/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   bitmap.c                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: rotrojan <rotrojan@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/12/28 16:15:20 by rotrojan          #+#    #+#             */
/*   Updated: 2026/06/15 18:25:35 by rotrojan         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "bitmap.h"

#include <stddef.h> /* for size_t*/
#include <stdint.h> /* for uint64_t */

uint64_t bitmap_get_bit(uint64_t *bitmap, size_t index)
{
	size_t word_index = index / BITS_PER_WORD;
	size_t offset     = index % BITS_PER_WORD;

	return bitmap[word_index] & (1ULL << offset);
}

void bitmap_set_bit(uint64_t *bitmap, size_t index)
{
	size_t word_index = index / BITS_PER_WORD;
	size_t offset     = index % BITS_PER_WORD;

	bitmap[word_index] |= (1ULL << offset);
}

void bitmap_clear_bit(uint64_t *bitmap, size_t index)
{
	size_t word_index = index / BITS_PER_WORD;
	size_t offset     = index % BITS_PER_WORD;

	bitmap[word_index] &= ~(1ULL << offset);
}

/**
 * Set `range` (1-8) consecutive bits starting at `index`. The low `range` bits
 * of `mask` are set, then shifted into place. When the run straddles a 64-bit
 * word boundary, the part that overflowed past bit 63 is the high bits of the
 * shifted mask; it is folded down (mask >> (BITS_PER_WORD - offset)) into the
 * next word. Range <= 8 (the TINY class max) guarantees the spill touches at
 * most one extra word.
 */
void bitmap_set_range(uint64_t *bitmap, size_t index, size_t range)
{
	size_t   word_index;
	size_t   offset;
	uint64_t mask = (1ULL << range) - 1;

	word_index = index / BITS_PER_WORD;
	offset     = index % BITS_PER_WORD;

	bitmap[word_index] |= mask << offset;
	if (offset + range > BITS_PER_WORD)
		bitmap[word_index + 1] |= (mask >> (BITS_PER_WORD - offset));
}

/**
 * Clear `range` (1-8) bits at `index`; the AND-with-inverted-mask mirror of
 * bitmap_set_range, including the same word-boundary spill into the next word.
 */
void bitmap_clear_range(uint64_t *bitmap, size_t index, size_t range)
{
	size_t   word_index;
	size_t   offset;
	uint64_t mask = (1ULL << range) - 1;

	word_index = index / BITS_PER_WORD;
	offset     = index % BITS_PER_WORD;

	bitmap[word_index] &= ~(mask << offset);
	if (offset + range > BITS_PER_WORD)
		bitmap[word_index + 1] &= ~(mask >> (BITS_PER_WORD - offset));
}

/**
 * How far the consecutive-zeros search advances after a window fails to match.
 * `window` has a bit set for each occupied chunk; a failed match means an
 * occupied chunk falls within the requested run. The search resumes one
 * position past the lowest occupied chunk -- the earliest start that no longer
 * overlaps it. skip_map maps the isolated lowest set bit 1<<p to p + 1, so the
 * advance is in 1..8; the [0] entry (no occupied chunk in the low byte) gives a
 * full 8-bit step.
 */
static inline size_t bitmap_skip_eight(uint64_t window)
{
	static const uint8_t skip_map[] = {
		[0] = 8,         [1ULL << 0] = 1, [1ULL << 1] = 2,
		[1ULL << 2] = 3, [1ULL << 3] = 4, [1ULL << 4] = 5,
		[1ULL << 5] = 6, [1ULL << 6] = 7, [1ULL << 7] = 8,
	};
	uint64_t least_significant_bit;

	/* Isolate the lowest set bit, i.e. the lowest occupied chunk. */
	least_significant_bit = window & -window;

	/* Mask to the low byte to keep the table index in range. */
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

	if (hint >= total_bits)
		return SIZE_MAX;

	while (i <= total_bits - requested) {
		word_index = i / BITS_PER_WORD;
		offset     = i % BITS_PER_WORD;

		/* A fully-occupied aligned word holds no run: skip it whole. */
		if (offset == 0 && bitmap[word_index] == UINT64_MAX) {
			i += BITS_PER_WORD;
			continue;
		}

		/**
		 * Build the `requested`-wide window starting at bit i. Shift
		 * the current word down so i lands at bit 0; if the run would
		 * spill past this word's top, splice in the low bits of the
		 * next word.
		 */
		window = bitmap[word_index] >> offset;
		if (offset + requested > BITS_PER_WORD && word_index < size - 1)
			window |= bitmap[word_index + 1]
				  << (BITS_PER_WORD - offset);

		/* Low `requested` bits all clear means a free run starts at i.
		 */
		if ((window & mask) == 0)
			return i;

		/* Otherwise jump past the obstructing occupied chunk. */
		i += bitmap_skip_eight(window);
	}

	return SIZE_MAX;
}
