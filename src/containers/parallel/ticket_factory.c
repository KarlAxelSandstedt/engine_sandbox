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

#include "ticket_factory.h"
#include "kas_math.h"

struct ticket_factory *ticket_factory_init(struct arena *mem, const u32 max_tickets)
{
	assert(max_tickets > 0 && is_power_of_two(max_tickets));
	struct ticket_factory *tf = arena_push(mem, sizeof(struct ticket_factory));

	tf->max_tickets = max_tickets;
	semaphore_init(&tf->available, max_tickets);
	atomic_store_rel_32(&tf->a_serve, 0);
	atomic_store_rel_32(&tf->a_next, 0);
	atomic_store_rel_32(&tf->a_open, 1);

	return tf;
}

void ticket_factory_destroy(struct ticket_factory *tf)
{
	semaphore_destroy(&tf->available);
}

u32 ticket_factory_try_get_ticket(u32 *ticket, struct ticket_factory *tf)
{
	if (!atomic_load_acq_32(&tf->a_open)) { return TICKET_FACTORY_CLOSED; }

	u32 got_ticket = 0;
	if (semaphore_trywait(&tf->available))
	{
		got_ticket = 1;
		*ticket = atomic_fetch_add_rlx_32(&tf->a_next, 1);
	}

	return got_ticket;
}

u32 ticket_factory_get_ticket(struct ticket_factory *tf)
{
	u32 ticket;
	while (!ticket_factory_try_get_ticket(&ticket, tf));

	return ticket;
}

void ticket_factory_return_tickets(struct ticket_factory *tf, const u32 count)
{
	assert(count <= tf->max_tickets);
	assert(count <= ((u32) atomic_load_rlx_32(&tf->a_next) - (u32) atomic_load_rlx_32(&tf->a_serve)));
	assert(((u32) atomic_load_rlx_32(&tf->a_next) - (u32) atomic_load_rlx_32(&tf->a_serve)) <= tf->max_tickets);

	/* sync_point */
	const u32 serve = atomic_fetch_add_rel_32(&tf->a_serve, count);
	for (u32 i = 0; i < count; ++i)
	{
		semaphore_post(&tf->available);
	}	
}
