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

#include <string.h>

#include "net_list.h"
#include "sys_public.h"

void static_assert_net_list_node(void)
{
	kas_static_assert(sizeof(struct net_list_node) == 4 * sizeof(u32), "Expected net list node header size");
}

struct net_list *net_list_alloc(struct arena *mem, 
		const u32 length, 
		const u64 data_size, 
		const u32 growable, 
		u32 (*index_in_previous_node)(struct net_list *, void **, const void *, const u32),
	       	u32 (*index_in_next_node)(struct net_list *, void **, const void *, const u32))
{
	struct net_list *net = NULL;
	void *data = NULL;
	if (mem)
	{
		kas_assert(growable == 0);
		net = arena_push(mem, sizeof(struct net_list));
		data = arena_push(mem, (2 + length) * data_size);
	}
	else
	{
		net = malloc(sizeof(struct net_list));
		data = malloc((2 + length) * data_size);
	}

	if (!net || !data)
	{
		log(T_SYSTEM, S_FATAL, "%s:%u - out of memory in function %s.", __FILE__, __LINE__, __func__); 
		fatal_cleanup_and_exit(kas_thread_self_tid());
	}

	net->data_size = data_size;
	net->data = data;
	net->length = length + 2;
	net->max_count = 2;
	net->count = 2;
	net->next_free = NET_LIST_NODE_NULL_INDEX;
	net->growable = growable;
	net->index_in_previous_node = index_in_previous_node;
	net->index_in_next_node = index_in_next_node;
	
	ALLOCATOR_DEBUG_INDEX_ALLOC(net, net->data, net->length, net->data_size, sizeof(struct net_list_node));

	struct net_list_node *not_allocated_dummy = (struct net_list_node *) ((u8 *) net->data + net->data_size * 0);
	struct net_list_node *null_dummy = (struct net_list_node *) ((u8 *) net->data + net->data_size * 1);

	ALLOCATOR_DEBUG_INDEX_UNPOISON(net, 0);
	ALLOCATOR_DEBUG_INDEX_UNPOISON(net, 1);

	not_allocated_dummy->prev[0] = NET_LIST_NODE_NULL_INDEX;
	not_allocated_dummy->prev[1] = NET_LIST_NODE_NULL_INDEX;
	not_allocated_dummy->next[0] = NET_LIST_NODE_NULL_INDEX;
	not_allocated_dummy->next[1] = NET_LIST_NODE_NULL_INDEX;

	null_dummy->prev[0] = NET_LIST_NODE_NULL_INDEX;
	null_dummy->prev[1] = NET_LIST_NODE_NULL_INDEX;
	null_dummy->next[0] = NET_LIST_NODE_NULL_INDEX;
	null_dummy->next[1] = NET_LIST_NODE_NULL_INDEX;

	kas_assert(net_list_index(net, not_allocated_dummy) == NET_LIST_NODE_NOT_ALLOCATED_INDEX);
	kas_assert(net_list_index(net, null_dummy) == NET_LIST_NODE_NULL_INDEX);

	return net;
}

void net_list_free(struct net_list *net)
{
	ALLOCATOR_DEBUG_INDEX_FREE(net);
	if (net)
	{
		free(net->data);
	}
	free(net);
}

void net_list_flush(struct net_list *net)
{
	net->max_count = 2;
	net->count = 2;
	net->next_free = NET_LIST_NODE_NULL_INDEX;

	ALLOCATOR_DEBUG_INDEX_FLUSH(net);

	struct net_list_node *not_allocated_dummy = (struct net_list_node *) ((u8 *) net->data + net->data_size * 0);
	struct net_list_node *null_dummy = (struct net_list_node *) ((u8 *) net->data + net->data_size * 1);

	ALLOCATOR_DEBUG_INDEX_UNPOISON(net, 0);
	ALLOCATOR_DEBUG_INDEX_UNPOISON(net, 1);

	not_allocated_dummy->prev[0] = NET_LIST_NODE_NULL_INDEX;
	not_allocated_dummy->prev[1] = NET_LIST_NODE_NULL_INDEX;
	not_allocated_dummy->next[0] = NET_LIST_NODE_NULL_INDEX;
	not_allocated_dummy->next[1] = NET_LIST_NODE_NULL_INDEX;

	null_dummy->prev[0] = NET_LIST_NODE_NULL_INDEX;
	null_dummy->prev[1] = NET_LIST_NODE_NULL_INDEX;
	null_dummy->next[0] = NET_LIST_NODE_NULL_INDEX;
	null_dummy->next[1] = NET_LIST_NODE_NULL_INDEX;
}

u32 net_list_push(struct net_list *list, const void *data_to_copy, const u32 next_0, const u32 next_1)
{
	kas_assert(next_0 != NET_LIST_NODE_NOT_ALLOCATED_INDEX && next_1 !=  NET_LIST_NODE_NOT_ALLOCATED_INDEX);

	u32 node_index = 0;
	if (list->count < list->length)
	{
		if (list->next_free != NET_LIST_NODE_NULL_INDEX)
		{
			node_index = list->next_free;
			struct net_list_node *free = (struct net_list_node *) ((u8 *) list->data + list->data_size * node_index);
			list->next_free = free->chain.next_free;
			kas_assert(!free->chain.allocated);
		}
		else
		{
			kas_assert(list->count == list->max_count);
			node_index = list->max_count;
			list->max_count += 1;
		}
		list->count += 1;
	}
	else if (list->growable)
	{
		kas_assert(list->max_count == list->length);
		node_index = list->max_count;
		list->length *= 2;
		list->max_count += 1;
		list->count += 1;
		list->data = realloc(list->data, list->data_size * list->length);
		if (!list->data)
		{
			log(T_SYSTEM, S_FATAL, "%s:%u - non-growable net_list out of memory %s.", __FILE__, __LINE__, __func__); 
			fatal_cleanup_and_exit(kas_thread_self_tid());
		}
		ALLOCATOR_DEBUG_INDEX_ALIAS_AND_REPOISON(list, list->data, list->length);
	} else
	{
		log(T_SYSTEM, S_FATAL, "%s:%u - non-growable net_list out of memory %s.", __FILE__, __LINE__, __func__); 
		fatal_cleanup_and_exit(kas_thread_self_tid());
	}
			
	ALLOCATOR_DEBUG_INDEX_UNPOISON(list, node_index);
	struct net_list_node *node = (struct net_list_node *) ((u8 *) list->data + list->data_size * node_index);

	node->prev[0] = NET_LIST_NODE_NULL_INDEX;
	node->prev[1] = NET_LIST_NODE_NULL_INDEX;
	node->next[0] = next_0;
	node->next[1] = next_1;

	memcpy(   ((u8 *) node + sizeof(struct net_list_node))
		, ((u8 *) data_to_copy + sizeof(struct net_list_node))
		, list->data_size - sizeof(struct net_list_node));

       	struct net_list_node *node_next_0;
       	struct net_list_node *node_next_1;

	const u32 index_next_0 = list->index_in_next_node(list, (void **) &node_next_0, node, 0);
	const u32 index_next_1 = list->index_in_next_node(list, (void **) &node_next_1, node, 1);

	kas_assert_string(next_0 == NET_LIST_NODE_NULL_INDEX || node_next_0->prev[index_next_0] == NET_LIST_NODE_NULL_INDEX, "either the next node must be the NULL NODE, indicating a list of size 1, or the previous head in the list which should have its previous node as the NULL NODE");
	kas_assert_string(next_1 == NET_LIST_NODE_NULL_INDEX || node_next_1->prev[index_next_1] == NET_LIST_NODE_NULL_INDEX, "either the next node must be the NULL NODE, indicating a list of size 1, or the previous head in the list which should have its previous node as the NULL NODE");
	node_next_0->prev[index_next_0] = node_index;
	node_next_1->prev[index_next_1] = node_index;

	kas_assert(next_0 == NET_LIST_NODE_NULL_INDEX || list->index_in_previous_node(list, (void **) &node, node_next_0, index_next_0) == 0);
	kas_assert(next_1 == NET_LIST_NODE_NULL_INDEX || list->index_in_previous_node(list, (void **) &node, node_next_1, index_next_1) == 1);

	return node_index;
}

void net_list_remove(struct net_list *list, const u32 index)
{
	kas_assert(1 < index && index < list->length);

	struct net_list_node *node = (struct net_list_node *) ((u8 *) list->data + index*list->data_size);
	kas_assert(node->chain.allocated);	

	struct net_list_node *node_prev_0, *node_prev_1, *node_next_0, *node_next_1; 
	const u32 index_prev_0 = list->index_in_previous_node(list, (void **) &node_prev_0, node, 0);
	const u32 index_prev_1 = list->index_in_previous_node(list, (void **) &node_prev_1, node, 1);
	const u32 index_next_0 = list->index_in_next_node(list, (void **) &node_next_0, node, 0);
	const u32 index_next_1 = list->index_in_next_node(list, (void **) &node_next_1, node, 1);

	kas_assert(node->prev[0] == NET_LIST_NODE_NULL_INDEX || node_prev_0->next[index_prev_0] == index);
	kas_assert(node->prev[1] == NET_LIST_NODE_NULL_INDEX || node_prev_1->next[index_prev_1] == index);
	kas_assert(node->next[0] == NET_LIST_NODE_NULL_INDEX || node_next_0->prev[index_next_0] == index);
	kas_assert(node->next[1] == NET_LIST_NODE_NULL_INDEX || node_next_1->prev[index_next_1] == index);

	node_prev_0->next[index_prev_0] = node->next[0];
	node_prev_1->next[index_prev_1] = node->next[1];
	node_next_0->prev[index_next_0] = node->prev[0];
	node_next_1->prev[index_next_1] = node->prev[1];

	node->chain.allocated = NET_LIST_NODE_NOT_ALLOCATED_INDEX;
	node->chain.next_free = list->next_free;
	list->next_free = index;
	list->count -= 1;
	ALLOCATOR_DEBUG_INDEX_POISON(list, index);
}

void *net_list_address(struct net_list *list, const u32 index)
{
	return (u8 *) list->data + index*list->data_size;
}

u32 net_list_index(struct net_list *list, const void *address)
{
	kas_assert((u64) address >= (u64) list->data);
	kas_assert((u64) address < (u64) list->data + list->length * list->data_size);
	kas_assert((u64) ((u64) address - (u64) list->data) % list->data_size == 0); 
	return (u32) (((u64) address - (u64) list->data) / list->data_size);
}



