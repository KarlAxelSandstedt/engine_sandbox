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

#ifndef __ARRAY_LIST_H__
#define __ARRAY_LIST_H__

#include "ds_common.h"
#include "allocator.h"
#include "allocator_debug.h"

/*
 * array_list: non-intrusive array list for allocating values in an array
 */
struct array_list
{
	u32 length;	/* array length */
	u32 max_count;	/* max count used over the object's lifetime */
	u32 count;	/* current count of occupied slots */
	u32 data_size;
	u32 slot_size;	/* slot size, minimum size 8 so we can store free list chain of pointers */
	u8 *slot;
	u32 free_index;
	u32 growable;
};

#define ARRAY_LIST_GROWABLE 1

/* alloc array_list (on arena if non-NULL and non-growable), returns NULL on failure */
struct array_list *	array_list_alloc(struct arena *mem, const u32 length, const u64 data_size, const u32 growable);
/* alloc array_list (on arena if not NULL) and allocate growable memory on heap, returns NULL on failure */
void 			array_list_free(struct array_list *list);
/* flush any alloctions within the list, reseting it. */
void			array_list_flush(struct array_list *list);
/* return allocation slot of possibly new allocation  */
struct slot	array_list_add(struct array_list *list);
/* reserve slot = returned address, or NULL if no space */
void *			array_list_reserve(struct array_list *list);
/* reserve slot = returned index to address, or U32_MAX if no space */
u32			array_list_reserve_index(struct array_list *list);
/* add address to free chain */
void 			array_list_remove(struct array_list *list, void *addr);
/* add slot[index] to free chain  */
void 			array_list_remove_index(struct array_list *list, const u32 index);
/* get address at slot[index] */
void *			array_list_address(struct array_list *list, const u32 index);
/* get index of address */
u32			array_list_index(struct array_list *list, const void *address);

/* array_list_intrusive_node: intrusive header to be put in any struct using the intrusive array list */
struct array_list_intrusive_node
{
	u32 allocated;
	union
	{
		u32 next_free;	
		u32 next;
		u32 prev;
	};
};

/*
 * array_list_intrusive: intrusive array list state for allocating values in an array.
 * 	Since any structure using the list will be known a head of time, 
 */
struct array_list_intrusive
{
	u32 data_size;	/* size of list struct containing node header */
	u8 *data;
	u32 length;	/* array length */
	u32 max_count;	/* max count used over the object's lifetime */
	u32 count;	/* current count of occupied slots */
	u32 free_index;
	u32 growable;
};

/* alloc array_list (on arena if non-NULL and non-growable), returns NULL on failure */
struct array_list_intrusive *	array_list_intrusive_alloc(struct arena *mem, const u32 length, const u64 data_size, const u32 growable);
/* alloc array_list (on arena if n_intrusiveot NULL) and allocate growable memory on heap, returns NULL on failure */
void 				array_list_intrusive_free(struct array_list_intrusive *list);
/* flush any alloctions within the list, reseting it. */
void				array_list_intrusive_flush(struct array_list_intrusive *list);
/* return allocation slot of possibly new allocation  */
struct slot			array_list_intrusive_add(struct array_list_intrusive *list);
/* reserve slot = return	ed address_intrusive, or NULL if no space */
void *				array_list_intrusive_reserve(struct array_list_intrusive *list);
/* reserve slot = return	ed index t_intrusiveo address, or U32_MAX if no space */
u32				array_list_intrusive_reserve_index(struct array_list_intrusive *list);
/* add address to free c	hain */
void 				array_list_intrusive_remove(struct array_list_intrusive *list, void *addr);
/* add slot[index] to fr	ee chain  _intrusive*/
void 				array_list_intrusive_remove_index(struct array_list_intrusive *list, const u32 index);
/* get address at slot[i	ndex] */
void *				array_list_intrusive_address(struct array_list_intrusive *list, const u32 index);
/* get index of address 	*/
u32				array_list_intrusive_index(struct array_list_intrusive *list, const void *address);

#endif
