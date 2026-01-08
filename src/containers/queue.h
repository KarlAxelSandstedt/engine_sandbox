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

#ifndef __MIN_QUEUE_H__
#define __MIN_QUEUE_H__

#include "kas_common.h"
#include "allocator.h"

/*
min_queue
=========
A min prioritiy queue which prioritises objects with low priorities This means that an element having a low priority
will be served faster than an element with a higher priority.

HOW-TO-USE:
	index = queue.push(priority, id) => queue.elements[index].{ priority, object_index }
	object indices are only used to find storage for user-level indices.

	extract minimum => returns external index corresponding to minimum (user_id = queue->object_index[i])
*/

struct queue_object
{
	POOL_SLOT_STATE;
	u32 	external_index;
	u32	queue_index;
};

struct queue_element 
{
	f32	priority;
	u32 	object_index;
};

struct min_queue 
{
	struct pool 		object_pool;
	struct queue_element *	elements;
	u32			growable;
	u32			heap_allocated;
};

/* Allocate a new priority queue. */
struct min_queue	min_queue_new(struct arena *arena, const u32 initial_length, const u32 growable);
/* Free a queue and all it's resources. */
void 			min_queue_free(struct min_queue * const queue);
/* append new element with given priority to queue, return its object index */
u32 			min_queue_insert(struct min_queue * const queue, const f32 priority, const u32 external_index);
/* return external index corresponding to extracted queue element */
u32 			min_queue_extract_min(struct min_queue * const queue);
/* Decrease the priority of the object corresponding to queue_index if the priority is smaller than it's
   current priority. If changes are made in the queue, the queue is updated and kept coherent.  */ 
void 			min_queue_decrease_priority(struct min_queue * const queue, const u32 object_index, const f32 priority);
/* flush min_queue */
void 			min_queue_flush(struct min_queue * const queue);

/*
min_queue_fixed
===============
Simplified min queue for the case when re-insertion of elements (changing the priority) isn't needed.
*/

struct min_queue_fixed 
{
	u32f32 *element;
	u32 	count;
	u32	length;
	u32	growable;
	u32 	heap_allocated;
};

/* Alocate a new priority queue */
struct min_queue_fixed	min_queue_fixed_new(struct arena *mem, const u32 initial_length, const u32 growable);
/* Free (if heap allocated) allocated memory */
void 			min_queue_fixed_free(struct min_queue_fixed *queue);
/* Flush queue resources */
void			min_queue_fixed_flush(struct min_queue_fixed *queue);
/* Debug print */
void 			min_queue_fixed_print(FILE *log, const struct min_queue_fixed *queue);
/* Push (id, priority) pair onto queue. */
void 			min_queue_fixed_push(struct min_queue_fixed *queue, const u32 id, const f32 priority);
/* pop minimum element */
u32f32 			min_queue_fixed_pop(struct min_queue_fixed *heap);
/* peek minimum element */
u32f32 			min_queue_fixed_peek(const struct min_queue_fixed *heap);

#endif
