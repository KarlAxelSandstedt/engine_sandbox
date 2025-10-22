#include "csg.h"
#include "sys_public.h"

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
	//csg.dcel_allocator = dcel_allocator_alloc(32, 32);

	struct csg_brush *stub_brush = string_database_address(&csg.brush_database, STRING_DATABASE_STUB_INDEX);
	stub_brush->primitive = CSG_PRIMITIVE_BOX;
	stub_brush->dcel = dcel_box();
	stub_brush->flags = CSG_FLAG_CONSTANT;
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

static void csg_remove_tagged_structs(struct csg *csg)
{

}

void csg_main(struct csg *csg)
{
	/* (1) Apply deltas */
	csg_apply_delta(csg);

	/* (2) Safe to flush frame now */
	arena_flush(&csg->frame);

	/* (3) Remove tagged csg structs */
	csg_remove_tagged_structs(csg);
}

