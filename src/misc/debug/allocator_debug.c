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

#include "allocator_debug.h"
#include "sys_common.h"

#if (defined(DS_ASAN) && defined(DS_ASSERT_DEBUG))

struct allocator_debug_index allocator_debug_index_alloc(const u8 *array, const u32 slot_count, const u64 slot_size, const u64 slot_header_size, const u64 slot_header_offset)
{
	struct allocator_debug_index debug =
	{
		.array = array,
		.poisoned = bit_vec_alloc(NULL, slot_count, 1, 1),
		.slot_size = slot_size,
		.slot_header_size = slot_header_size,
		.slot_header_offset = slot_header_offset,
		.slot_count = slot_count,
		.max_unpoisoned_count = 0,
	};

	ds_assert(debug.slot_count > 0);
	ds_assert(debug.poisoned.bit_count > 0);
	ds_assert(array != NULL);

	ASAN_POISON_MEMORY_REGION(array, slot_count*slot_size);

	return debug;
}

void allocator_debug_index_free(struct allocator_debug_index *debug)
{
	bit_vec_free(&debug->poisoned);
}

void allocator_debug_index_flush(struct allocator_debug_index *debug)
{
	debug->max_unpoisoned_count = 0;	
	bit_vec_clear(&debug->poisoned, 1);
	ASAN_POISON_MEMORY_REGION(debug->array, debug->slot_count*debug->slot_size);
}

void allocator_debug_index_poison(struct allocator_debug_index *debug, const u32 index)
{
	ds_assert(index < debug->slot_count);
	ds_assert(bit_vec_get_bit(&debug->poisoned, index) == 0);

	if (debug->slot_header_offset)
	{
		ASAN_POISON_MEMORY_REGION(debug->array + index*debug->slot_size, debug->slot_header_offset);
		ASAN_POISON_MEMORY_REGION(debug->array + index*debug->slot_size + debug->slot_header_offset + debug->slot_header_size
				, debug->slot_size - debug->slot_header_size - debug->slot_header_offset);
	}
	else
	{
		ASAN_POISON_MEMORY_REGION(debug->array + index*debug->slot_size + debug->slot_header_size, debug->slot_size - debug->slot_header_size);
	}
	bit_vec_set_bit(&debug->poisoned, index, 1);
}

void allocator_debug_index_unpoison(struct allocator_debug_index *debug, const u32 index)
{
	ds_assert(index < debug->slot_count);
	ds_assert(bit_vec_get_bit(&debug->poisoned, index) == 1);

	if (debug->max_unpoisoned_count <= index)
	{
		ds_assert(debug->max_unpoisoned_count == index);
		ASAN_UNPOISON_MEMORY_REGION(debug->array + index*debug->slot_size, debug->slot_size);
		debug->max_unpoisoned_count = index+1;
	}
	else
	{
		if (debug->slot_header_offset)
		{
			ASAN_UNPOISON_MEMORY_REGION(debug->array + index*debug->slot_size, debug->slot_header_offset);
			ASAN_UNPOISON_MEMORY_REGION(debug->array + index*debug->slot_size + debug->slot_header_offset + debug->slot_header_size
					, debug->slot_size - debug->slot_header_size - debug->slot_header_offset);
		}
		else
		{
			ASAN_UNPOISON_MEMORY_REGION(debug->array + index*debug->slot_size + debug->slot_header_size, debug->slot_size - debug->slot_header_size);
		}

	}
	bit_vec_set_bit(&debug->poisoned, index, 0);
}

void allocator_debug_index_alias_and_repoison(struct allocator_debug_index *debug, const u8 *reallocated_array, const u32 new_slot_count)
{
	ds_assert(debug->slot_count <= new_slot_count);
	if (debug->poisoned.bit_count < new_slot_count)
	{
		bit_vec_increase_size(&debug->poisoned, new_slot_count, 1);
	}

	for (u32 i = 0; i < debug->slot_count; ++i)
	{
		if (bit_vec_get_bit(&debug->poisoned, i))
		{
			allocator_debug_index_poison(debug, i);
		}	
	
		ASAN_POISON_MEMORY_REGION(reallocated_array + debug->slot_count*debug->slot_size, (new_slot_count - debug->slot_count)*debug->slot_size);
	}
	debug->array = reallocated_array;
	debug->slot_count = new_slot_count;
}

#endif
