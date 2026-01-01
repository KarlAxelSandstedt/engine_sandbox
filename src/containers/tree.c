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

#include "tree.h"

struct bt bt_alloc_internal(struct arena *mem, const u32 initial_length, const u64 slot_size, const u64 parent_offset, const u64 left_offset, const u64 right_offset, const u64 pool_slot_offset, const u32 growable)
{
	kas_assert(!growable || !mem);

	struct bt bt =
	{
		.parent_offset = parent_offset,
		.left_offset = left_offset,
		.right_offset = right_offset,
		.heap_allocated = !mem,
		.root = BT_NULL,
		.pool = pool_alloc_internal(mem, initial_length, slot_size, pool_slot_offset, U64_MAX, growable),
	};

	return bt;
}

void bt_dealloc(struct bt *tree)
{
	if (tree->heap_allocated)
	{
		pool_dealloc(&tree->pool);
	}
}

void bt_flush(struct bt *tree)
{
	pool_flush(&tree->pool);
}

struct slot bt_node_add(struct bt *tree)
{
	return pool_add(&tree->pool);
}
