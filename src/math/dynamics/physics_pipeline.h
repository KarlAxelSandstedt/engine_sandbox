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

#ifndef __PHYSICS_PIPELINE_H__
#define __PHYSICS_PIPELINE_H__

#include "kas_common.h"
#include "allocator.h"
#include "dbvt.h"
#include "rigid_body.h"
#include "collision.h"
#include "contact_database.h"
#include "island.h"

#define UNITS_PER_METER		1.0f
#define UNITS_PER_DECIMETER	0.1f
#define UNITS_PER_CENTIMETER	0.01f
#define UNITS_PER_MILIMETER	0.001f

#define COLLISION_MARGIN_DEFAULT 5.0f * UNITS_PER_MILIMETER 

#define UNIFORM_SIZE 256
#define GRAVITY_CONSTANT_DEFAULT 9.80665f

#define PHYSICS_EVENT_BODY(pipeline, event_type, body_index)						\
	{												\
		struct physics_event *__physics_debug_event = physics_pipeline_event_push(pipeline);	\
		__physics_debug_event->type = event_type;						\
		__physics_debug_event->body = body_index;						\
	}

#define PHYSICS_EVENT_ISLAND(pipeline, event_type, island_index)					\
	{												\
		struct physics_event *__physics_debug_event = physics_pipeline_event_push(pipeline);	\
		__physics_debug_event->type = event_type;						\
		__physics_debug_event->island = island_index;						\
	}

#ifdef KAS_PHYSICS_DEBUG

#define	PHYSICS_EVENT_BODY_NEW(pipeline, body)			PHYSICS_EVENT_BODY(pipeline, PHYSICS_EVENT_BODY_NEW, body)
#define	PHYSICS_EVENT_BODY_REMOVED(pipeline, body)		PHYSICS_EVENT_BODY(pipeline, PHYSICS_EVENT_BODY_REMOVED, body)
#define	PHYSICS_EVENT_ISLAND_ASLEEP(pipeline, island)		PHYSICS_EVENT_ISLAND(pipeline, PHYSICS_EVENT_ISLAND_ASLEEP, island)
#define	PHYSICS_EVENT_ISLAND_AWAKE(pipeline, island)		PHYSICS_EVENT_ISLAND(pipeline, PHYSICS_EVENT_ISLAND_AWAKE, island)
#define	PHYSICS_EVENT_ISLAND_NEW(pipeline, island)		PHYSICS_EVENT_ISLAND(pipeline, PHYSICS_EVENT_ISLAND_NEW, island)
#define	PHYSICS_EVENT_ISLAND_MERGED_INTO(pipeline, island)	PHYSICS_EVENT_ISLAND(pipeline, PHYSICS_EVENT_ISLAND_MERGED_INTO, island)
#define	PHYSICS_EVENT_ISLAND_REMOVED(pipeline, island)		PHYSICS_EVENT_ISLAND(pipeline, PHYSICS_EVENT_ISLAND_REMOVED, island)
#define PHYSICS_EVENT_CONTACT_NEW(pipeline, contact_index)						\
	{												\
		struct physics_event *__physics_debug_event = physics_pipeline_event_push(pipeline);	\
		__physics_debug_event->type = PHYSICS_EVENT_CONTACT_NEW;				\
		__physics_debug_event->contact = contact_index;						\
	}
#define PHYSICS_EVENT_CONTACT_REMOVED(pipeline, body1_index, body2_index)				\
	{												\
		struct physics_event *__physics_debug_event = physics_pipeline_event_push(pipeline);	\
		__physics_debug_event->type = PHYSICS_EVENT_CONTACT_REMOVED;				\
		__physics_debug_event->contact_bodies.body1 = body1_index;				\
		__physics_debug_event->contact_bodies.body2 = body2_index;				\
	}

#else

#define	PHYSICS_EVENT_BODY_NEW(pipeline, body)
#define	PHYSICS_EVENT_BODY_REMOVED(pipeline, body)
#define	PHYSICS_EVENT_ISLAND_ASLEEP(pipeline, island)
#define	PHYSICS_EVENT_ISLAND_AWAKE(pipeline, island) 
#define	PHYSICS_EVENT_ISLAND_NEW(pipeline, island)   
#define	PHYSICS_EVENT_ISLAND_MERGED_INTO(pipeline, island)   
#define	PHYSICS_EVENT_ISLAND_REMOVED(pipeline, island)
#define PHYSICS_EVENT_CONTACT_NEW(pipeline, contact)
#define PHYSICS_EVENT_CONTACT_REMOVED(pipeline, body1, body2)

#endif

enum physics_event_type
{
	PHYSICS_EVENT_CONTACT_NEW,
	PHYSICS_EVENT_CONTACT_REMOVED,
	PHYSICS_EVENT_ISLAND_NEW,
	PHYSICS_EVENT_ISLAND_MERGED_INTO,
	PHYSICS_EVENT_ISLAND_REMOVED,
	PHYSICS_EVENT_ISLAND_AWAKE,
	PHYSICS_EVENT_ISLAND_ASLEEP,
	PHYSICS_EVENT_BODY_NEW,
	PHYSICS_EVENT_BODY_REMOVED,
	PHYSICS_EVENT_BODY_ORIENTATION,
	PHYSICS_EVENT_COUNT
};

struct physics_event
{
	u64			ns;	/* time of event */
	enum physics_event_type type;
	union
	{
		u32 contact;
		u32 island;
		u32 body;
		struct 
		{
			u32 body1;
			u32 body2;
		} contact_bodies;
	};
};

struct physics_debug
{
	vec4	island_static_color;
	vec4	island_sleeping_color;
	vec4	island_awake_color;
};

/*
 * Physics Pipeline
 */
struct physics_pipeline 
{
	struct arena 	frame;			/* frame memory */

	//TODO physics_pipeline_start -> set ns_start, we want timings reported in a unified clock setting
	u64		ns_start;		/* external ns at start of physics pipeline */
	u64		ns_elapsed;		/* actual ns elasped in pipeline (= 0 at start) */
	u64		ns_tick;		/* ns per game tick */
	u64 		frames_completed;	/* number of completed physics frames */ 

	struct string_database *shape_db;	/* externally owned */
	struct string_database *prefab_db;	/* externally owned */


	struct array_list_intrusive *body_list;

	struct dbvt dynamic_tree;
	struct collision_state c_state;

	struct contact_database c_db;
	struct island_database is_db;

	/* debug information */
	struct physics_debug	debug;

	/* TODO: simplify, use dlls, .... physics events */
	u32 			event_count;
	u32			event_len;
	struct physics_event *	event;

	//TODO temporary, move somewhere else.
	vec3 gravity;	/* gravity constant */
};

/**************** PHYISCS PIPELINE API ****************/

/* Initialize a new growable physics pipeline; ns_tick is the duration of a physics frame. */
struct physics_pipeline	physics_pipeline_alloc(struct arena *mem, const u32 initial_size, const u64 ns_tick, const u64 frame_memory, struct string_database *shape_db, struct string_database *prefab_db);
/* free pipeline resources */
void 			physics_pipeline_free(struct physics_pipeline *physics_pipeline);
/* flush pipeline resources */
void			physics_pipeline_flush(struct physics_pipeline *physics_pipeline);
/* pipeline main method: simulate a single physics frame and update internal state  */
void 			physics_pipeline_tick(struct physics_pipeline *pipeline);
/* allocate new rigid body in pipeline and return its slot */
struct slot		physics_pipeline_rigid_body_alloc(struct physics_pipeline *pipeline, struct rigid_body_prefab *prefab, const vec3 position, const quat rotation, const u32 entity);
/* lookup a collision shape given its handle; return NULL if body isn't found */
struct rigid_body *	physics_pipeline_rigid_body_lookup(const struct physics_pipeline *pipeline, const u32 handle);
/* deallocate a collision shape associated with the given handle. If no shape is found, do nothing */
void			physics_pipeline_rigid_body_dealloc(struct physics_pipeline *pipeline, const u32 handle);
/* validate and assert internal state of physics pipeline */
void			physics_pipeline_validate(const struct physics_pipeline *pipeline);
/* return 0 if no hit, 1 if hit. If 1, set hit_index to the body's pipeline handle. */
u32 			physics_pipeline_raycast(struct arena *mem_tmp, struct slot *slot, const struct physics_pipeline *pipeline, const struct ray *ray);
/* return, IF hit, parameter t of ray at first collision. Otherwise return F32_INFINITY */
f32 			physics_pipeline_raycast_parameter(struct arena *mem_tmp, struct slot *slot, const struct physics_pipeline *pipeline, const struct ray *ray);

#ifdef KAS_DEBUG
#define PHYSICS_PIPELINE_VALIDATE(pipeline)	physics_pipeline_validate(pipeline)
#else
#define PHYSICS_PIPELINE_VALIDATE(pipeline)	
#endif

/**************** PHYISCS PIPELINE INTERNAL API ****************/

/* run a physics frame with given timestep  and update pipeline state */
void 			internal_physics_pipeline_simulate_frame(struct physics_pipeline *pipeline, const f32 delta);
/* push physics event into pipeline memory and return pointer to allocated event */
struct physics_event *	physics_pipeline_event_push(struct physics_pipeline *pipeline);

/**************** TODO MOVE ****************/

//void 	physics_pipeline_push_convex_hulls(const struct physics_pipeline *pipeline, struct drawbuffer *buf, const vec4 color, struct arena *mem_1, struct arena *mem_2, struct arena *mem_3, struct arena *mem_4, struct arena *mem_5, i32 i_count[], i32 i_offset[]);
void	physics_pipeline_push_positions(void *buf, const struct physics_pipeline *pipeline);
void physics_pipeline_push_closest_points_between_bodies(struct physics_pipeline *pipeline);
void physics_pipeline_simulate(struct physics_pipeline *pipeline, const f32 delta);

void 	physics_pipeline_clear_frame(struct physics_pipeline *pipeline);

void physics_pipeline_enable_sleeping(struct physics_pipeline *pipeline);
void physics_pipeline_disable_sleeping(struct physics_pipeline *pipeline);

//void	physics_pipeline_push_dbvt(struct drawbuffer *buf, struct physics_pipeline *pipeline, const vec4 color);
//void	physics_pipeline_push_proxies(struct drawbuffer *buf, const struct physics_pipeline *pipeline, const vec4 color);

#endif
