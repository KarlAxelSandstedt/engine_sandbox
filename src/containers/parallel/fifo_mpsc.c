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

#include "fifo_mpsc.h"
#include "kas_math.h"

struct fifo_mpsc *fifo_mpsc_init(struct arena *mem, const u32 max_entry_count)
{
	assert(max_entry_count > 0 && is_power_of_two(max_entry_count));
	struct fifo_mpsc *q = NULL; 

	q = arena_push(mem, sizeof(struct fifo_mpsc));
	q->max_entry_count = max_entry_count;
	q->entries = arena_push(mem, q->max_entry_count * sizeof(struct fifo_mpsc_entry));
	for (u32 i = 0; i < q->max_entry_count; ++i)
	{
		atomic_store_rel_32(&q->entries[i].a_pushed, 0);
		q->entries[i].data = NULL;
	}

	semaphore_init(&q->available, max_entry_count);
	atomic_store_rel_32(&q->a_next, 0);
	atomic_store_rel_32(&q->a_first, 0);

	return q;
}

void fifo_mpsc_destroy(struct fifo_mpsc *q)
{
	semaphore_destroy(&q->available);
}

u32 fifo_mpsc_try_push(struct fifo_mpsc *q, void *data)
{
	u32 pushed = 0;
	if (semaphore_trywait(&q->available))
	{
		pushed = 1;	
		const u32 i = atomic_fetch_add_rlx_32(&q->a_next, 1) % q->max_entry_count;
		q->entries[i].data = data;
		atomic_store_rel_32(&q->entries[i].a_pushed, 1);
		/* mem-barrier */
	}

	return pushed;
}

void fifo_mpsc_push(struct fifo_mpsc *q, void *data)
{
	while (!fifo_mpsc_try_push(q, data));
}

void *fifo_mpsc_consume(struct fifo_mpsc *q)
{
	const u32 first = atomic_load_acq_32(&q->a_first);
	/* mem-barrier */
	const u32 pushed = atomic_load_acq_32(&q->entries[first].a_pushed);

	void *data = NULL;
	if (pushed)
	{	
		data = q->entries[first].data;
		atomic_store_rel_32(&q->entries[first].a_pushed, 1);
		semaphore_post(&q->available);
	}

	return data;
}

void *fifo_mpsc_peek(struct fifo_mpsc *q)
{
	const u32 first = atomic_load_acq_32(&q->a_first);
	/* mem-barrier */
	const u32 pushed = atomic_load_acq_32(&q->entries[first].a_pushed);
	return (pushed) ? q->entries[first].data : NULL;
}
