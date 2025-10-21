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

#ifndef __NET_LIST_H__
#define __NET_LIST_H__

#include "kas_common.h"
#include "allocator.h"
#include "allocator_debug.h"

/* Growable boolean */
#define NET_LIST_GROWABLE			1
/* reserved index for representing an unallocated node */
#define NET_LIST_NODE_NOT_ALLOCATED_INDEX	0
/* index reserved to indicated no more nodes at a prev/next index */
#define NET_LIST_NODE_NULL_INDEX		1

/*
 * net_list : A set of intertwined lists. Each node represents a node in two lists, and adding or removing a net_node
 * affects two lists simultaneously. It is up to the user of the data structure to construct identifying data in their
 * own structures to identify who/what owns what parts of the node, i.e. which list owns prev_1, next_1 and
 * which list owns prev_2, next_2.
 */
struct net_list
{
	u64	data_size;	/* data size of structure with included node header 				*/
	u8 *	data;		/* address to data 								*/
	u32	length;		/* data[length]									*/
	u32	max_count;	/* max count used over the data structure's life time   			*/
	u32	count;		/* current node count 								*/
	u32	next_free;	/* first free node in the free list, or NULL_INDEX if the free list is empty 	*/
	u32	growable;	/* boolean : if true, then the size of the data structure may grow           	*/

	/* user provided identifier methods: 
	 *
	 * 	cur_index: the index used for the owner of prev[cur_index], next[cur_index]
	 * 	cur_node: [cur_node, cur_index], the node in a specific list we are inspecting
	 * 	prev/next_node: the node in which we wish to identify index owned by the owner of [cur_node, cur_index]
	 * 	returns the index owned by [cur_node, cur_index] in the prev/next index
	 *
	 * When removing a node in the middle of two lists, we must know how to identify the indices of the two
	 * corresponding lists within the prev/next nodes
	 */
	u32	(*index_in_previous_node)(struct net_list *net, void **prev_node, const void *cur_node, const u32 cur_index);
	u32	(*index_in_next_node)(struct net_list *net, void **next_node, const void *cur_node, const u32 cur_index);

	ALLOCATOR_DEBUG_INDEX_STRUCT;
};

/* intrusive net list node header: Should be placed at base of any structure to be used as a net node. */
struct net_list_node
{
	union
	{
		struct
		{
			u32	allocated;	/* if (allocated > 0), prev, next is defined  	*/
			u32	next_free;	/* next free index in chain, or NET_LIST_NODE_NULL_INDEX */
		} chain;
		u32	prev[2];	/* list 0,1 previous node of corresponding list */
	};
	u32 next[2];			/* list 0,1 next node of corresponding list	*/
};

/* allocate net_list memory. If mem != NULL, the list cannot be growable. If mem == NULL, heap allocation is made */
struct net_list *	net_list_alloc(struct arena *mem, 
				const u32 length, 
				const u64 data_size, 
				const u32 growable, 
				u32 (*index_in_previous_node)(struct net_list *, void **, const void *, const u32),
		       		u32 (*index_in_next_node)(struct net_list *, void **, const void *, const u32));
/* free allocated resources */
void			net_list_free(struct net_list *net);
/* flush / reset net_list  */
void			net_list_flush(struct net_list *net);
/* reserve a memory node and return the memory index and set the node's links. next_0 and next_1 MUST always be 
 * the first nodes, or static node references, of the two corresponding lists owning the node */
u32			net_list_push(struct net_list *list, const void *data_to_copy, const u32 next_0, const u32 node_1);
/* free a memory node, updating both lists it is a part of */
void 			net_list_remove(struct net_list *list, const u32 index);
/* get the node address given its index */
void *			net_list_address(struct net_list *list, const u32 index);
/* get the node index given its address  */
u32			net_list_index(struct net_list *list, const void *address);

#endif

