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

#include <stdlib.h>
#include <assert.h>

#include "bit_vector.h"
#include "sys_public.h"

const struct bit_vec bvec_empty = { 0 };

static void static_assert_bit_vec(void)
{
	ds_StaticAssert(BIT_VEC_BLOCK_SIZE == 64, "must be a power of two");
}

struct bit_vec bit_vec_alloc(struct arena *mem, const u64 bit_count, const u64 clear_bit, const u32 growable)
{
	ds_AssertString(bit_count >= 1 && clear_bit <= 1, "invalid bit_vec_alloc bit count or clear bit value");
	ds_Assert(!(mem && growable));

	const u64 mod = (bit_count % BIT_VEC_BLOCK_SIZE);
	const u64 bit_count_req = (mod) ? bit_count + (BIT_VEC_BLOCK_SIZE - mod) : bit_count; 

	struct bit_vec bvec = 
	{ 
		.block_count = bit_count_req / BIT_VEC_BLOCK_SIZE,
		.bit_count = bit_count_req,
		.growable = growable,
	};

	bvec.bits = (mem)
		? arena_push(mem, bvec.block_count * sizeof(u64))
		: malloc(bvec.block_count * sizeof(u64));

	if (bvec.bits == NULL)
	{
		bvec = (struct bit_vec) { .block_count = 0, .bit_count = 0, .bits = NULL, .growable = 0 };
	}
	else
	{
		for (u64 block = 0; block < bvec.block_count; ++block) 
		{
			bvec.bits[block] = U64_MAX * clear_bit;
		}
	}

	return bvec;
}

void bit_vec_free(struct bit_vec *bvec)
{
	free(bvec->bits);
}

void bit_vec_increase_size(struct bit_vec *bvec, const u64 bit_count, const u64 clear_bit)
{
	ds_Assert(bvec->bit_count < bit_count);
	ds_Assert(bvec->growable);

	const u64 new_blocks_start = bvec->block_count;
	const u64 mod = (bit_count % BIT_VEC_BLOCK_SIZE);
	bvec->bit_count = (mod) ? bit_count + (BIT_VEC_BLOCK_SIZE - mod) : bit_count; 
	bvec->block_count = bit_count / BIT_VEC_BLOCK_SIZE;
	bvec->bits = realloc(bvec->bits, (bvec->block_count * BIT_VEC_BLOCK_SIZE) / 8);

	if (!bvec->bits)
	{
		log_string(T_SYSTEM, S_FATAL, "Failed on reallocation in bit_vec_increase_size, exiting");
		fatal_cleanup_and_exit(0);
	}

	for (u64 i = new_blocks_start; i < bvec->block_count; ++i)
	{
		bvec->bits[i] = U64_MAX * clear_bit; 
	}
}

uint8_t bit_vec_get_bit(const struct bit_vec* bvec, const u64 bit)
{
	ds_Assert(bit < bvec->bit_count);

	const u64 block = bit / BIT_VEC_BLOCK_SIZE;
	const u64 block_bit = bit % BIT_VEC_BLOCK_SIZE;

	return (bvec->bits[block] >> block_bit) & 0x1;
}

void bit_vec_set_bit(const struct bit_vec* bvec, const u64 bit, const u64 bit_value)
{
	ds_Assert(bit < bvec->bit_count && bit_value <= 1);

	const u64 block = bit / BIT_VEC_BLOCK_SIZE;
	const u64 block_bit = bit % BIT_VEC_BLOCK_SIZE;

	/* Get all bits in block but set wanted bit to zero */
	const u64 mask = ~((u64) 0x1 << block_bit); 
	bvec->bits[block] = (bvec->bits[block] & mask) | (bit_value << block_bit);
}

void bit_vec_clear(struct bit_vec* bvec, const u64 clear_bit)
{
	for (u64 block = 0; block < bvec->block_count; ++block) 
	{
		bvec->bits[block] = U64_MAX * clear_bit;
	}
}
