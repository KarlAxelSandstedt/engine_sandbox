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

#include "string_database.h"
#include "sys_public.h"

struct string_database *string_database_init(struct arena *mem, const u32 hash_size, const u32 index_size, const u64 data_size, const u32 growable)
{
	kas_assert(index_size && hash_size);
	struct string_database *db = NULL;
	struct hash_map *hash = NULL;
	struct array_list_intrusive *list = NULL;
	if (mem)
	{
		kas_assert(!growable);
		db = arena_push(mem, sizeof(struct string_database));
		hash = hash_map_alloc(mem, hash_size, index_size, 0);
		list = array_list_intrusive_alloc(mem, index_size, data_size, 0);
	}
	else
	{
		db = malloc(sizeof(struct string_database));
		hash = hash_map_alloc(NULL, hash_size, index_size, growable);
		list = array_list_intrusive_alloc(NULL, index_size, data_size, growable);
	}

	if (!db || !hash || !list)
	{
		log_string(T_SYSTEM, S_FATAL, "Failed to allocate string_database");
		fatal_cleanup_and_exit(kas_thread_self_tid());
	}

	db->hash = hash;
	db->list = list;
	db->growable = growable;
	const utf8 stub_id = utf8_empty();
	const u32 key = utf8_hash(stub_id);
	u32 index = array_list_intrusive_reserve_index(db->list);
	hash_map_add(db->hash, key, index);
	struct string_database_node *node = array_list_intrusive_address(db->list, index);
	node->reference_count = 0;
	node->id = stub_id;
	kas_assert(index == STRING_DATABASE_STUB_INDEX);

	return db;
}

void string_database_free(struct string_database *db)
{
	if (db)
	{
		array_list_intrusive_free(db->list);
		hash_map_free(db->hash);
		free(db);
	}
}

void string_database_flush(struct string_database *db)
{
	hash_map_flush(db->hash);
	array_list_intrusive_flush(db->list);
	const utf8 stub_id = utf8_empty();
	const u32 key = utf8_hash(stub_id);
	u32 index = array_list_intrusive_reserve_index(db->list);
	hash_map_add(db->hash, key, index);
	struct string_database_node *node = array_list_intrusive_address(db->list, index);
	node->reference_count = 0;
	node->id = stub_id;
	kas_assert(index == STRING_DATABASE_STUB_INDEX);
}

struct allocation_slot string_database_add(struct arena *mem_db_lifetime, struct string_database *db, const utf8 copy)
{
	kas_assert(string_database_lookup(db, copy).index == STRING_DATABASE_STUB_INDEX);

	utf8 id = utf8_copy(mem_db_lifetime, copy);
	struct allocation_slot slot = { .index = STRING_DATABASE_STUB_INDEX, .address = NULL };
	if (id.buf)
	{
		const u32 key = utf8_hash(copy);
		slot = array_list_intrusive_add(db->list);
		hash_map_add(db->hash, key, slot.index);
		struct string_database_node *node = slot.address;
		node->reference_count = 0;
		node->id = id;
	}

	return slot;
}

struct allocation_slot string_database_add_and_alias(struct string_database *db, const utf8 id)
{
	kas_assert(string_database_lookup(db, id).index == STRING_DATABASE_STUB_INDEX);

	struct allocation_slot slot = array_list_intrusive_add(db->list);
	const u32 key = utf8_hash(id);
	hash_map_add(db->hash, key, slot.index);

	return slot;
}

void string_database_remove(struct string_database *db, const utf8 id)
{
	const struct allocation_slot slot = string_database_lookup(db, id);
	if (slot.index != STRING_DATABASE_STUB_INDEX)
	{
		kas_assert(((struct string_database_node *) slot.address)->list_header.allocated);
		kas_assert(((struct string_database_node *) slot.address)->reference_count == 0);
		const u32 key = utf8_hash(((struct string_database_node *)slot.address)->id);
		hash_map_remove(db->hash, key, slot.index);
		array_list_intrusive_remove_index(db->list, slot.index);
	}
}

struct allocation_slot string_database_lookup(const struct string_database *db, const utf8 id)
{
	const u32 key = utf8_hash(id);
	struct allocation_slot slot = { .index = STRING_DATABASE_STUB_INDEX, .address = NULL };
	for (u32 i = hash_map_first(db->hash, key); i != HASH_NULL; i = hash_map_next(db->hash, i))
	{
		struct string_database_node *node = string_database_address(db, i);
		if (utf8_equivalence(id, node->id))
		{
			slot.index = i;
			slot.address = node;
			break;
		}
	}

	return slot;
}

void *string_database_address(const struct string_database *db, const u32 handle)
{
	struct string_database_node *node = array_list_intrusive_address(db->list, handle);
	kas_assert(node->list_header.allocated);
	return (node->list_header.allocated)
		? node
		: string_database_address(db, STRING_DATABASE_STUB_INDEX);
}

struct allocation_slot string_database_reference(struct string_database *db, const utf8 id)
{
	struct allocation_slot slot = string_database_lookup(db, id);
	if (slot.index != STRING_DATABASE_STUB_INDEX)
	{
		struct string_database_node *node = slot.address;
		node->reference_count += 1;
	}
	return slot;
}

void string_database_dereference(struct string_database *db, const u32 handle)
{
	struct string_database_node *node = string_database_address(db, handle);
	kas_assert(node->reference_count);
	node->reference_count -= 1;
}
