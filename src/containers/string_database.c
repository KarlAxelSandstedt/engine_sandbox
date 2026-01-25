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

struct string_database	string_database_alloc_internal(struct arena *mem, const u32 hash_size, const u32 index_size, const u64 data_size, const u64 id_offset, const u64 reference_count_offset, const u64 allocated_prev_offset, const u64 allocated_next_offset, const u64 pool_state_offset, const u32 growable)
{
	ds_Assert(!growable || !mem);
	ds_Assert(index_size && hash_size);

	u32 heap_allocated;
	struct pool pool;
	struct hash_map *hash = NULL;
	struct string_database db = { 0 };
	if (mem)
	{
		heap_allocated = 0;
		hash = hash_map_alloc(mem, hash_size, index_size, 0);
		pool = PoolAllocInternal(mem, index_size, data_size, pool_state_offset, U64_MAX, 0);
	}
	else
	{
		heap_allocated = 1;
		hash = hash_map_alloc(NULL, hash_size, index_size, growable);
		pool = PoolAllocInternal(mem, index_size, data_size, pool_state_offset, U64_MAX, growable);
	}

	if (!hash || !pool.length)
	{
		log_string(T_SYSTEM, S_FATAL, "Failed to allocate string_database");
		fatal_cleanup_and_exit(ds_thread_self_tid());
	}

	db.hash = hash;
	db.pool = pool;
	db.growable = growable;
	db.heap_allocated = heap_allocated;
	db.id_offset = id_offset;
	db.reference_count_offset = reference_count_offset;
	db.allocated_prev_offset = allocated_prev_offset;
	db.allocated_next_offset = allocated_next_offset;
	db.allocated_dll = dll_init_internal(data_size, allocated_prev_offset, allocated_next_offset);

	const utf8 stub_id = Utf8Empty();
	const u32 key = Utf8Hash(stub_id);

	struct slot slot = PoolAdd(&db.pool);
	hash_map_add(db.hash, key, slot.index);
	struct string_database_node *node = slot.address;

	utf8 *id = (utf8 *)(((u8 *) slot.address) + db.id_offset);
	*id = Utf8Empty();

	u32 *reference_count = (u32 *)(((u8 *) slot.address) + db.reference_count_offset);
	*reference_count = 0;

	ds_Assert(slot.index == STRING_DATABASE_STUB_INDEX);

	return db;
}

void string_database_free(struct string_database *db)
{
	if (db->heap_allocated)
	{
		PoolDealloc(&db->pool);
		hash_map_free(db->hash);
	}
}

void string_database_flush(struct string_database *db)
{
	hash_map_flush(db->hash);
	PoolFlush(&db->pool);
	dll_flush(&db->allocated_dll);
	const utf8 stub_id = Utf8Empty();
	const u32 key = Utf8Hash(stub_id);

	struct slot slot = PoolAdd(&db->pool);
	hash_map_add(db->hash, key, slot.index);
	struct string_database_node *node = slot.address;

	utf8 *id = (utf8 *)(((u8 *) slot.address) + db->id_offset);
	*id = stub_id;

	u32 *reference_count = (u32 *)(((u8 *) slot.address) + db->reference_count_offset);
	*reference_count = 0;

	ds_Assert(slot.index == STRING_DATABASE_STUB_INDEX);
}

struct slot string_database_add(struct arena *mem_db_lifetime, struct string_database *db, const utf8 copy)
{
	struct slot slot = { .index = STRING_DATABASE_STUB_INDEX, .address = db->pool.buf };
	if (string_database_lookup(db, copy).index != STRING_DATABASE_STUB_INDEX)
	{
		return slot;
	}

	utf8 id = Utf8Copy(mem_db_lifetime, copy);
	if (id.buf)
	{
		const u32 key = Utf8Hash(copy);
		struct slot slot = PoolAdd(&db->pool);
		hash_map_add(db->hash, key, slot.index);

		utf8 *id_ptr = (utf8 *)(((u8 *) slot.address) + db->id_offset);
		*id_ptr = id;

		u32 *reference_count = (u32 *)(((u8 *) slot.address) + db->reference_count_offset);
		*reference_count = 0;

		dll_append(&db->allocated_dll, db->pool.buf, slot.index);
	}

	return slot;
}

struct slot string_database_add_and_alias(struct string_database *db, const utf8 id)
{
	struct slot slot = { .index = STRING_DATABASE_STUB_INDEX, .address = db->pool.buf };
	if (string_database_lookup(db, id).index != STRING_DATABASE_STUB_INDEX)
	{
		return slot;
	}

	slot = PoolAdd(&db->pool);
	const u32 key = Utf8Hash(id);
	hash_map_add(db->hash, key, slot.index);

	utf8 *id_ptr = (utf8 *)(((u8 *) slot.address) + db->id_offset);
	*id_ptr = id;

	u32 *reference_count = (u32 *)(((u8 *) slot.address) + db->reference_count_offset);
	*reference_count = 0;
		
	dll_append(&db->allocated_dll, db->pool.buf, slot.index);

	return slot;
}

void string_database_remove(struct string_database *db, const utf8 id)
{
	const struct slot slot = string_database_lookup(db, id);
	if (slot.index != STRING_DATABASE_STUB_INDEX)
	{
		ds_Assert(*(u32 *)((u8 *) slot.address + db->reference_count_offset) == 0);
		const u32 key = Utf8Hash(*(utf8 *)((u8 *) slot.address + db->id_offset));
		hash_map_remove(db->hash, key, slot.index);
		PoolRemove(&db->pool, slot.index);
		dll_remove(&db->allocated_dll, db->pool.buf, slot.index);
	}
}

struct slot string_database_lookup(const struct string_database *db, const utf8 id)
{
	const u32 key = Utf8Hash(id);
	struct slot slot = { .index = STRING_DATABASE_STUB_INDEX, .address = db->pool.buf };
	for (u32 i = hash_map_first(db->hash, key); i != HASH_NULL; i = hash_map_next(db->hash, i))
	{
		u8 *address = string_database_address(db, i);
		utf8 *id_ptr = (utf8 *) (address + db->id_offset);
		if (Utf8Equivalence(id, *id_ptr))
		{
			slot.index = i;
			slot.address = address;
			break;
		}
	}

	return slot;
}

void *string_database_address(const struct string_database *db, const u32 handle)
{
	u8 *address = PoolAddress(&db->pool, handle);
	ds_Assert((*(u32 *)(address + db->pool.slot_allocation_offset)) & 0x80000000);
	return address;
}

struct slot string_database_reference(struct string_database *db, const utf8 id)
{
	struct slot slot = string_database_lookup(db, id);
	u32 *reference_count = (u32 *)(((u8 *) slot.address) + db->reference_count_offset);
	*reference_count += 1;
	return slot;
}

void string_database_dereference(struct string_database *db, const u32 handle)
{
	u32 *reference_count = (u32 *)(((u8 *) string_database_address(db, handle)) + db->reference_count_offset);
	ds_Assert(*reference_count || handle == STRING_DATABASE_STUB_INDEX);
	*reference_count -= 1;
}
