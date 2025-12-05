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

#include "sys_public.h"
#include "dynamics.h"

u32 c_db_index_in_previous_contact_node(struct nll *net, void **prev_node, const void *cur_node, const u32 cur_index)
{
	kas_assert(cur_index <= 1);
	const struct contact *c = cur_node;
	const u32 body = (1-cur_index) * CONTACT_KEY_TO_BODY_0(c->key) + cur_index * CONTACT_KEY_TO_BODY_1(c->key);
	
	*prev_node = nll_address(net, c->nll_prev[cur_index]);
	const u64 key = ((struct contact *) *prev_node)->key;
	kas_assert(c->nll_prev[cur_index] == NLL_NULL || body == CONTACT_KEY_TO_BODY_0(key) || body == CONTACT_KEY_TO_BODY_1(key));
	return (body == CONTACT_KEY_TO_BODY_0(key))
		? 0
		: 1;
}

u32 c_db_index_in_next_contact_node(struct nll *net, void **next_node, const void *cur_node, const u32 cur_index)
{
	kas_assert(cur_index <= 1);
	const struct contact *c = cur_node;
	const u32 body = (1-cur_index) * CONTACT_KEY_TO_BODY_0(c->key) + cur_index * CONTACT_KEY_TO_BODY_1(c->key);
	
	*next_node = nll_address(net, c->nll_next[cur_index]);
	const u64 key = ((struct contact *) *next_node)->key;
	kas_assert(c->nll_next[cur_index] == NLL_NULL || body == CONTACT_KEY_TO_BODY_0(key) || body == CONTACT_KEY_TO_BODY_1(key));
	return (body == CONTACT_KEY_TO_BODY_0(key))
		? 0
		: 1;
}

struct contact_database c_db_alloc(struct arena *mem_persistent, const u32 size)
{
	struct contact_database c_db = { 0 };
	kas_assert(is_power_of_two(size));

	c_db.sat_cache_list = dll_init(struct sat_cache);
	c_db.sat_cache_map = hash_map_alloc(NULL, size, size, GROWABLE);
	c_db.sat_cache_pool = pool_alloc(NULL, size, struct sat_cache, GROWABLE);
	c_db.contact_net = nll_alloc(NULL, size, struct contact, c_db_index_in_previous_contact_node, c_db_index_in_next_contact_node, GROWABLE);
	c_db.contact_map = hash_map_alloc(NULL, size, size, GROWABLE);
	c_db.contacts_persistent_usage = bit_vec_alloc(NULL, size, 0, 1);

	return c_db;
}

void c_db_free(struct contact_database *c_db)
{
	pool_dealloc(&c_db->sat_cache_pool);
	hash_map_free(c_db->sat_cache_map);
	nll_dealloc(&c_db->contact_net);
	hash_map_free(c_db->contact_map);
	bit_vec_free(&c_db->contacts_persistent_usage);
}

void c_db_flush(struct contact_database *c_db)
{
	c_db_clear_frame(c_db);
	dll_flush(&c_db->sat_cache_list);
	pool_flush(&c_db->sat_cache_pool);
	hash_map_flush(c_db->sat_cache_map);
	nll_flush(&c_db->contact_net);
	hash_map_flush(c_db->contact_map);
	bit_vec_clear(&c_db->contacts_persistent_usage, 0);
}

void c_db_validate(const struct physics_pipeline *pipeline)
{
	for (u64 i = 0; i < pipeline->c_db.contacts_persistent_usage.bit_count; ++i)
	{
		if (bit_vec_get_bit(&pipeline->c_db.contacts_persistent_usage, i))
		{
			const struct contact *c = nll_address(&pipeline->c_db.contact_net, (u32) i);
			kas_assert(POOL_SLOT_ALLOCATED(c));

			//fprintf(stderr, "contact[%lu] (next[0], next[1], prev[0], prev[1]) : (%u,%u,%u,%u)\n",
			//	       i,
			//	       c->nll_next[0],	
			//	       c->nll_next[1],	
			//	       c->nll_prev[0],	
			//	       c->nll_prev[1]);

			const struct rigid_body *b1 = pool_address(&pipeline->body_pool, c->cm.i1);
			const struct rigid_body *b2 = pool_address(&pipeline->body_pool, c->cm.i2);

			u32 prev, k, found; 
			prev = NLL_NULL;
			k = b1->first_contact_index;
			found = 0;
			while (k != NLL_NULL)
			{
				if (k == i)
				{
					found = 1;
					break;
				}

				const struct contact *tmp = nll_address(&pipeline->c_db.contact_net, k);
				kas_assert(POOL_SLOT_ALLOCATED(tmp));
				if (CONTACT_KEY_TO_BODY_0(tmp->key) == c->cm.i1)
				{
					kas_assert(prev == tmp->nll_prev[0]);
					prev = k;
					k = tmp->nll_next[0];
				}
				else
				{
					kas_assert(CONTACT_KEY_TO_BODY_1(tmp->key) == c->cm.i1);
					kas_assert(prev == tmp->nll_prev[1]);
					prev = k;
					k = tmp->nll_next[1];
				}
			}
			kas_assert(found);
 
			prev = NLL_NULL;
			k = b2->first_contact_index;
			found = 0;
			while (k != NLL_NULL)
			{
				if (k == i)
				{
					found = 1;
					break;
				}

				const struct contact *tmp = nll_address(&pipeline->c_db.contact_net, k);
				kas_assert(POOL_SLOT_ALLOCATED(tmp));
				if (CONTACT_KEY_TO_BODY_0(tmp->key) == c->cm.i2)
				{
					kas_assert(prev == tmp->nll_prev[0]);
					prev = k;
					k = tmp->nll_next[0];
				}
				else
				{
					kas_assert(prev == tmp->nll_prev[1]);
					kas_assert(CONTACT_KEY_TO_BODY_1(tmp->key) == c->cm.i2);
					prev = k;
					k = tmp->nll_next[1];
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

	if (c_db->contacts_persistent_usage.bit_count < c_db->contact_net.pool.count_max)
	{
		const u64 low_bit = c_db->contacts_persistent_usage.bit_count;
		const u64 high_bit = c_db->contact_net.pool.count_max;
		bit_vec_increase_size(&c_db->contacts_persistent_usage, c_db->contact_net.pool.length, 0);
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

	//fprintf(stderr, "count: %u\n", c_db->sat_cache_pool.count);
	for (u32 i = c_db->sat_cache_list.first; i != DLL_NULL; )
	{
		struct sat_cache *cache = pool_address(&c_db->sat_cache_pool, i);
		const u32 next = DLL_NEXT(cache);
		if (cache->touched)
		{
			cache->touched = 0;
		}
		else
		{
			dll_remove(&c_db->sat_cache_list, c_db->sat_cache_pool.buf, i);
			hash_map_remove(c_db->sat_cache_map, (u32) cache->key, i);
			pool_remove(&c_db->sat_cache_pool, i);
		}
		i = next;
	}
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

	struct rigid_body *body1 = pool_address(&pipeline->body_pool, b1);
	struct rigid_body *body2 = pool_address(&pipeline->body_pool, b2);

	const u64 key = key_gen_u32_u32(b1, b2);
	kas_assert(b1 == CONTACT_KEY_TO_BODY_0(key));
	kas_assert(b2 == CONTACT_KEY_TO_BODY_1(key));
	const u32 index = c_db_lookup_contact_index(&pipeline->c_db, b1, b2);

	if (index == NLL_NULL)
	{
		/* smaller valued body owns slot 0, larger valued body owns slot 1 in node header */
		kas_assert(POOL_SLOT_ALLOCATED(body1));
		kas_assert(POOL_SLOT_ALLOCATED(body2));
		struct contact cpy =
		{
			.cm = *cm,
			.key = key,
			.cached_count = 0,
		};
		struct slot slot = nll_add(&pipeline->c_db.contact_net, &cpy, body1->first_contact_index, body2->first_contact_index);
		const u32 ci = slot.index; 
		struct contact *c = slot.address; 

		body1->first_contact_index = ci;
		body2->first_contact_index = ci;

		hash_map_add(pipeline->c_db.contact_map, (u32) key, ci);

		if (ci < pipeline->c_db.contacts_frame_usage.bit_count)
		{
			bit_vec_set_bit(&pipeline->c_db.contacts_frame_usage, ci, 1);
		}
		PHYSICS_EVENT_CONTACT_NEW(pipeline, b1, b2);

		return c;
	}
	else
	{
		struct contact *c = nll_address(&pipeline->c_db.contact_net, index);
		bit_vec_set_bit(&pipeline->c_db.contacts_frame_usage, index, 1);
		c->cm = *cm;
		return c;
	}
}

void c_db_remove_contact(struct physics_pipeline *pipeline, const u64 key, const u32 index)
{
	struct contact *c = nll_address(&pipeline->c_db.contact_net, index);
	struct rigid_body *body0 = pool_address(&pipeline->body_pool, (u32) CONTACT_KEY_TO_BODY_0(c->key));
	struct rigid_body *body1 = pool_address(&pipeline->body_pool, (u32) CONTACT_KEY_TO_BODY_1(c->key));
	
	if (body0->first_contact_index == index)
	{
		body0->first_contact_index = c->nll_next[0];
	}

	if (body1->first_contact_index == index)
	{
		body1->first_contact_index = c->nll_next[1];
	}

	PHYSICS_EVENT_CONTACT_REMOVED(pipeline, (u32) CONTACT_KEY_TO_BODY_0(c->key), (u32) CONTACT_KEY_TO_BODY_1(c->key));
	hash_map_remove(pipeline->c_db.contact_map, (u32) key, index);
	nll_remove(&pipeline->c_db.contact_net, index);
}

void c_db_remove_body_contacts(struct physics_pipeline *pipeline, const u32 body_index)
{
	struct rigid_body *body = pool_address(&pipeline->body_pool, body_index);
	u32 ci = body->first_contact_index;
	body->first_contact_index = NLL_NULL;
	while (ci != NLL_NULL)
	{
		struct contact *c = nll_address(&pipeline->c_db.contact_net, ci);
		struct sat_cache *sat = sat_cache_lookup(&pipeline->c_db, CONTACT_KEY_TO_BODY_0(c->key), CONTACT_KEY_TO_BODY_1(c->key));
		if (sat)
		{
			const u32 sat_index = pool_index(&pipeline->c_db.sat_cache_pool, sat);
			dll_remove(&pipeline->c_db.sat_cache_list, pipeline->c_db.sat_cache_pool.buf, sat_index);
			hash_map_remove(pipeline->c_db.sat_cache_map, (u32) c->key, sat_index);
			pool_remove(&pipeline->c_db.sat_cache_pool, sat_index);
		}

		u32 next_i;
		if (body_index == CONTACT_KEY_TO_BODY_0(c->key))
		{
			next_i = 0;
			body = pool_address(&pipeline->body_pool, CONTACT_KEY_TO_BODY_1(c->key));
		}
		else
		{
			next_i = 1;
			body = pool_address(&pipeline->body_pool, CONTACT_KEY_TO_BODY_0(c->key));
		}

		if (body->first_contact_index == ci)
		{
			body->first_contact_index = c->nll_next[1-next_i];
		}
		const u32 ci_next = c->nll_next[next_i];

		PHYSICS_EVENT_CONTACT_REMOVED(pipeline, (u32) CONTACT_KEY_TO_BODY_0(c->key), (u32) CONTACT_KEY_TO_BODY_1(c->key));
		bit_vec_set_bit(&pipeline->c_db.contacts_persistent_usage, ci, 0);
		hash_map_remove(pipeline->c_db.contact_map, (u32) c->key, ci);
		nll_remove(&pipeline->c_db.contact_net, ci);
		ci = ci_next;
	}
}

u32 *c_db_remove_static_contacts_and_store_affected_islands(struct arena *mem, u32 *count, struct physics_pipeline *pipeline, const u32 static_index)
{
	u32 *array = (u32 *) mem->stack_ptr;
	*count = 0;

	struct rigid_body *body = pool_address(&pipeline->body_pool, static_index);
	kas_assert(body->island_index == ISLAND_STATIC);
	u32 ci = body->first_contact_index;
	body->first_contact_index = NLL_NULL;
	while (ci != NLL_NULL)
	{
		struct contact *c = nll_address(&pipeline->c_db.contact_net, ci);
		u32 next_i;
		if (static_index == CONTACT_KEY_TO_BODY_0(c->key))
		{
			next_i = 0;
			body = pool_address(&pipeline->body_pool, CONTACT_KEY_TO_BODY_1(c->key));
		}
		else
		{
			next_i = 1;
			body = pool_address(&pipeline->body_pool, CONTACT_KEY_TO_BODY_0(c->key));
		}

		if (body->first_contact_index == ci)
		{
			body->first_contact_index = c->nll_next[1-next_i];
		}
		const u32 ci_next = c->nll_next[next_i];
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
		nll_remove(&pipeline->c_db.contact_net, ci);
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
		struct contact *c = nll_address(&c_db->contact_net, i);
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
	u32 ret = NLL_NULL;
	for (u32 i = hash_map_first(c_db->contact_map, (u32) key); i != HASH_NULL; i = hash_map_next(c_db->contact_map, i))
	{
		struct contact *c = nll_address(&c_db->contact_net, i);
		if (c->key == key)
		{
			ret = (u32) i;
			break;
		}
	}

	return ret;
}

void sat_cache_add(struct contact_database *c_db, const struct sat_cache *sat_cache)
{
	const u32 b0 = CONTACT_KEY_TO_BODY_0(sat_cache->key);
	const u32 b1 = CONTACT_KEY_TO_BODY_1(sat_cache->key);
	kas_assert(sat_cache_lookup(c_db, b0, b1) == NULL);

	//breakpoint(b0 == 62 && b1 == 66);
	struct slot slot = pool_add(&c_db->sat_cache_pool);
	struct sat_cache *sat = slot.address;
	const u32 slot_allocation_state = sat->slot_allocation_state;
	*sat = *sat_cache;
	sat->slot_allocation_state = slot_allocation_state;
	dll_append(&c_db->sat_cache_list, c_db->sat_cache_pool.buf, slot.index);
	hash_map_add(c_db->sat_cache_map, (u32) sat_cache->key, slot.index);
	sat->touched = 1;
}

struct sat_cache *sat_cache_lookup(const struct contact_database *c_db, const u32 b1, const u32 b2)
{
	kas_assert(b1 < b2);
	const u64 key = key_gen_u32_u32(b1, b2);
	struct sat_cache *ret = NULL;
	for (u32 i = hash_map_first(c_db->sat_cache_map, (u32) key); i != HASH_NULL; i = hash_map_next(c_db->sat_cache_map, i))
	{
		struct sat_cache *sat = pool_address(&c_db->sat_cache_pool, i);
		if (sat->key == key)
		{
			ret = sat;
			break;
		}
	}

	return ret;
}
