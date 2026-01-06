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
		.root = POOL_NULL,
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

struct slot bt_node_add_root(struct bt *tree)
{
	struct slot slot = pool_add(&tree->pool);
	if (slot.index != POOL_NULL)
	{
		kas_assert(tree->root == POOL_NULL);
		tree->root = slot.index;
		u32 *parent = (u32 *) (((u8 *) slot.address) + tree->parent_offset);	
		*parent = BT_PARENT_LEAF_MASK | POOL_NULL;
	}
	return slot;
}


void bt_node_add_children(struct bt *tree, struct slot *left, struct slot *right, const u32 parent)
{
	*left = pool_add(&tree->pool);
	*right = pool_add(&tree->pool);

	if (!right->address)
	{
		if (left->address)
		{
			pool_remove(&tree->pool, left->index);
			*left = empty_slot;
		}
	}
	else
	{
		u32 *bt_parent = (u32 *)(((u8 *) tree->pool.buf) + tree->pool.slot_size*parent + tree->parent_offset);	
		u32 *bt_left   = (u32 *)(((u8 *) tree->pool.buf) + tree->pool.slot_size*parent + tree->left_offset);	
		u32 *bt_right  = (u32 *)(((u8 *) tree->pool.buf) + tree->pool.slot_size*parent + tree->right_offset);	

		kas_assert(*bt_parent & BT_PARENT_LEAF_MASK);
		*bt_parent &= ~BT_PARENT_LEAF_MASK;
		*bt_left = left->index;
		*bt_right = right->index;

		bt_parent = (u32 *)(((u8 *) left->address) + tree->parent_offset);	
		*bt_parent = BT_PARENT_LEAF_MASK | parent;

		bt_parent = (u32 *)(((u8 *) right->address) + tree->parent_offset);	
		*bt_parent = BT_PARENT_LEAF_MASK | parent;
	}
}
