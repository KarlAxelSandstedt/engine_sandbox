/*
==========================================================================
    Copyright (C) 2025,2026 Axel Sandstedt 

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

#include <stdio.h>
#include <stdlib.h>
#include <float.h>
#include "queue.h"
#include "sys_public.h"

/**
 * parent_index() - get queue index of parent.
 *
 * queue_index - index of child
 *
 * RETURNS: the index of the parent. If the function is called on the element first in the queue (index 0),
 * 	then an error value of U32_MAX is returned: 0/2 - ((0 AND 1) XOR 1) = 0 - (0 XOR 1) = U32_MAX.
 */
static u32 parent_index(const u32 queue_index)
{
	return queue_index / 2 - ((queue_index & 0x1) ^ 0x1);
}

/**
 * left_index() - get queue index of left child 
 *
 * queue_index - index of parent 
 */
static u32 left_index(const u32 queue_index)
{
	return (queue_index << 1) + 1;
}

/**
 * right_index() - get queue index of right child 
 *
 * queue_index - index of parent 
 */
static u32 right_index(const u32 queue_index)
{
	return (queue_index + 1) << 1;
}

/**
 * min_queue_change_elements() - Change two elements in the element array, and update their corresponding 
 * 	data' queue_index
 *
 * queue - The queue
 * i1 - queue index of element 1
 * i2 - queue index of element 2
 */
static void min_queue_change_elements(struct min_queue * const queue, const u32 i1, const u32 i2)
{
	/* Update data queue indices */
	struct queue_object *obj1 = pool_address(&queue->object_pool, queue->elements[i1].object_index);
	struct queue_object *obj2 = pool_address(&queue->object_pool, queue->elements[i2].object_index);
	obj1->queue_index = i2;
	obj2->queue_index = i1;

	/* Update priorities */
	const f32 tmp_priority = queue->elements[i1].priority;
	queue->elements[i1].priority = queue->elements[i2].priority;
	queue->elements[i2].priority = tmp_priority;

	/* update queue's data indices */
	const u32 tmp_index = queue->elements[i1].object_index;
	queue->elements[i1].object_index = queue->elements[i2].object_index;
	queue->elements[i2].object_index = tmp_index;
}

/**
 * min_queue_heapify_up() - Keep the queue coherent after a decrease of the queue_index's priority has been made.
 * 	A Decrease of the priority may destroy the coherency of the queue, as the parent should always be infront
 * 	of the children in the queue. decreasing the priority of a child may thus result in the child having a
 * 	lower priority than it's parent, so they have to be interchanged.
 *
 * queue - The queue
 * queue_index - The index of the queue element who just had it's priority decreased.
 */
static void min_queue_heapify_up(struct min_queue * const queue, u32 queue_index)
{
	u32 parent = parent_index(queue_index);

	/* Continue until parent's priority is smaller than child or the root has been reached */
	while (parent != U32_MAX && queue->elements[queue_index].priority < queue->elements[parent].priority) 
	{
		min_queue_change_elements(queue, queue_index, parent);
		queue_index = parent;
		parent = parent_index(queue_index); 
	}
}

static void recursion_done(struct min_queue * const queue, const u32 queue_index, const u32 small_priority_index);
static void recursive_call(struct min_queue * const queue, const u32 queue_index, const u32 small_priority_index);

void (*func[2])(struct min_queue * const, const u32, const u32) = { &recursion_done, &recursive_call };

/**
 * min_queue_heapify_down() - Keeps the queue coherent when the element at queue_index has had 
 * 	it's priority increased. Then the element may have a higher priority than it's children, breaking
 * 	the min-heap property.
 *
 * queue - The queue
 * queue_index - Index of the element whose priority has been increased
 */
static void min_queue_heapify_down(struct min_queue * const queue, const u32 queue_index)
{
	const u32 left = left_index(queue_index);
	const u32 right = right_index(queue_index);
	u32 smallest_priority_index = queue_index;

	if (left < queue->object_pool.count && queue->elements[left].priority < queue->elements[smallest_priority_index].priority)
		smallest_priority_index = left;
	
	if (right < queue->object_pool.count && queue->elements[right].priority < queue->elements[smallest_priority_index].priority)
		smallest_priority_index = right;
	
	/* Child had smaller priority */
	func[ ((u32) queue_index - smallest_priority_index) >> 31 ](queue, queue_index, smallest_priority_index);
}

static void recursion_done(struct min_queue * const queue, const u32 queue_index, const u32 small_priority_index)
{
	return;
}

static void recursive_call(struct min_queue * const queue, const u32 queue_index, const u32 smallest_priority_index)
{
		min_queue_change_elements(queue, queue_index, smallest_priority_index);
		/* Now child may break the min-heap property */
		min_queue_heapify_down(queue, smallest_priority_index);
}

struct min_queue min_queue_new(struct arena *arena, const u32 initial_length, const u32 growable)
{
	kas_assert(initial_length);
	kas_assert(!arena || !growable);
	kas_static_assert(sizeof(u32f32) == 8);

	struct min_queue_fixed queue;

	if (arena)
	{
		queue.object_pool = pool_alloc(arena, initial_length, struct queue_object, !GROWABLE);
		queue.elements = arena_push(arena, initial_length * sizeof(struct queue_element));
		queue.heap_allocated = 0;
	}
	else
	{
		queue.object_pool = pool_alloc(NULL, initial_length, struct queue_object, !GROWABLE);
		queue.elements = malloc(initial_length * sizeof(struct queue_element));
		queue.heap_allocated = 1;
	}

	queue.growable = growable;

	if (queue.object_pool.length == 0 || queue.elements == NULL)
	{
		log_string(T_SYSTEM, S_FATAL, "Failed to allocate min queue, exiting.");
		fatal_cleanup_and_exit(kas_thread_self_tid());
	}
		
	return queue;
}

void min_queue_free(struct min_queue * const queue)
{
	if (queue->heap_allocated)
	{
		pool_dealloc(&queue->object_pool);
		free(queue->elements);
	}
}

u32 min_queue_extract_min(struct min_queue * const queue)
{
	kas_assert_string(queue->object_pool.count > 0, "Queue should have elements to extract\n");
	struct queue_object *obj = pool_address(&queue->object_pool, queue->elements[0].object_index);
	const u32 external_index = obj->external_index;
	queue->elements[0].priority = FLT_MAX;

	/* Keep the array of elements compact */
	min_queue_change_elements(queue, 0, queue->object_pool.count-1);
	/* Check coherence of the queue from the start */
	min_queue_heapify_down(queue, 0);

	pool_remove_address(&queue->object_pool, obj);

	return external_index;
}

u32 min_queue_insert(struct min_queue * const queue, const f32 priority, const u32 external_index)
{
	const u32 old_length = queue->object_pool.length;
	const u32 queue_index = queue->object_pool.count;
	struct slot slot = pool_add(&queue->object_pool);
	if (old_length != queue->object_pool.length)
	{
		kas_assert(queue->growable);
		queue->elements = realloc(queue->elements, queue->object_pool.length * sizeof(struct queue_element));
		if (queue->elements == NULL)
		{
			log_string(T_SYSTEM, S_FATAL, "Failed to reallocate min queue, exiting.");
			fatal_cleanup_and_exit(kas_thread_self_tid());
		}
	}

	queue->elements[queue_index].priority = priority;
	queue->elements[queue_index].object_index = slot.index;

	struct queue_object *object = slot.address;
	object->external_index = external_index;
	object->queue_index = queue_index;

	min_queue_heapify_up(queue, queue_index);

	return slot.index;
}

void min_queue_decrease_priority(struct min_queue * const queue, const u32 object_index, const f32 priority)
{
	kas_assert_string(object_index < queue->object_pool.count, "Queue index should be withing queue bounds");

	struct queue_object *obj = pool_address(&queue->object_pool, object_index);
	if (priority < queue->elements[obj->queue_index].priority) 
	{
		queue->elements[obj->queue_index].priority = priority;
		min_queue_heapify_up(queue, obj->queue_index);
	}
}

void min_queue_flush(struct min_queue * const queue)
{
	pool_flush(&queue->object_pool);
}

static void min_queue_fixed_heapify_up(struct min_queue_fixed * const queue, u32 queue_index)
{
	u32 parent = parent_index(queue_index);

	/* Continue until parent's priority is smaller than child or the root has been reached */
	while (parent != U32_MAX && queue->element[queue_index].f32 < queue->element[parent].f32) 
	{
		const u32f32 tuple = queue->element[queue_index];
		queue->element[queue_index] = queue->element[parent];
		queue->element[parent] = tuple; 
		queue_index = parent;
		parent = parent_index(queue_index); 
	}
}

static void queue_fixed_recursion_done(struct min_queue_fixed * const queue, const u32 queue_index, const u32 small_priority_index);
static void queue_fixed_recursive_call(struct min_queue_fixed * const queue, const u32 queue_index, const u32 small_priority_index);

void (*queue_fixed_func[2])(struct min_queue_fixed * const, const u32, const u32) = { &queue_fixed_recursion_done, &queue_fixed_recursive_call };

static void min_queue_fixed_heapify_down(struct min_queue_fixed * const queue, const u32 queue_index)
{
	const u32 left = left_index(queue_index);
	const u32 right = right_index(queue_index);
	u32 smallest_priority_index = queue_index;

	if (left < queue->count && queue->element[left].f32 < queue->element[smallest_priority_index].f32)
		smallest_priority_index = left;
	
	if (right < queue->count && queue->element[right].f32 < queue->element[smallest_priority_index].f32)
		smallest_priority_index = right;
	
	/* Child had smaller priority */
	queue_func[ ((u32) queue_index - smallest_priority_index) >> 31 ](queue, queue_index, smallest_priority_index);
}

static void queue_recursion_done(struct min_queue_fixed * const queue, const u32 queue_index, const u32 small_priority_index)
{
	return;
}

static void queue_recursive_call(struct min_queue_fixed * const queue, const u32 queue_index, const u32 smallest_priority_index)
{
	min_queue_fixed_change_elements(queue, queue_index, smallest_priority_index);
	min_queue_fixed_queueify_down(queue, smallest_priority_index);
}


struct min_queue_fixed min_queue_fixed_alloc(struct arena *mem, const u32 initial_length, const u32 growable)
{
	kas_assert(!growable || !mem);
	if (!initial_length) { return (struct min_queue_fixed) { 0 } };

	struct min_queue_fixed queue =
	{
		.count = 0,
		.growable = growable,
		.length = initial_length,	
	};

	if (mem)
	{
		queue->heap_allocated = 0;
		queue->element = arena_push(mem, inital_length*sizeof(u32f32));
	}
	else
	{
		queue->heap_allocated = 1;
		queue->element = malloc(inital_length*sizeof(u32f32));
	}

	if (queue.elements == NULL)
	{
		log_string(T_SYSTEM, S_FATAL, "Failed to allocate min_queue_fixed memory, exiting.");
		fatal_cleanup_and_exit(kas_thread_self_tid());
	}

	return queue;
}

struct min_queue_fixed min_queue_fixed_alloc_all(struct arena *mem)
{
	kas_assert(mem);

	struct allocation_array arr = arena_push_aligned_all(mem, sizeof(u32f32), 4);
	struct min_queue_fixed queue =
	{
		.count = 0,
		.growable = 0,
		.heap_allocated = 0,
		.length = arr.len,	
		.element = arr.addr,
	};

	return queue;
}

void min_queue_fixed_dealloc(struct min_queue_fixed *queue)
{
	if (queue->heap_allocated)
	{
		free(queue->element);
	}
}

void min_queue_fixed_flush(struct min_queue_fixed *queue)
{
	queue->count = 0;
}

void min_queue_fixed_print(FILE *log, const struct min_queue_fixed *queue)
{
	fprintf(log, "min queue_fixed %p: ", queue);
	fprintf(log, "{ ");
	for (u32 i = 0; i < queue->count; ++i)
	{
		fprintf(log, "(%u,%f), ", queue->element[i].u32, queue->element[i].f32);
	}
	fprintf(log, "}\n");


}

void min_queue_fixed_push(struct min_queue_fixed *queue, const u32 id, const f32 priority)
{
	if (queue->count == queue->length)
	{
		if (queue->growable)
		{
			queue->length *= 2;
			queue->element = realloc(queue->element, queue->length*sizeof(u32f32));
			if (queue->element == NULL)
			{
				log_string(T_SYSTEM, S_FATAL, "Failed to reallocate min_queue_fixed memory, exiting.");
				fatal_cleanup_and_exit(kas_thread_self_tid());
			}
		}	
		else
		{
			return;
		}
	}

	const u32 i = queue->count;
	queue->count += 1;
	queue->elements[i].f32 = priority;
	queue->elements[i].u32 = id;
	min_queue_fixed_heapify_up(queue, i);
}

u32f32 min_queue_fixed_pop(struct min_queue_fixed *queue)
{
	kas_assert_string(queue->count > 0, "Heap should have elements to extract\n");
	queue->count -= 1;

	const u32f32 tuple = queue->elements[0];
	queue->element[0] = queue->element[queue->count];
	min_queue_fixed_heapify_down(queue, 0);

	return tuple;
}

u32f32 	min_queue_fixed_peek(const struct min_queue_fixed *queue)
{
	kas_assert_string(queue->count > 0, "Heap should have elements to extract\n");
	return queue->element[0];
}
