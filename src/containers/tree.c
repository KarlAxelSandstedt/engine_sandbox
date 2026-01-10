/*
==========================================================================
    Copyright (C) 2025, 2026 Axel Sandstedt 

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
	tree->root = POOL_NULL;
}

void bt_validate(struct arena *tmp, const struct bt *tree)
{
	if (tree->root == POOL_NULL)
	{
		kas_assert(0 == bt_node_count(tree));
		return;
	}


	arena_push_record(tmp);
	struct bit_vec traversed = bit_vec_alloc(&tmp1, tree->pool.length, 0, NON_GROWABLE);
	struct allocation_array arr = arena_push_aligned_all(mem, sizeof(u32), 4);
	u32 *stack = arr.addr;
	stack[0] = tree->root;
	u32 sc = 1;

	u32 count = 0;
	u32 leaf_count = 0;
	while (sc--)
	{
		count += 1;
		u8 *addr = pool_address(&tree->pool, stack[sc]);
		u32 *alloc = (u32 *) (addr + tree->pool.slot_allocation_offset);
		u32 *p = (u32 *) (addr + tree->parent_offset);
		u32 *l = (u32 *) (addr + tree->left_offset);
		u32 *r = (u32 *) (addr + tree->right_offset);

		kas_assert((*alloc) >> 31);
		kas_assert(bit_vec_get_bit(&traversed, stack[sc]) == 0);
		bit_vec_set_bit(&traversed, stack[sc], 1);

		if ((BT_PARENT_INDEX_MASK & (*p)) != POOL_NULL)
		{
			u8 *parent = pool_address(&tree->pool, BT_PARENT_INDEX_MASK & (*p));
			u32 *p_alloc = (u32 *) (parent + tree->pool.slot_allocation_offset);
			u32 *p_p = (u32 *) (parent + tree->parent_offset);
			u32 *p_l = (u32 *) (parent + tree->left_offset);
			u32 *p_r = (u32 *) (parent + tree->right_offset);

			kas_assert((*p_alloc) >> 31);
			kas_assert(!(BT_PARENT_LEAF_MASK & (*p_p)));
			kas_assert(*p_l == stack[sc] || *p_r == stack[sc]);
		}

		if (BT_PARENT_LEAF_MASK & (*p))
		{
			leaf_count += 1;
		}
		else
		{
			stack[sc + 0] = *l;
			stack[sc + 1] = *r;
			sc += 2;
		}
	};

	kas_assert(count == bt_node_count(tree));

	arena_pop_record(tmp);
}

struct slot bt_node_add(struct bt *tree)
{
	return pool_add(&tree->pool);
}

void bt_node_remove(struct bt *tree, const u32 index)
{
	pool_remove(&tree->pool, BT_PARENT_INDEX_MASK & index);
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

u32 bt_node_count(const struct bt *tree)
{
	kas_assert(tree->pool.count == 0 || (tree->pool_count & 0x1));
	return tree->pool.count;
}

u32 bt_leaf_count(const struct bt *tree)
{
	kas_assert(tree->pool.count == 0 || (tree->pool_count & 0x1));
	return (tree->pool.count)
		? (tree->pool.count >> 1) + 1
		: 0;
}


