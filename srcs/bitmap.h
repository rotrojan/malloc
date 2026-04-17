/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   bitmap.h                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: rotrojan <rotrojan@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/12/16 19:52:54 by rotrojan          #+#    #+#             */
/*   Updated: 2026/04/17 18:48:51 by rotrojan         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef BITMAP_H
#define BITMAP_H

#include <stddef.h> /* for size_t*/
#include <stdint.h> /* for uint64_t */

/**
 * @brief Finds the first occurrence of a consecutive sequence of zero bits.
 *
 * This function scans a bitmap (array of uint64_t) to find a range of
 * @p requested bits that are all set to 0. It supports a @p hint to
 * start the search from a specific bit offset and uses an optimized
 * sliding window approach.
 *
 * @param bitmap    Pointer to the array of 64-bit words representing the
 *                  bitmap.
 * @param size      The number of 64-bit words in the bitmap.
 * @param requested The number of consecutive zeros to find (Must be 1-8).
 * @param hint      The bit index at which to start the search.
 *
 * @return The bit index of the start of the first suitable range found,
 * or @c SIZE_MAX if no such range exists or if the hint is out of bounds.
 *
 * @note This function is optimized to skip full words and uses a skip-ahead
 * mechanism via bitmap_skip_eight() to accelerate the search.
 * @warning The function asserts that @p requested is between 1 and 8 bits.
 */
size_t bitmap_find_consecutive_zeros(uint64_t *bitmap, size_t size,
				     size_t requested, size_t hint);

/**
 * @brief Sets a range of bits in the bitmap to 1.
 *
 * This function marks a specific sequence of bits as "set" (1). If the
 * requested range spans across a 64-bit word boundary, the function
 * automatically handles the bitwise logic for both the current and the
 * subsequent word.
 *
 * @param bitmap Pointer to the array of 64-bit words.
 * @param index  The starting bit index where the range begins.
 * @param range  The number of bits to set (Must be 1-8).
 *
 * @note This operation is performed using a bitwise OR with a generated mask.
 * @warning Asserts if @p range is 0 or exceeds 8 bits.
 * @see bitmap_clear_range
 */
void   bitmap_set_range(uint64_t *bitmap, size_t index, size_t range);

/**
 * @brief Clears a range of bits in the bitmap to 0.
 *
 * This function marks a specific sequence of bits as "cleared" (0). If the
 * requested range overlaps a 64-bit word boundary, the operation is
 * split across the primary word and the next word in the @p bitmap array.
 *
 * @param bitmap Pointer to the array of 64-bit words.
 * @param index  The starting bit index where the range begins.
 * @param range  The number of bits to clear (Must be 1-8).
 *
 * @note This operation is performed using a bitwise AND with an inverted mask.
 * @warning Asserts if @p range is 0 or exceeds 8 bits.
 * @see bitmap_set_range
 */
void   bitmap_clear_range(uint64_t *bitmap, size_t index, size_t range);

#endif /* BITMAP_H */
