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

#include "csg.h"
#include "sys_public.h"
#include "log.h"

/*
global command identifiers
==========================
*/

struct csg csg_alloc(void)
{
	struct csg csg;

	csg.brush_database = string_database_alloc(NULL, 32, 32, struct csg_brush, GROWABLE);
	csg.instance_pool = pool_alloc(NULL, 32, struct csg_instance, GROWABLE);
	csg.node_pool = pool_alloc(NULL, 32, struct csg_instance, GROWABLE);
	csg.frame = arena_alloc(1024*1024*1024);
	csg.brush_marked_list = list_init(struct csg_brush);
	csg.instance_marked_list = list_init(struct csg_instance);
	//csg.dcel_allocator = dcel_allocator_alloc(32, 32);

	struct csg_brush *stub_brush = string_database_address(&csg.brush_database, STRING_DATABASE_STUB_INDEX);
	stub_brush->primitive = CSG_PRIMITIVE_BOX;
	stub_brush->dcel = dcel_box();
	stub_brush->flags = CSG_FLAG_CONSTANT;
	stub_brush->delta = NULL;
	stub_brush->id_hash = utf8_hash(stub_brush->id);
	stub_brush->ui_index_cached = UI_NON_CACHED_INDEX;

	dcel_assert_topology(&stub_brush->dcel);

	return csg;
}

void csg_dealloc(struct csg *csg)
{
	string_database_free(&csg->brush_database);
	pool_dealloc(&csg->instance_pool);
	pool_dealloc(&csg->node_pool);
	arena_free(&csg->frame);
	//dcel_allocator_dealloc(csg->dcel_allocator);
}

void csg_flush(struct csg *csg)
{
	string_database_flush(&csg->brush_database);
	pool_flush(&csg->instance_pool);
	pool_flush(&csg->node_pool);
	arena_flush(&csg->frame);
	list_flush(&csg->brush_marked_list);
	list_flush(&csg->instance_marked_list);
	//dcel_allocator_flush(csg->dcel_allocator);
}

void csg_serialize(struct serialize_stream *ss, const struct csg *csg)
{

}

struct csg csg_deserialize(struct arena *mem, struct serialize_stream *ss, const u32 growable)
{
	kas_assert(!mem || !growable);
}

static void csg_apply_delta(struct csg *csg)
{

}

static void csg_remove_marked_structs(struct csg *csg)
{
	struct csg_brush *brush = NULL;
	for (u32 i = csg->brush_marked_list.first; i != LIST_NULL; i = LIST_NEXT(brush))
	{
		brush = string_database_address(&csg->brush_database, i);
		if (brush->flags & CSG_FLAG_CONSTANT || brush->reference_count)
		{
			brush->flags &= ~CSG_FLAG_MARKED_FOR_REMOVAL;
			continue;
		}

		utf8 id = brush->id;
		string_database_remove(&csg->brush_database, id);
		thread_free_256B(id.buf);
	}

	list_flush(&csg->brush_marked_list);
	list_flush(&csg->instance_marked_list);
}

void csg_main(struct csg *csg)
{
	/* (1) Apply deltas */
	csg_apply_delta(csg);

	/* (2) Safe to flush frame now */
	arena_flush(&csg->frame);

	/* (3) Remove markged csg structs */
	csg_remove_marked_structs(csg);
}

struct slot csg_brush_add(struct csg *csg, const utf8 id)
{
	if (id.size > 256)
	{
		log(T_CSG, S_WARNING, "Failed to create csg_brush, id %k requires size > 256B.", &id);
		return empty_slot; 
	}

	void *buf = thread_alloc_256B();
	const utf8 heap_id = utf8_copy_buffered(buf, 256, id);
	struct slot slot = string_database_add_and_alias(&csg->brush_database, heap_id);
	if (!slot.address)
	{
		log(T_CSG, S_WARNING, "Failed to create csg_brush, brush with id %k already exist.", &id);
		thread_free_256B(buf);
	}
	else
	{
		struct csg_brush *brush = slot.address;
		brush->primitive = CSG_PRIMITIVE_BOX;
		brush->dcel = dcel_box();
		brush->flags = CSG_FLAG_NONE;
		brush->delta = NULL;

		brush->id_hash = utf8_hash(brush->id);
		/* TODO:(Note) must also be reset when modifying brush id later on... */
		brush->ui_index_cached = UI_NON_CACHED_INDEX;
	}

	return slot;
}

void csg_brush_mark_for_removal(struct csg *csg, const utf8 id)
{
	struct slot slot = string_database_lookup(&csg->brush_database, id);
	struct csg_brush *brush = slot.address;
	if (brush && !(brush->flags & CSG_FLAG_CONSTANT))
	{
		brush->flags |= CSG_FLAG_MARKED_FOR_REMOVAL;
		list_append(&csg->brush_marked_list, csg->brush_database.pool.buf, slot.index);
	}
}
