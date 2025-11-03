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

#include <stdlib.h>
#include <string.h>
#include "physics_pipeline.h"
#include "contact_solver.h"
#include "collision.h"
#include "kas_profiler.h"
#include "island.h"
#include "float32.h"
#include "bit_vector.h"

struct physics_pipeline	physics_pipeline_alloc(struct arena *mem, const u32 initial_size, const u64 ns_tick, const u64 frame_memory, struct string_database *shape_db, struct string_database *prefab_db)
{
	COLLISION_DEBUG_INIT(mem, initial_size, 10000);

	struct physics_pipeline pipeline =
	{
		.gravity = { 0.0f, -GRAVITY_CONSTANT_DEFAULT, 0.0f },
		.c_state = { 0 },
		.ns_tick = ns_tick,
		.ns_elapsed = 0,
		.frame = arena_alloc(frame_memory),
		.event_count = 0,
		.frames_completed = 0,
	};

	vec4_set(pipeline.debug.island_static_color, 0.6f, 0.6f, 0.6f, 1.0f);
	vec4_set(pipeline.debug.island_sleeping_color, 113.0f/256.0f, 241.0f/256.0f, 157.0f/256.0f, 1.0f);
	vec4_set(pipeline.debug.island_awake_color, 255.0f/256.0f, 36.0f/256.0f, 48.0f/256.0f, 1.0f);

	static u32 init_solver_once = 0;
	if (!init_solver_once)
	{
		init_solver_once = 1;
		const u32 iteration_count = 10;
		const u32 block_solver = 0; 
		const u32 warmup_solver = 1;
		const vec3 gravity = { 0.0f, -GRAVITY_CONSTANT_DEFAULT, 0.0f };
       		const f32 baumgarte_constant = 0.1f;
		const f32 max_condition = 1000.0f;
		const f32 linear_dampening = 0.1f;
		const f32 angular_dampening = 0.1f;
		const f32 linear_slop = 0.0005f;
		const f32 restitution_threshold = 0.0005f;
		const u32 sleep_enabled = 1;
		const f32 sleep_time_threshold = 0.5f;
		f32 sleep_linear_velocity_sq_limit = 0.001f*0.001f; 
		f32 sleep_angular_velocity_sq_limit = 0.01f*0.01f*2.0f*F32_PI;
		contact_solver_config_init(iteration_count, block_solver, warmup_solver, gravity, baumgarte_constant, max_condition, linear_dampening, angular_dampening, linear_slop, restitution_threshold, sleep_enabled, sleep_time_threshold, sleep_linear_velocity_sq_limit, sleep_angular_velocity_sq_limit);

		pipeline.event_len = 1024;
		pipeline.event = malloc(pipeline.event_len * sizeof(struct physics_event));
	}

	kas_assert_string(is_power_of_two(initial_size), "For simplicity of future data structures, expect pipeline sizes to be powers of two");
	pipeline.body_list = array_list_intrusive_alloc(NULL, initial_size, sizeof(struct rigid_body), ARRAY_LIST_GROWABLE);

	pipeline.c_state.margin_on = 1;
	pipeline.c_state.margin = COLLISION_MARGIN_DEFAULT;

	pipeline.dynamic_tree = dbvt_alloc(mem, 2*initial_size);

	pipeline.c_db = c_db_alloc(mem, initial_size);
	pipeline.is_db = is_db_alloc(mem, initial_size);
	pipeline.shape_db = shape_db;

	return pipeline;
}

void physics_pipeline_free(struct physics_pipeline *pipeline)
{
	dbvt_free(&pipeline->dynamic_tree);
	c_db_free(&pipeline->c_db);
	is_db_free(&pipeline->is_db);
	array_list_intrusive_free(pipeline->body_list);
	free(pipeline->event);
}

void physics_pipeline_flush(struct physics_pipeline *pipeline)
{
	collision_state_clear_frame(&pipeline->c_state);
	array_list_intrusive_flush(pipeline->body_list);
	dbvt_flush(&pipeline->dynamic_tree);
	c_db_flush(&pipeline->c_db);
	is_db_flush(&pipeline->is_db);
	
	arena_flush(&pipeline->frame);
	pipeline->frames_completed = 0;
	pipeline->ns_elapsed = 0;
	pipeline->event_count = 0;
}

void physics_pipeline_tick(struct physics_pipeline *pipeline)
{
	KAS_TASK(__func__, T_PHYSICS);

	if (pipeline->frames_completed > 0)
	{
		physics_pipeline_clear_frame(pipeline);
	}
	const f32 delta = (f32) pipeline->ns_tick / NSEC_PER_SEC;
	pipeline->frames_completed += 1;
	internal_physics_pipeline_simulate_frame(pipeline, delta);

	KAS_END;
}

u32 physics_pipeline_rigid_body_alloc(struct physics_pipeline *pipeline)
{
	const u32 handle = array_list_intrusive_reserve_index(pipeline->body_list);
	PHYSICS_EVENT_BODY_NEW(pipeline, handle);
	return handle;
}

struct rigid_body *physics_pipeline_rigid_body_lookup(const struct physics_pipeline *pipeline, const u32 handle)
{

	struct rigid_body *body = array_list_intrusive_address(pipeline->body_list, handle);
	return (body->header.allocated) 
		? body 
		: NULL;
}

void physics_pipeline_rigid_body_dealloc(struct physics_pipeline *pipeline, const u32 handle)
{
	struct rigid_body *body = physics_pipeline_rigid_body_lookup(pipeline, handle);
	kas_assert(body->header.allocated);

	string_database_dereference(pipeline->shape_db, body->shape_handle);
	dbvt_remove(&pipeline->dynamic_tree, body->proxy);
	if (body->island_index != ISLAND_STATIC)
	{
		is_db_island_remove_body_resources(pipeline, body->island_index, handle);
		c_db_remove_body_contacts(pipeline, handle);
		if (bit_vec_get_bit(&pipeline->is_db.island_usage, body->island_index) && ((struct island *) array_list_address(pipeline->is_db.islands, body->island_index))->contact_count > 0)
		{
			is_db_split_island(&pipeline->frame, pipeline, body->island_index);
		}
	}
	else
	{
		arena_push_record(&pipeline->frame);
		u32 island_count;
		u32 *island = c_db_remove_static_contacts_and_store_affected_islands(&pipeline->frame, &island_count, pipeline, handle);
		for (u32 i = 0; i < island_count; ++i)
		{
			struct island *is = array_list_address(pipeline->is_db.islands, island[i]);
			struct is_index_entry *prev = NULL;
			struct is_index_entry *entry;
			u32 prev_index = ISLAND_NULL;
			u32 index = is->contact_first;
			do 
			{
				entry = array_list_address(pipeline->is_db.island_contact_lists, index);
				const u32 next = entry->next;
				if (bit_vec_get_bit(&pipeline->c_db.contacts_persistent_usage, entry->index) == 0)
				{
					if (prev)
					{
						prev->next = entry->next;
					}
					else
					{
						is->contact_first = entry->next;
					}
					entry->next = ISLAND_NULL;
					entry->index = ISLAND_NULL;
					array_list_remove_index(pipeline->is_db.island_contact_lists, index);
					is->contact_count -= 1;
				}
				else
				{
					prev_index = index;
					prev = entry;
				}
				index = next;
			} 
			while (index != ISLAND_NULL);

			is->contact_last = prev_index;
			if (is->contact_count > 0)
			{
				is_db_split_island(&pipeline->frame, pipeline, island[i]);
			}
			else
			{
				is->flags &= ~(ISLAND_SPLIT);
				if (!(is->flags & ISLAND_AWAKE))
				{
					PHYSICS_EVENT_ISLAND_AWAKE(pipeline, island[i]);	
				}
				is->flags |= ISLAND_SLEEP_RESET | ISLAND_AWAKE;
			}
		}
		arena_pop_record(&pipeline->frame);
	}
	array_list_intrusive_remove(pipeline->body_list, body);
	PHYSICS_EVENT_BODY_REMOVED(pipeline, handle);
}

void physics_pipeline_validate(const struct physics_pipeline *pipeline)
{
	KAS_TASK(__func__, T_PHYSICS);
	c_db_validate(pipeline);
	is_db_validate(pipeline);
	KAS_END;
}

u32 physics_pipeline_rigid_body_add(struct physics_pipeline *pipeline, const utf8 shape_id, const vec3 translation, const f32 density, const u32 dynamic, const f32 restitution, const f32 friction)
{
	const u32 handle  = physics_pipeline_rigid_body_alloc(pipeline);
	struct rigid_body *body = physics_pipeline_rigid_body_lookup(pipeline, handle);

	memset(((u8 *) body) + sizeof(struct array_list_intrusive_node), 0, sizeof(struct rigid_body) - sizeof(struct array_list_intrusive_node));

	vec3 axis = { 0.0f, 1.0f, 0.0f };
	axis_angle_to_quaternion(body->rotation, axis, 0.0f);
	vec3_copy(body->position, translation);
	vec3_set(body->angular_velocity, 0.0f, 0.0f, 0.0f);

	const u32 dynamic_flag = (dynamic) ? RB_DYNAMIC : 0;
	body->flags = RB_ACTIVE | (g_solver_config->sleep_enabled * RB_AWAKE) | dynamic_flag;
	body->margin = 0.25f;

	const struct slot slot = string_database_reference(pipeline->shape_db, shape_id);
	const struct collision_shape *shape = slot.address;
	body->shape_handle = slot.index;
	body->shape_type = shape->type;

	body->restitution = restitution;
	body->friction = friction;
	body->low_velocity_time = 0.0f;
	vec3_set(body->position, 0.0f, 0.0f, 0.0f);
	statics_setup(body, shape, density);
	vec3_translate(body->position, translation);	

	struct AABB proxy;
	rigid_body_update_local_box(body, shape);
	rigid_body_proxy(&proxy, body);
	body->proxy = dbvt_insert(&pipeline->dynamic_tree, handle, &proxy);

	body->first_contact_index = NET_LIST_NODE_NULL_INDEX;
	if (body->flags & RB_DYNAMIC)
	{
		is_db_init_island_from_body(pipeline, handle);
	}
	else
	{
		body->island_index = ISLAND_STATIC;
	}
	

	return handle;
}

static void internal_update_dynamic_tree(struct physics_pipeline *pipeline)
{
	KAS_TASK(__func__, T_PHYSICS);
	struct AABB world_AABB;

	const u32 flags = RB_ACTIVE | RB_DYNAMIC | (g_solver_config->sleep_enabled * RB_AWAKE);
	//TODO use dll...
	for (u32 i = 0; i < pipeline->body_list->max_count; ++i)
	{
		struct rigid_body *b = physics_pipeline_rigid_body_lookup(pipeline, i);
		if (b && (b->flags & flags) == flags)
		{
			const struct collision_shape *shape = string_database_address(pipeline->shape_db, b->shape_handle);
			rigid_body_update_local_box(b, shape);
			vec3_add(world_AABB.center, b->local_box.center, b->position);
			vec3_copy(world_AABB.hw, b->local_box.hw);
			const struct AABB *proxy = &pipeline->dynamic_tree.nodes[b->proxy].box;
			if (!AABB_contains(proxy, &world_AABB))
			{
				world_AABB.hw[0] += b->margin;
				world_AABB.hw[1] += b->margin;
				world_AABB.hw[2] += b->margin;
				dbvt_remove(&pipeline->dynamic_tree, b->proxy);
				b->proxy = dbvt_insert(&pipeline->dynamic_tree, i, &world_AABB);
			}
		}
	}
	KAS_END;
}

static void internal_push_proxy_overlaps(struct arena *mem_frame, struct physics_pipeline *pipeline)
{
	KAS_TASK(__func__, T_PHYSICS);
	pipeline->c_state.proxy_overlap = dbvt_push_overlap_pairs(mem_frame, &pipeline->c_state.overlap_count, &pipeline->dynamic_tree);
	KAS_END;
}

struct tpc_output
{
	struct contact_manifold *cm;
	u32 cm_count;
};

static void *thread_push_contacts(void *task_addr)
{
	KAS_TASK("contact creation", T_PHYSICS);

	const struct task *task = task_addr;
	struct worker *worker = task->executor;
	const struct task_range *range = task->range;
	const struct physics_pipeline *pipeline = task->input;
	const struct dbvt_overlap *proxy_overlap = range->base;

	struct tpc_output *out = arena_push(&worker->mem_frame, sizeof(struct tpc_output));
	out->cm_count = 0;
	out->cm = arena_push(&worker->mem_frame, range->count * sizeof(struct contact_manifold));

	const f32 margin = (pipeline->c_state.margin_on) ? pipeline->c_state.margin : 0.0f;
	const struct rigid_body *b1, *b2;

	for (u64 i = 0; i < range->count; ++i)
	{
		b1 = physics_pipeline_rigid_body_lookup(pipeline, proxy_overlap[i].id1);
		b2 = physics_pipeline_rigid_body_lookup(pipeline, proxy_overlap[i].id2);

		if (body_body_contact_manifold(&worker->mem_frame, out->cm + out->cm_count, pipeline, b1, b2, margin))
		{
			out->cm[out->cm_count].i1 = proxy_overlap[i].id1;
			out->cm[out->cm_count].i2 = proxy_overlap[i].id2;

			vec3 tmp;
			vec3_sub(tmp, b2->position, b1->position);
			if (vec3_dot(tmp, out->cm[out->cm_count].n) < 0)
			{
				vec3_mul_constant(out->cm[out->cm_count].n, -1.0f);
			}

			out->cm_count += 1;
		}	
	}

	arena_pop_packed(&worker->mem_frame, (range->count - out->cm_count) * sizeof(struct contact_manifold));

	KAS_END;
	return out;
}

static void internal_parallel_push_contacts(struct arena *mem_frame, struct physics_pipeline *pipeline)
{
	struct task_bundle *bundle = task_bundle_split_range(
			mem_frame, 
			&thread_push_contacts, 
			g_task_ctx->worker_count, 
			pipeline->c_state.proxy_overlap, 
			pipeline->c_state.overlap_count, 
			sizeof(struct dbvt_overlap), 
			pipeline);

	KAS_TASK(__func__, T_PHYSICS);
	pipeline->c_state.cm = (struct contact_manifold *) arena_push(mem_frame, pipeline->c_state.overlap_count * sizeof(struct contact_manifold));
	arena_push_record(mem_frame);

	pipeline->c_state.cm_count = 0;
	if (bundle)
	{	
		task_main_master_run_available_jobs();
		task_bundle_wait(bundle);

		for (u32 i = 0; i < bundle->task_count; ++i)
		{
			struct tpc_output *out = (struct tpc_output *) atomic_load_acq_64(&bundle->tasks[i].output);
			memcpy(pipeline->c_state.cm + pipeline->c_state.cm_count, out->cm, out->cm_count * sizeof(struct contact_manifold));
			pipeline->c_state.cm_count += out->cm_count;
		}
	
		task_bundle_release(bundle);
	}
	
	arena_pop_record(mem_frame);
	arena_pop_packed(mem_frame, (pipeline->c_state.overlap_count - pipeline->c_state.cm_count) * sizeof(struct contact_manifold));

	pipeline->c_db.contacts_frame_usage = bit_vec_alloc(mem_frame, pipeline->c_db.contacts_persistent_usage.bit_count, 0, 0);
	kas_assert(pipeline->c_db.contacts_frame_usage.block_count == pipeline->c_db.contacts_persistent_usage.block_count);
	kas_assert(pipeline->c_db.contacts_frame_usage.bit_count == pipeline->c_db.contacts_persistent_usage.bit_count);

	pipeline->c_db.new_list = (u32 *) mem_frame->stack_ptr;
	if (bundle)
	{
		//fprintf(stderr, "A: {");
		for (u32 i = 0; i < bundle->task_count; ++i)
		{
			struct tpc_output *out = (struct tpc_output *) atomic_load_acq_64(&bundle->tasks[i].output);
			for (u32 j = 0; j < out->cm_count; ++j)
			{
				const struct contact *c = c_db_add_contact(pipeline, out->cm + j, out->cm[j].i1, out->cm[j].i2);
				/* add to new links if needed */
				const u32 index = (u32) net_list_index(pipeline->c_db.contacts, c);
				if (index >= pipeline->c_db.contacts_persistent_usage.bit_count
					 || bit_vec_get_bit(&pipeline->c_db.contacts_persistent_usage, index) == 0)
				{
						pipeline->c_db.new_count += 1;
						arena_push_packed_memcpy(mem_frame, &index, sizeof(index));
				}
				//fprintf(stderr, " %u", index);
			}	
		}
		//fprintf(stderr, " } ");
	}
	KAS_END;
}

static void internal_merge_islands(struct arena *mem_frame, struct physics_pipeline *pipeline)
{
	KAS_TASK(__func__, T_PHYSICS);
	for (u32 i = 0; i < pipeline->c_db.new_count; ++i)
	{
		struct contact *c = net_list_address(pipeline->c_db.contacts, pipeline->c_db.new_list[i]);
		const u32 is1 = ((struct rigid_body *)pipeline->body_list->data)[c->cm.i1].island_index;
		const u32 is2 = ((struct rigid_body *)pipeline->body_list->data)[c->cm.i2].island_index;
		const u32 d1 = (is1 != ISLAND_STATIC) ? 0x2 : 0x0;
		const u32 d2 = (is2 != ISLAND_STATIC) ? 0x1 : 0x0;
		switch (d1 | d2)
		{
			/* dynamic-dynamic */
			case 0x3: 
			{
				is_db_merge_islands(pipeline, pipeline->c_db.new_list[i], c->cm.i1, c->cm.i2);
			} break;

			/* dynamic-static */
			case 0x2:
			{
				is_db_add_contact_to_island(&pipeline->is_db, is1, pipeline->c_db.new_list[i]);
			} break;

			/* static-dynamic */
			case 0x1:
			{
				is_db_add_contact_to_island(&pipeline->is_db, is2, pipeline->c_db.new_list[i]);
			} break;
		}
	}
	KAS_END;
}

static void internal_remove_contacts_and_tag_split_islands(struct arena *mem_frame, struct physics_pipeline *pipeline)
{
	KAS_TASK(__func__, T_PHYSICS);
	if (pipeline->c_db.contacts->count == 0) 
	{ 
		KAS_END;
		return; 
	}

	/* 
	 * For every removed contact
	 * (1)	if island is not tagged, tag island and push. 
	 * (2) 	remove contact
	 */

	/* Remove any contacts that were not persistent */
	//fprintf(stderr, " R: {");
	u32 bit = 0;
	is_db_reserve_splits_memory(mem_frame, &pipeline->is_db);
	for (u64 block = 0; block < pipeline->c_db.contacts_frame_usage.block_count; ++block)
	{
		u64 broken_link_block = 
				    pipeline->c_db.contacts_persistent_usage.bits[block]
				& (~pipeline->c_db.contacts_frame_usage.bits[block]);
		u32 b = 0;
		while (broken_link_block)
		{
			const u32 tzc = ctz64(broken_link_block);
			b += tzc;
			const u32 ci = bit + b;
			b += 1;

			broken_link_block = (tzc < 63) 
				? broken_link_block >> (tzc + 1)
				: 0;
		
			//fprintf(stderr, " %lu", ci);
			struct contact *c = net_list_address(pipeline->c_db.contacts, ci);

			/* tag island, if any exist, to split */
			const u32 b1 = CONTACT_KEY_TO_BODY_0(c->key);
			const u32 b2 = CONTACT_KEY_TO_BODY_1(c->key);
			if (((struct rigid_body *)pipeline->body_list->data)[b1].island_index != ISLAND_STATIC)
			{
				struct island *is = is_db_body_to_island(pipeline, b1);
				assert(is->contact_count > 0);
				is_db_tag_for_splitting(pipeline, b1);
			}
			else if (((struct rigid_body *)pipeline->body_list->data)[b2].island_index != ISLAND_STATIC)
			{
				struct island *is = is_db_body_to_island(pipeline, b2);
				assert(is->contact_count > 0);
				is_db_tag_for_splitting(pipeline, b2);
			}

			c_db_remove_contact(pipeline, c->key, (u32) ci);
		}
		bit += 64;
	}	
	is_db_release_unused_splits_memory(mem_frame, &pipeline->is_db);
	//fprintf(stderr, " }\tcontacts: %u\n", pipeline->c_db.contacts->count-2);
	KAS_END;
}

static void internal_split_islands(struct arena *mem_frame, struct physics_pipeline *pipeline)
{
	KAS_TASK(__func__, T_PHYSICS);
	/* TODO: Parallelize island splitting */

	for (u32 i = 0; i < pipeline->is_db.possible_splits_count; ++i)
	{
		is_db_split_island(mem_frame, pipeline, pipeline->is_db.possible_splits[i]);
	}

	c_db_update_persistent_contacts_usage(&pipeline->c_db);

	KAS_END;
}

static void internal_parallel_solve_islands(struct arena *mem_frame, struct physics_pipeline *pipeline, const f32 delta)
{
	KAS_TASK(__func__, T_PHYSICS);

	/* acquire any task resources */
	struct task_stream *stream = task_stream_init(mem_frame);
	struct island_solve_output *output = NULL;
	struct island_solve_output **next = &output;
	u32 base = 0;
	u32 bodies = 0;

	for (u64 block = 0; block < pipeline->is_db.island_usage.block_count; ++block)
	{
		u64 island_block = pipeline->is_db.island_usage.bits[block];
		u32 offset = 0;
		while (island_block)
		{
			const u32 tzc = ctz64(island_block);
			offset += tzc;
			const u32 is_index = base + offset;
			offset += 1;
			island_block = ((tzc + 1) < 64)
				? island_block >> (tzc + 1)
				: 0;

			struct island *is = array_list_address(pipeline->is_db.islands, is_index);
			
			if (!g_solver_config->sleep_enabled || ISLAND_AWAKE_BIT(is))
			{
				bodies += is->body_count;
				struct island_solve_input *args = arena_push(mem_frame, sizeof(struct island_solve_input));
				*next = arena_push(mem_frame, sizeof(struct island_solve_output));
				(*next)->island = is_index;
				(*next)->island_asleep = 0;
				(*next)->next = NULL;
				args->out = *next;
				args->is = is;
				args->pipeline = pipeline;
				args->timestep = delta;
				task_stream_dispatch(mem_frame, stream, thread_island_solve, args);

				next = &(*next)->next;
			}
		}
		base += 64;
	}

	task_main_master_run_available_jobs();

	/* spin wait until last job completes */
	task_stream_spin_wait(stream);
	/* release any task resources */
	task_stream_cleanup(stream);		


	/*
	 * TODO:
	 * 	(1) pipeline->event_list sequential list of physics events
	 * 	(2) implement array_list_flush to clear whole list 
	 */
	for (; output; output = output->next)
	{
		if (output->island_asleep)
		{
			PHYSICS_EVENT_ISLAND_ASLEEP(pipeline, output->island);
		}

		for (u32 i = 0; i < output->body_count; ++i)
		{
			struct physics_event *event = physics_pipeline_event_push(pipeline);
			event->type = PHYSICS_EVENT_BODY_ORIENTATION;
			event->body = output->bodies[i];
			event->ns = pipeline->frames_completed * pipeline->ns_tick;
		}
	}

	KAS_END;
}

void physics_pipeline_enable_sleeping(struct physics_pipeline *pipeline)
{
	assert(g_solver_config->sleep_enabled == 0);
	if (!g_solver_config->sleep_enabled)
	{
		g_solver_config->sleep_enabled = 1;
		const u32 body_flags = RB_ACTIVE | RB_DYNAMIC;
		//TODO only want to iterate over stuff once...
		for (u32 i = 0; i < pipeline->body_list->max_count; ++i)
		{
			struct rigid_body *body = physics_pipeline_rigid_body_lookup(pipeline, i);
			if (body->header.allocated && (body->flags & body_flags))
			{
				body->flags |= RB_AWAKE;
			}
		}

		for (u32 i = 0; i < pipeline->is_db.islands->length; ++i)
		{
			struct island *is = array_list_address(pipeline->is_db.islands, i);
			is->flags |= ISLAND_AWAKE | ISLAND_SLEEP_RESET;
			is->flags &= ~ISLAND_TRY_SLEEP;
		}
	}
}

void physics_pipeline_disable_sleeping(struct physics_pipeline *pipeline)
{
	assert(g_solver_config->sleep_enabled == 1);
	if (g_solver_config->sleep_enabled)
	{
		g_solver_config->sleep_enabled = 0;
		const u32 body_flags = RB_ACTIVE | RB_DYNAMIC;
		for (u32 i = 0; i < pipeline->body_list->max_count; ++i)
		{
			struct rigid_body *body = physics_pipeline_rigid_body_lookup(pipeline, i);
			if (body->flags & body_flags)
			{
				body->flags |= RB_AWAKE;
			}
		}

		for (u32 i = 0; i < pipeline->is_db.islands->length; ++i)
		{
			struct island *is = array_list_address(pipeline->is_db.islands, i);
			is->flags |= ISLAND_AWAKE;
			is->flags &= ~(ISLAND_SLEEP_RESET | ISLAND_TRY_SLEEP);
		}
	}
}

static void internal_update_contact_solver_config(struct physics_pipeline *pipeline)
{
	g_solver_config->warmup_solver = g_solver_config->pending_warmup_solver;
	g_solver_config->block_solver = g_solver_config->pending_block_solver;
	g_solver_config->iteration_count = g_solver_config->pending_iteration_count;
	g_solver_config->linear_slop = g_solver_config->pending_linear_slop;
	g_solver_config->baumgarte_constant = g_solver_config->pending_baumgarte_constant;
	g_solver_config->restitution_threshold = g_solver_config->pending_restitution_threshold;
	g_solver_config->linear_dampening = g_solver_config->pending_linear_dampening;
	g_solver_config->angular_dampening = g_solver_config->pending_angular_dampening;

	if (g_solver_config->pending_sleep_enabled != g_solver_config->sleep_enabled)
	{
		(g_solver_config->pending_sleep_enabled)
			? physics_pipeline_enable_sleeping(pipeline)
			: physics_pipeline_disable_sleeping(pipeline);

		g_solver_config->sleep_enabled = g_solver_config->pending_sleep_enabled;
	}

}

void internal_physics_pipeline_simulate_frame(struct physics_pipeline *pipeline, const f32 delta)
{
	KAS_TASK(__func__, T_PHYSICS);

	/* update, if possible, any pending values in contact solver config */
	internal_update_contact_solver_config(pipeline);

	/* broadphase => narrowphase => solve => integrate */
	internal_update_dynamic_tree(pipeline);
	internal_push_proxy_overlaps(&pipeline->frame, pipeline);
	internal_parallel_push_contacts(&pipeline->frame, pipeline);

	internal_merge_islands(&pipeline->frame, pipeline);
	internal_remove_contacts_and_tag_split_islands(&pipeline->frame, pipeline);
	internal_split_islands(&pipeline->frame, pipeline);
	internal_parallel_solve_islands(&pipeline->frame, pipeline, delta);

	PHYSICS_PIPELINE_VALIDATE(pipeline);
	
	KAS_END;
}

void physics_pipeline_clear_frame(struct physics_pipeline *pipeline)
{
	COLLISION_DEBUG_CLEAR();
	collision_state_clear_frame(&pipeline->c_state);
	is_db_clear_frame(&pipeline->is_db);
	c_db_clear_frame(&pipeline->c_db);
	arena_flush(&pipeline->frame);
}

f32 physics_pipeline_raycast_parameter(u32 *hit_handle, struct arena *mem_tmp, const struct physics_pipeline *pipeline, const struct ray *ray)
{
	arena_push_record(mem_tmp);

	const i32 *proxies_hit = (i32 *) mem_tmp->stack_ptr;
	const u32 proxy_count = dbvt_raycast(mem_tmp, &pipeline->dynamic_tree, ray);

	f32 t_best = F32_INFINITY;
	if (proxy_count == 0) 
	{ 
		goto physics_pipeline_raycast_parameter_cleanup;
	}

	f32 t;
	for (u32 i = 0; i < proxy_count; ++i)
	{
		struct rigid_body *body = physics_pipeline_rigid_body_lookup(pipeline, proxies_hit[i]);
		t = body_raycast_parameter(pipeline, body, ray);
		if (t < t_best)
		{
			t_best = t;
			*hit_handle = proxies_hit[i];
		}
	}

physics_pipeline_raycast_parameter_cleanup:
	arena_pop_record(mem_tmp);
	return t_best;
}

u32 physics_pipeline_raycast(u32 *hit_handle, struct arena *mem_tmp, const struct physics_pipeline *pipeline, const struct ray *ray)
{
	return (physics_pipeline_raycast_parameter(hit_handle, mem_tmp, pipeline, ray) != F32_INFINITY) ? 1 : 0;
}

struct physics_event *physics_pipeline_event_push(struct physics_pipeline *pipeline)
{
	if (pipeline->event_count == pipeline->event_len)
	{
		const u32 new_len = 2*pipeline->event_len;
		pipeline->event = realloc(pipeline->event, new_len * sizeof(struct physics_event));
		if (!pipeline->event)
		{
			log(T_SYSTEM, S_FATAL, "Failed to reallocate physics event array to new size[%u], aborting.", new_len);
			fatal_cleanup_and_exit(kas_thread_self_tid());
		}
		pipeline->event_len = new_len;
	}
	
	struct physics_event *event = pipeline->event + pipeline->event_count;
	pipeline->event_count += 1;
	return event;
}
