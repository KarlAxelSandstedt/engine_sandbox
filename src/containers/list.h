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

#ifndef __KAS_LIST_H__
#define __KAS_LIST_H__

#include "kas_common.h"
#include "allocator.h"

/*
list
==== 
Intrusive linked list for indexed structures. To use a struct as a list node, put
LIST_SLOT_STATE in the structure. It is meant to be used for arrays < U32_MAX 
indices, where all structs are allocated in the same array. 
 */

#define LIST_NULL		U32_MAX

#define LIST_SLOT_STATE			u32 list_slot_state
#define LIST_NEXT(structure_addr)	((structure_addr)->list_slot_state)

struct list
{
	u32 	count;
	u32 	first;
	u32 	last;
	u64 	slot_size;
	u64	slot_state_offset;
};

/* initalize linked list  */
struct list			list_init_interal(const u64 slot_size, const u64 slot_state_offset);
#define list_init(STRUCT)	list_init_interal(sizeof(STRUCT), (u64) &((STRUCT *)0)->list_slot_state)
/* flush list */
void				list_flush(struct list *list);
/* append to list */
void				list_append(struct list *list, void *array, const u32 index);
/* prepend to list */
void				list_prepend(struct list *list, void *array, const u32 index);

#endif
