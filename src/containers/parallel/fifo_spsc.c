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

#include <stdlib.h>

#include "fifo_spsc.h"

struct fifo_spsc *fifo_alloc(struct arena *mem)
{
	struct fifo_spsc *q = NULL;

	if (mem)
	{
		q = ArenaPush(mem, sizeof(struct fifo_spsc));
		q->a_first = ArenaPush(mem, sizeof(struct fifo_spsc_node));
	}
	else
	{
		q = malloc(sizeof(struct fifo_spsc));
		q->a_first = malloc(sizeof(struct fifo_spsc_node));
	}

	q->a_last = q->a_first;
	q->a_first->a_next = NULL;
	q->a_first->data = NULL;

	return q;
}

void *fifo_spsc_pop(struct fifo_spsc *q)
{
	struct fifo_spsc_node *first = (struct fifo_spsc_node *) AtomicLoadRlx64(&q->a_first);
	struct fifo_spsc_node *next  = (struct fifo_spsc_node *) AtomicLoadAcq64(&first->a_next);
	/* Local thread rw barrier */
	/* Any writes before first->next release on other threads are now visible. */

	/* We are at dummy, so queue was empty when we loaded next aquired next */
	if (next == NULL) return NULL;

	void *data = (void *) AtomicLoadRlx64(&next->data);
	AtomicStoreRlx64(&next->data, NULL);
	AtomicStoreRlx64(&q->a_first, next);	

	return data;
}

void fifo_spsc_push(struct fifo_spsc *q, struct fifo_spsc_node *node)
{

	struct fifo_spsc_node *prev_last = (struct fifo_spsc_node *) AtomicLoadRlx64(&q->a_last); 
	AtomicStoreRlx64(&q->a_last, node);

	/* Any aquisition of a next node will always be a pointer to valid data */
	/* Local thread rw barrier */
	AtomicStoreRel64(&prev_last->a_next, node);
}
