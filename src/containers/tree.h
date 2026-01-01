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

#ifndef __PD_TREE_H__
#define __PD_TREE_H__

#include "kas_common.h"
#include "sys_public.h"
#include "allocator.h"

/*
bt
==
Intrusive general binary tree for indexed structures. With general we mean that 
we do not enforce a specific relation between parent and child indices, a property
that can be exploited in certain tree based data structures.
*/

#define BT_NULL		U32_MAX
#define	BT_SLOT_STATE	u32	bt_parent;	\
			u32	bt_left;	\
			u32	bt_right;	\
			POOL_SLOT_STATE

struct bt
{
	struct pool	pool;
	u64		parent_offset;
	u64		left_offset;
	u64		right_offset;
	u32		heap_allocated;		
	u32		root;
};

/* allocate tree node memory. If mem != NULL, the tree cannot be growable. If mem == NULL, heap allocation is made */
struct bt	bt_alloc_internal(struct arena *mem, 
				const u32 initial_length,
				const u64 slot_size, 
				const u64 parent_offset, 
				const u64 left_offset, 
				const u64 right_offset, 
				const u64 pool_slot_offset,
				const u32 growable);
#define 	bt_alloc(mem, initial_length, STRUCT, growable)	bt_alloc_internal(mem,				\
									initial_length,				\
									sizeof(STRUCT),				\
									(u64) &((STRUCT *)0)->bt_parent,	\
									(u64) &((STRUCT *)0)->bt_left,		\
									(u64) &((STRUCT *)0)->bt_right,		\
									(u64) &((STRUCT *)0)->slot_allocation_state, \
									growable)
/* free allocated resources */
void		bt_dealloc(struct bt *tree);
/* flush / reset binary tree  */
void 		bt_flush(struct bt *tree);
/* allocate node. On Failure, return empty slot. */
struct slot 	bt_node_add(struct bt *tree);

#endif
