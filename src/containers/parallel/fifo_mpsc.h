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

#ifndef __FIFO_MPSC_H__
#define __FIFO_MPSC_H__

#include "kas_common.h"
#include "allocator.h"
#include "sys_public.h"

struct fifo_mpsc_entry
{
	void *data;
	u32 a_pushed; /* acq-rel sync */
};

struct fifo_mpsc
{
	struct fifo_mpsc_entry *entries;
	/* TODO CACHELINE */
	semaphore available;	/* slots available for producers */
	/* TODO CACHELINE */
	u32 a_next;		/* Producer owned, next to push */
	/* TODO CACHELINE */
	u32 a_first;		/* Consumer owned, first to consume */
	/* TODO CACHELINE */
	u32 max_entry_count;	/* Must be power of 2, so that on overflow of tickers, 
				 * modulo count still is correct. It may not be U32_MAX. 
				 */
};

struct fifo_mpsc *	fifo_mpsc_init(struct arena *mem, const u32 max_entry_count);
void			fifo_mpsc_destroy(struct fifo_mpsc *q);
/* returns 0 push failure, otherwise return 1 */
u32			fifo_mpsc_try_push(struct fifo_mpsc *q, void *data);
/* spinlock push data */
void			fifo_mpsc_push(struct fifo_mpsc *q, void *data);
/* returns NULL if no pushed data, otherwise pop memory and return pointer to producer provided memory */
void *			fifo_mpsc_consume(struct fifo_mpsc *q);
/* returns NULL if no pushed data, otherwise return pointer to producer provided memory */
void *			fifo_mpsc_peek(struct fifo_mpsc *q);

#endif
