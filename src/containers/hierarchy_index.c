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

#include "hierarchy_index.h"
#include "sys_public.h"

#include <string.h>

struct hierarchy_index *hierarchy_index_alloc(struct arena *mem, const u32 length, const u64 data_size, const u32 growable)
{
	ds_assert(length > 0);

	struct hierarchy_index *hi;

	if (mem)
	{
		arena_push_record(mem);
		hi = arena_push(mem, sizeof(struct hierarchy_index));
		if (hi == NULL || (hi->list = array_list_alloc(mem, length, data_size, growable)) == NULL)
		{	
			arena_pop_record(mem);
			hi = NULL;
		}
	}
	else
	{
		hi = malloc(sizeof(struct hierarchy_index));
		if (hi != NULL)
		{
			hi->list = array_list_alloc(NULL, length, data_size, growable);
			if (hi->list == NULL)
			{
				free(hi);
				hi = NULL;
			}
		} 
	}

	if (hi && hi->list)
	{
		const u32 root_stub = (u32) array_list_reserve_index(hi->list);
		ds_assert(root_stub == HI_ROOT_STUB_INDEX);
		struct hierarchy_index_node *root = array_list_address(hi->list, root_stub);	
		root->parent = HI_NULL_INDEX;
		root->next = HI_NULL_INDEX;
		root->prev = HI_NULL_INDEX;
		root->first = HI_NULL_INDEX;
		root->last = HI_NULL_INDEX;

		const u32 orphan_stub = (u32) array_list_reserve_index(hi->list);
		ds_assert(orphan_stub == HI_ORPHAN_STUB_INDEX);
		struct hierarchy_index_node *orphan = array_list_address(hi->list, orphan_stub);	
		orphan->parent = HI_NULL_INDEX;
		orphan->next = HI_NULL_INDEX;
		orphan->prev = HI_NULL_INDEX;
		orphan->first = HI_NULL_INDEX;
		orphan->last = HI_NULL_INDEX;
	}

	return hi;
}

void hierarchy_index_free(struct hierarchy_index *hi)
{
	array_list_free(hi->list);
	free(hi);
}
void hierarchy_index_flush(struct hierarchy_index *hi)
{
	array_list_flush(hi->list);
	const u32 root_stub = (u32) array_list_reserve_index(hi->list);
	ds_assert(root_stub == HI_ROOT_STUB_INDEX);
	struct hierarchy_index_node *root = array_list_address(hi->list, root_stub);
	root->parent = HI_NULL_INDEX;
	root->next = HI_NULL_INDEX;
	root->prev = HI_NULL_INDEX;
	root->first = HI_NULL_INDEX;
	root->last = HI_NULL_INDEX;
}

struct slot hierarchy_index_add(struct hierarchy_index *hi, const u32 parent_index)
{
	ds_assert(parent_index <= hi->list->max_count);

	const u32 new_index = (u32) array_list_reserve_index(hi->list);
	if (new_index == U32_MAX)
	{
		return (struct slot) { .index = 0, .address = NULL };
	}

	struct hierarchy_index_node *parent = array_list_address(hi->list, parent_index);
	struct hierarchy_index_node *new_node = array_list_address(hi->list, new_index);

	parent->child_count += 1;
	new_node->child_count = 0;

	new_node->parent = parent_index;
	new_node->prev = parent->last;
	new_node->next = HI_NULL_INDEX;
	new_node->first = HI_NULL_INDEX;
	new_node->last = HI_NULL_INDEX;
	if (parent->last)
	{
		struct hierarchy_index_node *prev = array_list_address(hi->list, parent->last);
		ds_assert(prev->parent == parent_index);
		ds_assert(prev->next == HI_NULL_INDEX);
		parent->last = new_index;
		prev->next = new_index;
	}
	else
	{
		parent->first = new_index;
		parent->last = new_index;
	}

	return (struct slot) { .index = new_index, .address = new_node };
}

static void internal_remove_recursive(struct hierarchy_index *hi, struct hierarchy_index_node *root)
{
	if (root->first)
	{
		struct hierarchy_index_node *child = array_list_address(hi->list, root->first);
		internal_remove_recursive(hi, child);	
	}

	if (root->next)
	{
		struct hierarchy_index_node *next = array_list_address(hi->list, root->next);
		internal_remove_recursive(hi, next);	
	}

	array_list_remove(hi->list, root);
}

static void internal_hierarchy_index_remove_sub_hierarchy_recursive(struct hierarchy_index *hi, struct hierarchy_index_node *root)
{
	if (root->first)
	{
		struct hierarchy_index_node *child = array_list_address(hi->list, root->first);
		internal_remove_recursive(hi, child);	
	}

	if (root->next)
	{
		struct hierarchy_index_node *next = array_list_address(hi->list, root->next);
		internal_remove_recursive(hi, next);	
	}
}

void hierarchy_index_remove(struct arena *tmp, struct hierarchy_index *hi, const u32 node_index)
{
	ds_assert(0 < node_index && node_index <= hi->list->max_count);

	struct hierarchy_index_node *node = array_list_address(hi->list, node_index);
	arena_push_record(tmp);
	/* remove any nodes it the node's sub-hierarchy */
	if (node->first)
	{
		u32 *stack = arena_push(tmp, hi->list->max_count * sizeof(u32));
		if (stack)
		{
			u32 sc = 1;
			stack[0] = node->first;
			while (sc--)
			{
				const u32 sub_index = stack[sc];
				const struct hierarchy_index_node *sub_node = array_list_address(hi->list, sub_index);
				if (sub_node->first)
				{
					stack[sc++] = sub_node->first;
				}

				if (sub_node->next)
				{
					stack[sc++] = sub_node->next;
				}

				array_list_remove_index(hi->list, sub_index);
			}
		}
		else
		{
			internal_hierarchy_index_remove_sub_hierarchy_recursive(hi, node);
		}
	}
	arena_pop_record(tmp);
	
	/* node is not a first or last child of its parent */
	if (node->prev && node->next)
	{
		struct hierarchy_index_node *prev = array_list_address(hi->list, node->prev);
		struct hierarchy_index_node *next = array_list_address(hi->list, node->next);
		prev->next = node->next;
		next->prev = node->prev;
	}
	else
	{
		struct hierarchy_index_node *parent = array_list_address(hi->list, node->parent);
		parent->child_count -= 1;
		/* node is an only child */
		if (parent->first == parent->last)
		{
			parent->first = HI_NULL_INDEX;
			parent->last = HI_NULL_INDEX;
		}
		else if (parent->first == node_index)
		{
			parent->first = node->next;
			struct hierarchy_index_node *next = array_list_address(hi->list, node->next);
			next->prev = HI_NULL_INDEX;
			ds_assert(next->parent == node->parent);
		}
		else
		{
			ds_assert(parent->last == node_index);
			parent->last = node->prev;
			struct hierarchy_index_node *prev = array_list_address(hi->list, node->prev);
			ds_assert(prev->next == node_index);
			prev->next = HI_NULL_INDEX;
		}
	}

	array_list_remove_index(hi->list, node_index);
}

void hierarchy_index_adopt_node_exclusive(struct hierarchy_index *hi, const u32 node_index, const u32 new_parent_index)
{
	ds_assert(new_parent_index <= hi->list->max_count);

	struct hierarchy_index_node *new_parent = array_list_address(hi->list, new_parent_index);
	struct hierarchy_index_node *node = array_list_address(hi->list, node_index);
	struct hierarchy_index_node *old_parent = array_list_address(hi->list, node->parent);
	struct hierarchy_index_node *next = array_list_address(hi->list, node->next);
	struct hierarchy_index_node *prev = array_list_address(hi->list, node->prev);
	struct hierarchy_index_node *child;

	old_parent->child_count += node->child_count - 1;
	if (old_parent->first == old_parent->last)
	{
		next->prev = node->prev;
		prev->next = node->next;
		old_parent->first = node->first;
		old_parent->last = node->last;
	}
	else if (old_parent->first == node_index)
	{
		next->prev = node->last;
		if (node->first)
		{
			old_parent->first = node->first;
			child = array_list_address(hi->list, node->last);
			child->next = node->next;
		}
		else
		{
			old_parent->first = node->next;
		}
	}
	else if (old_parent->last == node_index)
	{
		prev->next = node->first;
		if (node->last)
		{
			old_parent->last = node->last;
			child = array_list_address(hi->list, node->first);
			child->prev = node->prev;
		}
		else
		{
			old_parent->last = node->prev;
		}
	}
	else
	{
		if (node->first)
		{
			prev->next = node->first;
			next->prev = node->last;
			child = array_list_address(hi->list, node->first);
			child->prev = node->prev;
			child = array_list_address(hi->list, node->last);
			child->next = node->next;
		}
		else
		{
			next->prev = node->prev;
			prev->next = node->next;
		}
	}

	for (u32 i = node->first; i != HI_NULL_INDEX; i = child->next)
	{
		child = array_list_address(hi->list, i);
		child->parent = node->parent;
	}

	new_parent->child_count += 1;
	node->child_count = 0;

	node->parent = new_parent_index;
	node->prev = new_parent->last;
	node->next = HI_NULL_INDEX;
	node->first = HI_NULL_INDEX;
	node->last = HI_NULL_INDEX;
	if (new_parent->last)
	{
		prev = array_list_address(hi->list, new_parent->last);
		ds_assert(prev->parent == new_parent_index);
		ds_assert(prev->next == HI_NULL_INDEX);
		new_parent->last = node_index;
		prev->next = node_index;
	}
	else
	{
		new_parent->first = node_index;
		new_parent->last = node_index;
	}
}

void hierarchy_index_adopt_node(struct hierarchy_index *hi, const u32 node_index, const u32 new_parent_index)
{
	ds_assert(new_parent_index <= hi->list->max_count);

	struct hierarchy_index_node *new_parent = array_list_address(hi->list, new_parent_index);
	struct hierarchy_index_node *node = array_list_address(hi->list, node_index);
	struct hierarchy_index_node *old_parent = array_list_address(hi->list, node->parent);
	struct hierarchy_index_node *next = array_list_address(hi->list, node->next);
	struct hierarchy_index_node *prev = array_list_address(hi->list, node->prev);

	old_parent->child_count -= 1;
	next->prev = node->prev;
	prev->next = node->next;

	if (old_parent->first == old_parent->last)
	{
		old_parent->first = HI_NULL_INDEX;
		old_parent->last = HI_NULL_INDEX;
	}
	else if (old_parent->first == node_index)
	{
		old_parent->first = node->next;
	}
	else if (old_parent->last == node_index)
	{
		old_parent->last = node->prev;
	}

	new_parent->child_count += 1;

	node->parent = new_parent_index;
	node->prev = new_parent->last;
	node->next = HI_NULL_INDEX;
	if (new_parent->last)
	{
		prev = array_list_address(hi->list, new_parent->last);
		ds_assert(prev->parent == new_parent_index);
		ds_assert(prev->next == HI_NULL_INDEX);
		new_parent->last = node_index;
		prev->next = node_index;
	}
	else
	{
		new_parent->first = node_index;
		new_parent->last = node_index;
	}
}

void hierarchy_index_apply_custom_free_and_remove(struct arena *tmp, struct hierarchy_index *hi, const u32 node_index, void (*custom_free)(const struct hierarchy_index *hi, const u32 index, void *data), void *data)
{
	ds_assert(0 < node_index && node_index <= hi->list->max_count);

	struct hierarchy_index_node *node = array_list_address(hi->list, node_index);
	arena_push_record(tmp);
	/* remove any nodes it the node's sub-hierarchy */
	if (node->first)
	{
		u32 *stack = arena_push(tmp, hi->list->max_count * sizeof(u32));
		if (stack)
		{
			u32 sc = 1;
			stack[0] = node->first;
			while (sc)
			{
				const u32 sub_index = stack[--sc];
				const struct hierarchy_index_node *sub_node = array_list_address(hi->list, sub_index);
				if (sub_node->first)
				{
					stack[sc++] = sub_node->first;
				}

				if (sub_node->next)
				{
					stack[sc++] = sub_node->next;
				}

				custom_free(hi, sub_index, data);
				array_list_remove_index(hi->list, sub_index);
			}
		}
		else
		{
			ds_assert_string(0, "increase arena mem size");
		}
	}
	arena_pop_record(tmp);
	
	struct hierarchy_index_node *parent = array_list_address(hi->list, node->parent);
	parent->child_count -= 1;

	struct hierarchy_index_node *prev = array_list_address(hi->list, node->prev);
	struct hierarchy_index_node *next = array_list_address(hi->list, node->next);
	ds_assert(node->next == HI_NULL_INDEX || next->prev == node_index);
	ds_assert(node->prev == HI_NULL_INDEX || prev->next == node_index);
	ds_assert(node->next == HI_NULL_INDEX || next->parent == node->parent);
	ds_assert(node->prev == HI_NULL_INDEX || prev->parent == node->parent);
	prev->next = node->next;
	next->prev = node->prev;

	/* node is an only child */
	if (parent->first == parent->last)
	{
		parent->first = HI_NULL_INDEX;
		parent->last = HI_NULL_INDEX;
	}
	else if (parent->first == node_index)
	{
		parent->first = node->next;
	}
	else if (parent->last == node_index)
	{
		parent->last = node->prev;
	}

	custom_free(hi, node_index, data);
	array_list_remove_index(hi->list, node_index);
}

void *hierarchy_index_address(const struct hierarchy_index *hi, const u32 node_index)
{
	ds_assert(node_index <= hi->list->max_count);
	return array_list_address(hi->list, node_index);
}

struct hierarchy_index_iterator	hierarchy_index_iterator_init(struct arena *mem, struct hierarchy_index *hi, const u32 root)
{
	ds_assert(mem);
	arena_push_record(mem);

	struct hierarchy_index_iterator it;
	it.hi = hi;
	it.mem = mem,
	it.forced_malloc = 0;

	struct allocation_array alloc = arena_push_aligned_all(mem, sizeof(u32), sizeof(u32));
	it.stack_len = alloc.len;
	it.stack = alloc.addr;

	if (it.stack == NULL)
	{
		it.forced_malloc = 1;
		it.stack_len = 64;
		it.stack = malloc(it.stack_len * sizeof(u32));	
	}

	ds_assert(root != HI_NULL_INDEX);
	it.stack[0] = HI_NULL_INDEX;
	it.stack[1] = root;	
	it.count = 1;

	return it;
}

void hierarchy_index_iterator_release(struct hierarchy_index_iterator *it)
{
	if (it->forced_malloc)
	{
		free(it->stack);
	}
	arena_pop_record(it->mem);
}

u32 hierarchy_index_iterator_peek(struct hierarchy_index_iterator *it)
{
	ds_assert(it->count);
	return it->stack[it->count];
}

u32 hierarchy_index_iterator_next_df(struct hierarchy_index_iterator *it)
{
	ds_assert(it->count);
	const u32 next = it->stack[it->count];

	struct hierarchy_index_node *node = hierarchy_index_address(it->hi, next);
	u32 push[2];
	u64 push_count = 0;

	if (node->next)
	{
		push[push_count++] = node->next;
	}
	
	if (node->first)
	{
		push[push_count++] = node->first;
	}

	if (push_count == 2)
	{
		/* count doesnt take initial stub into account {  (1 + it->count - 1 + push_count) } */
		if (it->count + push_count > it->stack_len)
		{
			it->forced_malloc = 1;
			it->stack_len += 64;
			u32 *new_arr = malloc(it->stack_len * sizeof(u32));
			memcpy(new_arr, it->stack, it->count * sizeof(u32));
			it->stack = new_arr;
		}

		it->stack[it->count + 0] = push[0];
		it->stack[it->count + 1] = push[1];
	}
	else if (push_count == 1)
	{
		it->stack[it->count + 0] = push[0];
	}

	it->count = it->count + push_count -1;

	return next;
}

void hierarchy_index_iterator_skip(struct hierarchy_index_iterator *it)
{
	ds_assert(it->count);
	const u32 next = it->stack[it->count];
	/* if it->count */
	struct hierarchy_index_node *node = hierarchy_index_address(it->hi, next);
	if (node->next)
	{
		it->stack[it->count] = node->next;
	}
	else
	{
		it->count -= 1;
	}
}
