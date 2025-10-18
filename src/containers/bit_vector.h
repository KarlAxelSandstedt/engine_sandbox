/*
==========================================================================
    Copyright (C) 2025 Axel Sandstedt 

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
==========================================================================
*/

#ifndef __BIT_VECTOR_H__
#define __BIT_VECTOR_H__

#include <stdint.h>

#include "kas_common.h"
#include "allocator.h"

#define BIT_VEC_BLOCK_SIZE 	64
#define BIT_VEC_GROWABLE	1
	
/* bit indexing starts at 0, up to bit_count-1. */
struct bit_vec {
	u64 	block_count;
	u64 	bit_count;
	u64 * 	bits;
	u32 	growable;
};

/* return a bit vector with bit_count >= bit_count, with all bits cleared to clear_bit. On failure, the zero vector is returned. */
struct bit_vec	bit_vec_alloc(struct arena *mem, const u64 bit_count, const u64 clear_bit, const u32 growable);
/* free the vectors resources.  */
void 		bit_vec_free(struct bit_vec *bvec);
/* increase the bit vector size and clear any newly allocated blocks with the clear bit. */
void 		bit_vec_increase_size(struct bit_vec *bvec, const u64 bit_count, const u64 clear_bit);
/* Clear the bit vector with the given bit value  */
void 		bit_vec_clear(struct bit_vec* bvec, const u64 clear_bit);
/* return the bit value of the given bit  */
uint8_t 	bit_vec_get_bit(const struct bit_vec* bvec, const u64 bit);
/* set the bit value of the given bit */
void 		bit_vec_set_bit(const struct bit_vec* bvec, const u64 bit, const u64 bit_value);

#endif
