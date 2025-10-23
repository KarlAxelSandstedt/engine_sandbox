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

/*
 * NOTE: (DEPRECATED, use pool allocator instead)
 */

#include <stdlib.h>
#include <string.h>

#include "sys_common.h"
#include "array_list.h"

struct array_list *array_list_alloc(struct arena *mem, const u32 length, const u64 data_size, const u32 growable)
{
	struct array_list *list;

	u32 slot_size = (u32) data_size;
	if (data_size < sizeof(u32))
	{
		slot_size = sizeof(u32);
	}

	if (mem)
	{
		kas_assert(growable == 0);
		arena_push_record(mem);
		list = arena_push(mem, sizeof(struct array_list));
		if (list)
		{
			list->slot = arena_push(mem, length * slot_size);
		}
	}
	else
	{
		list = malloc(sizeof(struct array_list));
		if (list)
		{
			list->slot = malloc(length * slot_size);
		}
	}

	if (list && list->slot)
	{
		ALLOCATOR_DEBUG_INDEX_ALLOC(list, list->slot, length, slot_size, 0, 0);
		list->length = length;
		list->max_count = 0;
		list->count = 0;
		list->data_size = (u32) data_size;
		list->slot_size = slot_size;
		list->free_index = U32_MAX;
		list->growable = growable;
	}
	else
	{
		if (mem)
		{
			arena_pop_record(mem);
		}
		else
		{
			array_list_free(list);
		}
		
		list = NULL;
	}
	
	return list;
}

void array_list_free(struct array_list *list)
{
	if (list)
	{
		ALLOCATOR_DEBUG_INDEX_FREE(list);
		free(list->slot);
	}
	free(list);
}

void array_list_flush(struct array_list *list)
{
	list->max_count = 0;
	list->count = 0;
	list->free_index = U32_MAX;
	ALLOCATOR_DEBUG_INDEX_FLUSH(list);
}

static void internal_array_list_realloc(struct array_list *list)
{
	list->length <<= 1;
	list->slot = realloc(list->slot, list->length * list->slot_size);
	ALLOCATOR_DEBUG_INDEX_ALIAS_AND_REPOISON(list, list->slot, list->length);
}

struct slot array_list_add(struct array_list *list)
{
	struct slot allocation = { .address = NULL, .index = U32_MAX };

	if (list->count < list->length)
	{
		if (list->free_index != U32_MAX)
		{
			ALLOCATOR_DEBUG_INDEX_UNPOISON(list, list->free_index);
			allocation.address = (u8 *) list->slot + list->free_index * list->slot_size;
			allocation.index = list->free_index;
			list->free_index = *((u32 *) allocation.address);
		}
		else
		{
			ALLOCATOR_DEBUG_INDEX_UNPOISON(list, list->max_count);
			allocation.address = (u8 *) list->slot + list->slot_size * list->max_count;
			allocation.index = list->max_count;
			list->max_count += 1;
		}	
		list->count += 1;
	}
	else if (list->growable)
	{
		internal_array_list_realloc(list);
		allocation.address = (u8 *) list->slot + list->slot_size * list->max_count;
		allocation.index = list->max_count;
		list->max_count += 1;
		list->count += 1;
	}

	return allocation;
}

void *array_list_reserve(struct array_list *list)
{
	void *addr = NULL;

	if (list->count < list->length)
	{
		if (list->free_index != U32_MAX)
		{
			ALLOCATOR_DEBUG_INDEX_UNPOISON(list, list->free_index);
			addr = (u8 *) list->slot + list->free_index * list->slot_size;
			list->free_index = *((u32 *) addr);
		}
		else
		{
			ALLOCATOR_DEBUG_INDEX_UNPOISON(list, list->max_count);
			addr = (u8 *) list->slot + list->slot_size * list->max_count;
			list->max_count += 1;
		}	
		list->count += 1;
	}
	else if (list->growable)
	{
		internal_array_list_realloc(list);
		addr = (u8 *) list->slot + list->slot_size * list->max_count;
		list->max_count += 1;
		list->count += 1;
	}

	return addr;
}

u32 array_list_reserve_index(struct array_list *list)
{
	u32 index = list->free_index;

	if (list->count < list->length)
	{
		if (list->free_index != U32_MAX)
		{
			ALLOCATOR_DEBUG_INDEX_UNPOISON(list, list->free_index);
			list->free_index = *(u32 *)((u8 *) list->slot + index * list->slot_size);
		}
		else
		{
			ALLOCATOR_DEBUG_INDEX_UNPOISON(list, list->max_count);
			index = list->max_count;
			list->max_count += 1;
		}	
		list->count += 1;
	}
	else if (list->growable)
	{
		internal_array_list_realloc(list);
		ALLOCATOR_DEBUG_INDEX_UNPOISON(list, list->max_count);
		index = list->max_count;
		list->max_count += 1;
		list->count += 1;
	}

	return index;
}

void array_list_remove(struct array_list *list, void *addr)
{
	kas_assert(addr && (void *) list->slot <= addr && addr < (void *) ((u8 *) list->slot + list->length * list->slot_size));
	kas_assert(((u64) addr - (u64) list->slot) % list->slot_size == 0);
	

	const u32 free_i = list->free_index;
	list->free_index = (u32) (((u64) addr - (u64) list->slot) / list->slot_size);
	*(u32 *) addr = free_i;
	list->count -= 1;
	ALLOCATOR_DEBUG_INDEX_POISON(list, ((u64) addr - (u64) list->slot) / list->slot_size);
}

void array_list_remove_index(struct array_list *list, const u32 index)
{
	kas_assert(index < list->length);
	void *addr = (u8 *) list->slot + index * list->slot_size;

	*(u32 *)((u8 *) list->slot + index * list->slot_size) = list->free_index;
	list->free_index = index;
	list->count -= 1;
	ALLOCATOR_DEBUG_INDEX_POISON(list, index);
}

void *array_list_address(struct array_list *list, const u32 index)
{
	return (u8 *) list->slot + index * list->slot_size;
}

u32 array_list_index(struct array_list *list, const void *address)
{
	kas_assert((u64) address >= (u64) list->slot);
	kas_assert((u64) address < (u64) list->slot + list->length * list->slot_size);
	kas_assert((u64) ((u64) address - (u64) list->slot) % list->slot_size == 0); 
	return (u32) (((u64) address - (u64) list->slot) / list->slot_size);
}

struct array_list_intrusive *array_list_intrusive_alloc(struct arena *mem, const u32 length, const u64 data_size, const u32 growable)
{
	struct array_list_intrusive *list;

	if (mem)
	{
		kas_assert(growable == 0);
		arena_push_record(mem);
		list = arena_push(mem, sizeof(struct array_list_intrusive));
		if (list)
		{
			list->data = arena_push(mem, length*data_size);
		}
	}
	else
	{
		list = malloc(sizeof(struct array_list_intrusive));
		if (list)
		{
			list->data = malloc(length*data_size);
		}
	}

	if (list && list->data)
	{
		ALLOCATOR_DEBUG_INDEX_ALLOC(list, list->data, length, data_size, sizeof(struct array_list_intrusive_node), 0);
		list->length = length;
		list->max_count = 0;
		list->count = 0;
		list->data_size = (u32) data_size;
		list->free_index = U32_MAX;
		list->growable = growable;
	}
	else
	{
		if (mem)
		{
			arena_pop_record(mem);
		}
		else
		{
			array_list_intrusive_free(list);
		}
		
		list = NULL;
	}
	
	return list;

}

void array_list_intrusive_free(struct array_list_intrusive *list)
{
	if (list)
	{	
		ALLOCATOR_DEBUG_INDEX_FREE(list);
		free(list->data);
	}
	free(list);
}

void array_list_intrusive_flush(struct array_list_intrusive *list)
{
	list->max_count = 0;
	list->count = 0;
	list->free_index = U32_MAX;
	ALLOCATOR_DEBUG_INDEX_FLUSH(list);
}

static void internal_array_list_intrusive_realloc(struct array_list_intrusive *list)
{
	list->length <<= 1;
	list->data = realloc(list->data, list->length*list->data_size);
	ALLOCATOR_DEBUG_INDEX_ALIAS_AND_REPOISON(list, list->data, list->length);
}

struct slot array_list_intrusive_add(struct array_list_intrusive *list)
{
	struct slot allocation = { .address = NULL, .index = U32_MAX };

	if (list->count < list->length)
	{
		if (list->free_index != U32_MAX)
		{
			ALLOCATOR_DEBUG_INDEX_UNPOISON(list, list->free_index);
			allocation.address =  (u8 *) list->data + list->free_index * list->data_size;
			allocation.index = list->free_index;
			kas_assert(((struct array_list_intrusive_node *) allocation.address)->allocated == 0);
			list->free_index = ((struct array_list_intrusive_node *) allocation.address)->next_free;
		}
		else
		{
			ALLOCATOR_DEBUG_INDEX_UNPOISON(list, list->max_count);
			allocation.address = (u8 *) list->data + list->data_size * list->max_count;
			allocation.index = list->max_count;
			list->max_count += 1;
		}	
		((struct array_list_intrusive_node *) allocation.address)->allocated = 1;
		list->count += 1;
	}
	else if (list->growable)
	{
		internal_array_list_intrusive_realloc(list);
		ALLOCATOR_DEBUG_INDEX_UNPOISON(list, list->max_count);
		allocation.address = (u8 *) list->data + list->data_size * list->max_count;
		allocation.index = list->max_count;
		((struct array_list_intrusive_node *)allocation.address)->allocated = 1;
		list->max_count += 1;
		list->count += 1;
	}

	return allocation;
}

void *array_list_intrusive_reserve(struct array_list_intrusive *list)
{
	struct array_list_intrusive_node *node = NULL;

	if (list->count < list->length)
	{
		if (list->free_index != U32_MAX)
		{
			ALLOCATOR_DEBUG_INDEX_UNPOISON(list, list->free_index);
			node = (struct array_list_intrusive_node *) ((u8 *) list->data + list->free_index * list->data_size);
			kas_assert(node->allocated == 0);
			list->free_index = node->next_free;
		}
		else
		{
			ALLOCATOR_DEBUG_INDEX_UNPOISON(list, list->max_count);
			node = (struct array_list_intrusive_node *) ((u8 *) list->data + list->data_size * list->max_count);
			list->max_count += 1;
		}	
		node->allocated = 1;
		list->count += 1;
	}
	else if (list->growable)
	{
		internal_array_list_intrusive_realloc(list);
		ALLOCATOR_DEBUG_INDEX_UNPOISON(list, list->max_count);
		node = (struct array_list_intrusive_node *) ((u8 *) list->data + list->data_size * list->max_count);
		node->allocated = 1;
		list->max_count += 1;
		list->count += 1;
	}

	return node;
}

u32 array_list_intrusive_reserve_index(struct array_list_intrusive *list)
{
	u32 index = list->free_index;

	struct array_list_intrusive_node *node;
	if (list->count < list->length)
	{
		if (list->free_index != U32_MAX)
		{
			ALLOCATOR_DEBUG_INDEX_UNPOISON(list, list->free_index);
			node = (struct array_list_intrusive_node *)((u8 *) list->data + index*list->data_size);
			list->free_index = node->next_free;
			kas_assert(node->allocated == 0);
		}
		else
		{
			ALLOCATOR_DEBUG_INDEX_UNPOISON(list, list->max_count);
			index = list->max_count;
			node = (struct array_list_intrusive_node *)((u8 *) list->data + index*list->data_size);
			list->max_count += 1;
		}
		node->allocated = 1;
		list->count += 1;
	}
	else if (list->growable)
	{
		internal_array_list_intrusive_realloc(list);
		ALLOCATOR_DEBUG_INDEX_UNPOISON(list, list->max_count);
		index = list->max_count;
		node = (struct array_list_intrusive_node *)((u8 *) list->data + index*list->data_size);
		node->allocated = 1;
		list->max_count += 1;
		list->count += 1;
	}

	return index;
}

void array_list_intrusive_remove(struct array_list_intrusive *list, void *addr)
{
	kas_assert(addr && (void *) list->data <= addr && addr < (void *) ((u8 *) list->data + list->length * list->data_size));
	kas_assert(((u64) addr - (u64) list->data) % list->data_size == 0);
	
	struct array_list_intrusive_node *node = addr;
	node->allocated = 0;
	node->next_free = list->free_index;
	list->free_index = (u32) (((u64) addr - (u64) list->data) / list->data_size);
	list->count -= 1;
	ALLOCATOR_DEBUG_INDEX_POISON(list, ((u64) addr - (u64) list->data) / list->data_size);
}

void array_list_intrusive_remove_index(struct array_list_intrusive *list, const u32 index)
{
	kas_assert(index < list->length);
	struct array_list_intrusive_node *node = (struct array_list_intrusive_node *) ((u8 *) list->data + index * list->data_size);
	node->allocated = 0;
	node->next_free = list->free_index;
	list->free_index = index;
	list->count -= 1;
	ALLOCATOR_DEBUG_INDEX_POISON(list, index);
}

void *array_list_intrusive_address(struct array_list_intrusive *list, const u32 index)
{
	return (u8 *) list->data + index*list->data_size;
}

u32 array_list_intrusive_index(struct array_list_intrusive *list, const void *address)
{
	kas_assert((u64) address >= (u64) list->data);
	kas_assert((u64) address < (u64) list->data + list->length * list->data_size);
	kas_assert((u64) ((u64) address - (u64) list->data) % list->data_size == 0); 
	return (u32) (((u64) address - (u64) list->data) / list->data_size);
}
