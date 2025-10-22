#include "list.h"

struct list list_init_interal(const u64 slot_size, const u64 slot_state_offset)
{
	struct list list =  
	{ 
		.count = 0,
	       	.first = LIST_NULL, 
		.last = LIST_NULL, 
		.slot_size = slot_size, 
		.slot_state_offset = slot_state_offset 
	};

	return list;
}

void list_flush(struct list *list)
{
	list->count = 0;
	list->first = LIST_NULL;
	list->last = LIST_NULL;
}

void list_append(struct list *list, void *array, const u32 index)
{
	list->count += 1;
	u32 *first = (u32*) ((u8*) array + index*list->slot_size + list->slot_state_offset);
	*first = list->first;
	list->first = index;
	if (list->last == LIST_NULL)
	{
		list->last = index;
	}
}

void list_prepend(struct list *list, void *array, const u32 index)
{
	list->count += 1;
	if (list->last == LIST_NULL)
	{
		list->first = index;
	}
	else
	{
		u32 *last = (u32*) ((u8*) array + list->last*list->slot_size + list->slot_state_offset);
		*last = index;
	}
	list->last = index;
	u32 *prev = (u32*) ((u8*) array + index*list->slot_size + list->slot_state_offset);
	*prev = LIST_NULL;	
}
