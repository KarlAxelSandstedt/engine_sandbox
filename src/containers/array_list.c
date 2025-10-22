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

struct dll *dll_alloc(struct arena *mem, const u32 length, const u64 data_size, const u32 growable)
{
	kas_static_assert(sizeof(struct dll_node) == 8, "Expected header size");

	struct dll *list;
	if (mem)
	{
		kas_assert(growable == 0);
		arena_push_record(mem);
		list = arena_push(mem, sizeof(struct dll));
		if (list)
		{
			list->data = arena_push(mem, (length+2)*data_size);
		}
	}
	else
	{
		list = malloc(sizeof(struct dll));
		if (list)
		{
			list->data = malloc((length+2)*data_size);
		}
	}

	if (list && list->data)
	{
		ALLOCATOR_DEBUG_INDEX_ALLOC(list, list->data, (length+2), data_size, sizeof(struct dll_node), 0);
		list->length = length+2;
		list->max_count = 0;
		list->count = 0;
		list->data_size = (u32) data_size;
		list->free_index = U32_MAX;
		list->growable = growable;

		/* add stub at index 0 */
		dll_add(list);
		/* add null at index 1 */
		dll_add(list);
	}
	else if (list)
	{
		(mem) ? arena_pop_record(mem) : free(list);
		list = NULL;
	}
	
	return list;

}

void dll_free(struct dll *list)
{
	if (list)
	{	
		ALLOCATOR_DEBUG_INDEX_FREE(list);
		free(list->data);
	}
	free(list);
}

void dll_flush(struct dll *list)
{
	list->max_count = 0;
	list->count = 0;
	list->free_index = U32_MAX;
	ALLOCATOR_DEBUG_INDEX_FLUSH(list);

	/* add stub at index 0 */
	dll_add(list);
	/* add null at index 1 */
	dll_add(list);
}

static void internal_dll_realloc(struct dll *list)
{
	list->length <<= 1;
	list->data = realloc(list->data, list->length*list->data_size);
	ALLOCATOR_DEBUG_INDEX_ALIAS_AND_REPOISON(list, list->data, list->length);
}

struct slot dll_add(struct dll *list)
{
	struct slot allocation = { .address = NULL, .index = DLL_STUB };

	if (list->count < list->length)
	{
		if (list->free_index != U32_MAX)
		{
			ALLOCATOR_DEBUG_INDEX_UNPOISON(list, list->free_index);
			allocation.address = (u8 *) list->data + list->free_index * list->data_size;
			allocation.index = list->free_index;
			kas_assert(((struct dll_node *) allocation.address)->allocated == 0);
			list->free_index = ((struct dll_node *) allocation.address)->next;
		}
		else
		{
			ALLOCATOR_DEBUG_INDEX_UNPOISON(list, list->max_count);
			allocation.address = (u8 *) list->data + list->data_size * list->max_count;
			allocation.index = list->max_count;
			list->max_count += 1;
		}	
		((struct dll_node *) allocation.address)->prev = DLL_NULL;
		((struct dll_node *) allocation.address)->next = DLL_NULL;
		list->count += 1;
	}
	else if (list->growable)
	{
		internal_dll_realloc(list);
		ALLOCATOR_DEBUG_INDEX_UNPOISON(list, list->max_count);
		allocation.address = (u8 *) list->data + list->data_size * list->max_count;
		allocation.index = list->max_count;
		((struct dll_node *) allocation.address)->prev = DLL_NULL;
		((struct dll_node *) allocation.address)->next = DLL_NULL;
		list->max_count += 1;
		list->count += 1;
	}

	return allocation;
}

struct slot dll_prepend(struct dll *list, const u32 next)
{
	kas_assert(next != DLL_STUB && next != DLL_NULL);
	struct slot allocation = dll_add(list);
	kas_assert(allocation.index);
       	if (allocation.index)
	{
		struct dll_node *node_next = (struct dll_node *) ((u8 *) list->data + next*list->data_size);
		((struct dll_node *) allocation.address)->prev = node_next->prev;
		((struct dll_node *) allocation.address)->next = next;
		node_next->prev = allocation.index;
	}

	return allocation;
}

struct slot dll_append(struct dll *list, const u32 prev)
{
	kas_assert(prev != DLL_STUB && prev != DLL_NULL);
	struct slot allocation = dll_add(list);
       	if (allocation.index)
	{
		struct dll_node *node_prev = (struct dll_node *) ((u8 *) list->data + prev * list->data_size);
		((struct dll_node *) allocation.address)->prev = prev;
		((struct dll_node *) allocation.address)->next = node_prev->next;
		node_prev->next = allocation.index;
	}

	return allocation;
}

/* unset any links (...) <-> index <-> (...), and set links  next.prev <-> slot <-> next */
void dll_unlink_and_prepend(struct dll *list, const u32 index, const u32 next)
{
	struct dll_node *node = (struct dll_node *) ((u8 *) list->data + index * list->data_size);
	struct dll_node *node_prev = (struct dll_node *) ((u8 *) list->data + node->prev * list->data_size);
	struct dll_node *node_next = (struct dll_node *) ((u8 *) list->data + node->next * list->data_size);

	node_prev->next = node->next;
	node_next->prev = node->prev;

	if (next != DLL_NULL)
	{
		node_next = (struct dll_node *) ((u8 *) list->data + next * list->data_size);
		node_prev = (struct dll_node *) ((u8 *) list->data + node_next->prev * list->data_size);

		node->prev = node_next->prev;
		node->next = next;
		node_prev->next = index;
		node_next->prev = index;
	}
	else
	{
		node->prev = DLL_NULL;
		node->next = DLL_NULL;
	}
}

void dll_remove(struct dll *list, void *addr)
{
	kas_assert(addr && (void *) list->data <= addr && addr < (void *) ((u8 *) list->data + list->length * list->data_size));
	kas_assert(((u64) addr - (u64) list->data) % list->data_size == 0);
	
	struct dll_node *node = addr;
	struct dll_node *prev = (struct dll_node *) ((u8 *) list->data + node->prev*list->data_size);
	struct dll_node *next = (struct dll_node *) ((u8 *) list->data + node->next*list->data_size);

	kas_assert(node->prev == DLL_NULL || prev->next == (u32) (((u64) addr - (u64) list->data) / list->data_size));
	kas_assert(node->next == DLL_NULL || next->prev == (u32) (((u64) addr - (u64) list->data) / list->data_size));
	prev->next = node->next;
	next->prev = node->prev;

	kas_assert(node->allocated);
	node->allocated = DLL_STUB;
	node->next = list->free_index;
	list->free_index = (u32) (((u64) addr - (u64) list->data) / list->data_size);
	list->count -= 1;
	ALLOCATOR_DEBUG_INDEX_POISON(list, ((u64) addr - (u64) list->data) / list->data_size);
}

void dll_remove_index(struct dll *list, const u32 index)
{
	kas_assert(index < list->length);
	struct dll_node *node = (struct dll_node *) ((u8 *) list->data + index*list->data_size);
	struct dll_node *prev = (struct dll_node *) ((u8 *) list->data + node->prev*list->data_size);
	struct dll_node *next = (struct dll_node *) ((u8 *) list->data + node->next*list->data_size);

	kas_assert(node->prev == DLL_NULL || prev->next == index);
	kas_assert(node->next == DLL_NULL || next->prev == index);
	prev->next = node->next;
	next->prev = node->prev;

	kas_assert(node->allocated);
	node->allocated = DLL_STUB;
	node->next = list->free_index;
	list->free_index = index;
	list->count -= 1;
	ALLOCATOR_DEBUG_INDEX_POISON(list, index);
}

void *dll_address(const struct dll *list, const u32 index)
{
	return (u8 *) list->data + index*list->data_size;
}

u32 dll_index(const struct dll *list, const void *address)
{
	kas_assert((u64) address >= (u64) list->data);
	kas_assert((u64) address < (u64) list->data + list->length * list->data_size);
	kas_assert((u64) ((u64) address - (u64) list->data) % list->data_size == 0); 
	return (u32) (((u64) address - (u64) list->data) / list->data_size);
}

void dll_print(const struct dll *list, const u32 index)
{
	const struct dll_node *node = dll_address(list, index);

	if (!node->allocated)
	{
		fprintf(stderr, "Not Allocated");
	}
	else if (index == DLL_NULL)
	{
		fprintf(stderr, "DLL_NULL");
	}
	else
	{
		(node->prev == DLL_NULL)
			? fprintf(stderr, "DLL_NULL <-> [%u] <-> ", index)
			: fprintf(stderr, "%u <-> [%u] <-> ", node->prev, index);
		for (u32 i = node->next; i != DLL_NULL; i = node->next)
		{
			fprintf(stderr, "%u <-> ", node->next);
			node = dll_address(list, node->next);
		}
		fprintf(stderr, "DLL_NULL\n");
	}
}
