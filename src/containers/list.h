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
ll
==== 
Intrusive linked list for indexed structures. To use a struct as a list node, put
LL_SLOT_STATE in the structure. It is meant to be used for arrays < U32_MAX 
indices, where all structs are allocated in the same array. 
 */

#define LL_NULL				U32_MAX
#define LL_SLOT_STATE			u32 ll_next
#define LL_NEXT(structure_addr)		((structure_addr)->ll_next)

struct ll
{
	u32 	count;
	u32 	first;
	u32 	last;
	u64 	slot_size;
	u64	slot_state_offset;
};

/* initalize linked list  */
struct ll		ll_init_interal(const u64 slot_size, const u64 slot_state_offset);
#define ll_init(STRUCT)	ll_init_interal(sizeof(STRUCT), (u64) &((STRUCT *)0)->ll_next)
/* flush list */
void			ll_flush(struct ll *ll);
/* append to list */
void			ll_append(struct ll *ll, void *array, const u32 index);
/* prepend to list */
void			ll_prepend(struct ll *ll, void *array, const u32 index);

/*
dll
==== 
Intrusive doubly linked list for indexed structures. To use a struct as a list node,
put DLL_SLOT_STATE in the structure. It is meant to be used for arrays < U32_MAX 
indices, where all structs are allocated in the same array. 
 */

#define DLL_NULL			U32_MAX
#define DLL_SLOT_STATE			u32 dll_prev;			\
                       			u32 dll_next			

#define DLL_PREV(structure_addr)	((structure_addr)->dll_prev)
#define DLL_NEXT(structure_addr)	((structure_addr)->dll_next)

struct dll
{
	u32 	count;
	u32 	first;
	u32 	last;
	u64 	slot_size;
	u64	prev_offset;
	u64	next_offset;
};

/* initalize linked list  */
struct dll		dll_init_interal(const u64 slot_size, const u64 prev_offset, const u64 next_offset);
#define dll_init(STRUCT)dll_init_interal(sizeof(STRUCT), (u64) &((STRUCT *)0)->dll_prev, (u64) &((STRUCT *)0)->dll_next)
/* flush list */
void			dll_flush(struct dll *dll);
/* append to list */
void			dll_append(struct dll *dll, void *array, const u32 index);
/* prepend to list */
void			dll_prepend(struct dll *dll, void *array, const u32 index);
/* remove from list */
void			dll_remove(struct dll *dll, void *array, const u32 index);

#endif
