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

#ifndef __FIFO_TICKET_H__
#define __FIFO_TICKET_H__

#include "ds_common.h"
#include "allocator.h"
#include "sys_public.h"

#define TICKET_FACTORY_CLOSED U32_MAX

/*
 * multiple ticket producers with a maximum ticket count. It is up to the user to determine
 * when a batch of tickets should be returned, or "served".
 */
struct ticket_factory
{
	semaphore available;	/* slots available for producers */
	u32 a_serve;		/* useful to keep track of where we are in array */
	u32 a_next;
	u32 a_open;		/* when open, tickets can be retrieved */
	u32 max_tickets;	/* debug */
};

struct ticket_factory * ticket_factory_init(struct arena *mem, const u32 max_tickets);
void			ticket_factory_destroy(struct ticket_factory *tf);
/* returns 0 on no ticket, 1 on ticket acquisition, or TICKET_FACTORY_CLOSED when no more tickets can be retrieved  */
u32			ticket_factory_try_get_ticket(u32 *ticket, struct ticket_factory *tf);
/* UNDEFINED BEHAVIOUR WHEN TICKET_FACTORY_CLOSED! wait on semaphore until we return a ticket  */
u32			ticket_factory_get_ticket(struct ticket_factory *tf);
/* puts ticket range [a_serve, a_serve + count) up for use again  */
void			ticket_factory_return_tickets(struct ticket_factory *tf, const u32 count);

#endif
