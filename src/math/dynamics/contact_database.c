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

#include "contact_database.h"
#include "sys_public.h"
#include "physics_pipeline.h"

u32 c_db_index_in_previous_contact_node(struct net_list *net, void **prev_node, const void *cur_node, const u32 cur_index)
{
	kas_assert(cur_index <= 1);
	const struct contact *c = cur_node;
	const u32 body = (1-cur_index) * CONTACT_KEY_TO_BODY_0(c->key) + cur_index * CONTACT_KEY_TO_BODY_1(c->key);
	
	*prev_node = net_list_address(net, c->header.prev[cur_index]);
	const u64 key = ((struct contact *) *prev_node)->key;
	kas_assert(c->header.prev[cur_index] == NET_LIST_NODE_NULL_INDEX || body == CONTACT_KEY_TO_BODY_0(key) || body == CONTACT_KEY_TO_BODY_1(key));
	return (body == CONTACT_KEY_TO_BODY_0(key))
		? 0
		: 1;
}

u32 c_db_index_in_next_contact_node(struct net_list *net, void **next_node, const void *cur_node, const u32 cur_index)
{
	kas_assert(cur_index <= 1);
	const struct contact *c = cur_node;
	const u32 body = (1-cur_index) * CONTACT_KEY_TO_BODY_0(c->key) + cur_index * CONTACT_KEY_TO_BODY_1(c->key);
	
	*next_node = net_list_address(net, c->header.next[cur_index]);
	const u64 key = ((struct contact *) *next_node)->key;
	kas_assert(c->header.next[cur_index] == NET_LIST_NODE_NULL_INDEX || body == CONTACT_KEY_TO_BODY_0(key) || body == CONTACT_KEY_TO_BODY_1(key));
	return (body == CONTACT_KEY_TO_BODY_0(key))
		? 0
		: 1;
}

struct contact_database c_db_alloc(struct arena *mem_persistent, const u32 size)
{
	struct contact_database c_db = { 0 };
	kas_assert(is_power_of_two(size));

	c_db.contacts = net_list_alloc(NULL, size, sizeof(struct contact), ARRAY_LIST_GROWABLE, c_db_index_in_previous_contact_node, c_db_index_in_next_contact_node);
	c_db.contact_map = hash_map_alloc(NULL, size, size, HASH_GROWABLE);
	c_db.contacts_persistent_usage = bit_vec_alloc(NULL, size, 0, 1);

	return c_db;
}

void c_db_free(struct contact_database *c_db)
{
	net_list_free(c_db->contacts);
	hash_map_free(c_db->contact_map);
	bit_vec_free(&c_db->contacts_persistent_usage);
}

void c_db_flush(struct contact_database *c_db)
{
	c_db_clear_frame(c_db);
	net_list_flush(c_db->contacts);
	hash_map_flush(c_db->contact_map);
	bit_vec_clear(&c_db->contacts_persistent_usage, 0);
}

void c_db_validate(const struct physics_pipeline *pipeline)
{
	for (u64 i = 0; i < pipeline->c_db.contacts_persistent_usage.bit_count; ++i)
	{
		if (bit_vec_get_bit(&pipeline->c_db.contacts_persistent_usage, i))
		{
			const struct contact *c = net_list_address(pipeline->c_db.contacts, (u32) i);
			kas_assert(c->header.chain.allocated);

			//fprintf(stderr, "contact[%lu] (next[0], next[1], prev[0], prev[1]) : (%u,%u,%u,%u)\n",
			//	       i,
			//	       c->header.next[0],	
			//	       c->header.next[1],	
			//	       c->header.prev[0],	
			//	       c->header.prev[1]);

			const struct rigid_body *b1 = physics_pipeline_rigid_body_lookup(pipeline, c->cm.i1);
			const struct rigid_body *b2 = physics_pipeline_rigid_body_lookup(pipeline, c->cm.i2);

			u32 prev, k, found; 
			prev = NET_LIST_NODE_NULL_INDEX;
			k = b1->first_contact_index;
			found = 0;
			while (k != NET_LIST_NODE_NULL_INDEX)
			{
				if (k == i)
				{
					found = 1;
					break;
				}

				const struct contact *tmp = net_list_address(pipeline->c_db.contacts, k);
				kas_assert(tmp->header.chain.allocated);
				if (CONTACT_KEY_TO_BODY_0(tmp->key) == c->cm.i1)
				{
					kas_assert(prev == tmp->header.prev[0]);
					prev = k;
					k = tmp->header.next[0];
				}
				else
				{
					kas_assert(CONTACT_KEY_TO_BODY_1(tmp->key) == c->cm.i1);
					kas_assert(prev == tmp->header.prev[1]);
					prev = k;
					k = tmp->header.next[1];
				}
			}
			kas_assert(found);
 
			prev = NET_LIST_NODE_NULL_INDEX;
			k = b2->first_contact_index;
			found = 0;
			while (k != NET_LIST_NODE_NULL_INDEX)
			{
				if (k == i)
				{
					found = 1;
					break;
				}

				const struct contact *tmp = net_list_address(pipeline->c_db.contacts, k);
				kas_assert(tmp->header.chain.allocated);
				if (CONTACT_KEY_TO_BODY_0(tmp->key) == c->cm.i2)
				{
					kas_assert(prev == tmp->header.prev[0]);
					prev = k;
					k = tmp->header.next[0];
				}
				else
				{
					kas_assert(prev == tmp->header.prev[1]);
					kas_assert(CONTACT_KEY_TO_BODY_1(tmp->key) == c->cm.i2);
					prev = k;
					k = tmp->header.next[1];
				}
			}
			kas_assert(found);
		}
	}
}

void c_db_update_persistent_contacts_usage(struct contact_database *c_db)
{
	kas_assert(c_db->contacts_persistent_usage.block_count == c_db->contacts_frame_usage.block_count);
	for (u64 i = 0; i < c_db->contacts_frame_usage.block_count; ++i)
	{
		c_db->contacts_persistent_usage.bits[i] = c_db->contacts_frame_usage.bits[i];	
	}

	if (c_db->contacts_persistent_usage.bit_count < c_db->contacts->max_count)
	{
		const u64 low_bit = c_db->contacts_persistent_usage.bit_count;
		const u64 high_bit = c_db->contacts->max_count;
		bit_vec_increase_size(&c_db->contacts_persistent_usage, c_db->contacts->length, 0);
		/* any new contacts that is in the appended region must now be set */
		for (u64 bit = low_bit; bit < high_bit; ++bit)
		{
			bit_vec_set_bit(&c_db->contacts_persistent_usage, bit, 1);
		}
	}
}


void c_db_clear_frame(struct contact_database *c_db)
{
	c_db->contacts_frame_usage.bits = NULL;
	c_db->contacts_frame_usage.bit_count = 0;
	c_db->contacts_frame_usage.block_count = 0;
	c_db->broken_list = NULL;
	c_db->new_list = NULL;
	c_db->broken_count = 0;
	c_db->new_count = 0;
}

struct contact *c_db_add_contact(struct physics_pipeline *pipeline, const struct contact_manifold *cm, const u32 i1, const u32 i2)
{
	u32 b1, b2;
	if (i1 < i2)
	{
		b1 = i1;
		b2 = i2;
	}
	else
	{
		b1 = i2;
		b2 = i1;
	}

	struct rigid_body *body1 = physics_pipeline_rigid_body_lookup(pipeline, b1);
	struct rigid_body *body2 = physics_pipeline_rigid_body_lookup(pipeline, b2);

	const u64 key = key_gen_u32_u32(b1, b2);
	kas_assert(b1 == CONTACT_KEY_TO_BODY_0(key));
	kas_assert(b2 == CONTACT_KEY_TO_BODY_1(key));
	const u32 index = c_db_lookup_contact_index(&pipeline->c_db, b1, b2);

	if (index == NET_LIST_NODE_NULL_INDEX)
	{
		const struct contact cpy =
		{
			.cm = *cm,
			.key = key,
			.cached_count = 0,
		};

		/* smaller valued body owns slot 0, larger valued body owns slot 1 in node header */
		kas_assert(body1->header.allocated);
		kas_assert(body2->header.allocated);
		const u32 ci = net_list_push(pipeline->c_db.contacts, &cpy, body1->first_contact_index, body2->first_contact_index);
		body1->first_contact_index = ci;
		body2->first_contact_index = ci;

		hash_map_add(pipeline->c_db.contact_map, (u32) key, ci);
		struct contact *c = net_list_address(pipeline->c_db.contacts, ci);

		if (ci < pipeline->c_db.contacts_frame_usage.bit_count)
		{
			bit_vec_set_bit(&pipeline->c_db.contacts_frame_usage, ci, 1);
		}
		PHYSICS_EVENT_CONTACT_NEW(pipeline, ci);

		return c;
	}
	else
	{
		struct contact *c = net_list_address(pipeline->c_db.contacts, index);
		bit_vec_set_bit(&pipeline->c_db.contacts_frame_usage, index, 1);
		c->cm = *cm;
		return c;
	}
}

void c_db_remove_contact(struct physics_pipeline *pipeline, const u64 key, const u32 index)
{
	struct contact *c = net_list_address(pipeline->c_db.contacts, index);
	struct rigid_body *body0 = physics_pipeline_rigid_body_lookup(pipeline, (u32) CONTACT_KEY_TO_BODY_0(c->key));
	struct rigid_body *body1 = physics_pipeline_rigid_body_lookup(pipeline, (u32) CONTACT_KEY_TO_BODY_1(c->key));
	
	if (body0->first_contact_index == index)
	{
		body0->first_contact_index = c->header.next[0];
	}

	if (body1->first_contact_index == index)
	{
		body1->first_contact_index = c->header.next[1];
	}

	PHYSICS_EVENT_CONTACT_REMOVED(pipeline, (u32) CONTACT_KEY_TO_BODY_0(c->key), (u32) CONTACT_KEY_TO_BODY_1(c->key));
	hash_map_remove(pipeline->c_db.contact_map, (u32) key, index);
	net_list_remove(pipeline->c_db.contacts, index);
}

void c_db_remove_body_contacts(struct physics_pipeline *pipeline, const u32 body_index)
{
	struct rigid_body *body = array_list_intrusive_address(pipeline->body_list, body_index);
	u32 ci = body->first_contact_index;
	body->first_contact_index = C_DB_NULL;
	while (ci != C_DB_NULL)
	{
		struct contact *c = net_list_address(pipeline->c_db.contacts, ci);
		u32 next_i;
		if (body_index == CONTACT_KEY_TO_BODY_0(c->key))
		{
			next_i = 0;
			body = array_list_intrusive_address(pipeline->body_list, CONTACT_KEY_TO_BODY_1(c->key));
		}
		else
		{
			next_i = 1;
			body = array_list_intrusive_address(pipeline->body_list, CONTACT_KEY_TO_BODY_0(c->key));
		}

		if (body->first_contact_index == ci)
		{
			body->first_contact_index = c->header.next[1-next_i];
		}
		const u32 ci_next = c->header.next[next_i];

		PHYSICS_EVENT_CONTACT_REMOVED(pipeline, (u32) CONTACT_KEY_TO_BODY_0(c->key), (u32) CONTACT_KEY_TO_BODY_1(c->key));
		bit_vec_set_bit(&pipeline->c_db.contacts_persistent_usage, ci, 0);
		hash_map_remove(pipeline->c_db.contact_map, (u32) c->key, ci);
		net_list_remove(pipeline->c_db.contacts, ci);
		ci = ci_next;
	}
}

u32 *c_db_remove_static_contacts_and_store_affected_islands(struct arena *mem, u32 *count, struct physics_pipeline *pipeline, const u32 static_index)
{
	u32 *array = (u32 *) mem->stack_ptr;
	*count = 0;

	struct rigid_body *body = array_list_intrusive_address(pipeline->body_list, static_index);
	kas_assert(body->island_index == ISLAND_STATIC);
	u32 ci = body->first_contact_index;
	body->first_contact_index = C_DB_NULL;
	while (ci != C_DB_NULL)
	{
		struct contact *c = net_list_address(pipeline->c_db.contacts, ci);
		u32 next_i;
		if (static_index == CONTACT_KEY_TO_BODY_0(c->key))
		{
			next_i = 0;
			body = array_list_intrusive_address(pipeline->body_list, CONTACT_KEY_TO_BODY_1(c->key));
		}
		else
		{
			next_i = 1;
			body = array_list_intrusive_address(pipeline->body_list, CONTACT_KEY_TO_BODY_0(c->key));
		}

		if (body->first_contact_index == ci)
		{
			body->first_contact_index = c->header.next[1-next_i];
		}
		const u32 ci_next = c->header.next[next_i];
		struct island *is = array_list_address(pipeline->is_db.islands, body->island_index);
		if ((is->flags & ISLAND_SPLIT) == 0)
		{
			arena_push_packed_memcpy(mem, &body->island_index, sizeof(body->island_index));
			is->flags |= ISLAND_SPLIT;
			*count += 1;
		}

		PHYSICS_EVENT_CONTACT_REMOVED(pipeline, (u32) CONTACT_KEY_TO_BODY_0(c->key), (u32) CONTACT_KEY_TO_BODY_1(c->key));
		bit_vec_set_bit(&pipeline->c_db.contacts_persistent_usage, ci, 0);
		hash_map_remove(pipeline->c_db.contact_map, (u32) c->key, ci);
		net_list_remove(pipeline->c_db.contacts, ci);
		ci = ci_next;
	}

	return array;
}

struct contact *c_db_lookup_contact(const struct contact_database *c_db, const u32 i1, const u32 i2)
{
	u32 b1, b2;
	if (i1 < i2)
	{
		b1 = i1;
		b2 = i2;
	}
	else
	{
		b1 = i2;
		b2 = i1;
	}

	const u64 key = key_gen_u32_u32(b1, b2);
	for (u32 i = hash_map_first(c_db->contact_map, (u32) key); i != HASH_NULL; i = hash_map_next(c_db->contact_map, i))
	{
		struct contact *c = net_list_address(c_db->contacts, i);
		if (c->key == key)
		{
			return c;
		}
	}

	return NULL;
}

u32 c_db_lookup_contact_index(const struct contact_database *c_db, const u32 i1, const u32 i2)
{
	u32 b1, b2;
	if (i1 < i2)
	{
		b1 = i1;
		b2 = i2;
	}
	else
	{
		b1 = i2;
		b2 = i1;
	}

	const u64 key = key_gen_u32_u32(b1, b2);
	for (u32 i = hash_map_first(c_db->contact_map, (u32) key); i != HASH_NULL; i = hash_map_next(c_db->contact_map, i))
	{
		struct contact *c = net_list_address(c_db->contacts, i);
		if (c->key == key)
		{
			return (u32) i;
		}
	}

	return NET_LIST_NODE_NULL_INDEX;
}
