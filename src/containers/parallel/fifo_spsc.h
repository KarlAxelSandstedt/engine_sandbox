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

#ifndef __FIFO_SPSC_H__
#define __FIFO_SPSC_H__

#include "kas_common.h"
#include "allocator.h"
#include "sys_public.h"

/* TODO: Pad cachelines to protect against flase sharing */
/* TODO: Clean write up of partial ownership */
struct fifo_spsc_node
{
	struct fifo_spsc_node *a_next;	/* atomic ptr, is NULL if last */
	void *data;
};

/*
 * fifo_spsc - non-intrusive first in first out, signle producer single consumer queue.
 * 
 * Invariants:
 */
struct fifo_spsc
{
	struct fifo_spsc_node *a_first; /**/
	struct fifo_spsc_node *a_last;  /**/
};

struct fifo_spsc *	fifo_alloc(struct arena *mem);
void *			fifo_spsc_pop(struct fifo_spsc *q);
void 			fifo_spsc_push(struct fifo_spsc *q, struct fifo_spsc_node *node);

#endif
