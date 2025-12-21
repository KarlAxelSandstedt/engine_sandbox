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

#ifndef __COLLISION_H__
#define __COLLISION_H__

#include "kas_common.h"
#include "allocator.h"
#include "kas_math.h"
#include "geometry.h"
#include "sys_public.h"
#include "string_database.h"
#include "float32.h"
#include "queue.h"

#define COLLISION_DEFAULT_MARGIN	(100.0f * F32_EPSILON)
#define COLLISION_POINT_DIST_SQ		(10000.0f * F32_EPSILON)

/********************************** COLLISION DEBUG **********************************/

typedef struct visual_segment
{
	struct segment	segment;
	vec4		color;
} visual_segment;
DECLARE_STACK(visual_segment);

struct visual_segment	visual_segment_construct(const struct segment segment, const vec4 color);

struct collision_debug
{
	stack_visual_segment	stack_segment;

	u8			pad[64];
};

extern kas_thread_local struct collision_debug *tl_debug;

#ifdef KAS_PHYSICS_DEBUG

#define COLLISION_DEBUG_ADD_SEGMENT(segment, color)							\
	stack_visual_segment_push(&tl_debug->stack_segment,  visual_segment_construct(segment, color))

#else

#define COLLISION_DEBUG_ADD_SEGMENT(segment, color)

#endif

/********************************** COLLISION SHAPES **********************************/

enum collision_shape_type
{
	COLLISION_SHAPE_SPHERE,
	COLLISION_SHAPE_CAPSULE,
	COLLISION_SHAPE_CONVEX_HULL,
	COLLISION_SHAPE_TRI_MESH,	/* Should be used static environment */
	COLLISION_SHAPE_COUNT,
};

struct collision_shape
{
	STRING_DATABASE_SLOT_STATE;
	u32 			center_of_mass_localized; /* Has the shape translated its vertices into COM space? */
	enum	collision_shape_type type;
	union
	{
		struct sphere 	sphere;
		struct capsule 	capsule;
		struct dcel	hull;
		struct tri_mesh tri_mesh;
	};
};

enum collision_result_type
{
	COLLISION_NONE,		/* No collision, no sat cache stored */
	COLLISION_SAT_CACHE,	/* No collision, sat cache stored    */
	COLLISION_CONTACT,	/* Collision, sat cache stored       */
	COLLISION_COUNT
};

struct contact_manifold
{
	vec3 	v[4];
	f32 	depth[4];
	vec3 	n;		/* B1 -> B2 */
	u32 	v_count;
	u32 	i1;
	u32 	i2;
};

enum sat_cache_type
{
	SAT_CACHE_SEPARATION,
	SAT_CACHE_CONTACT_FV,
	SAT_CACHE_CONTACT_EE,
	SAT_CACHE_COUNT,
};

struct sat_cache
{
	POOL_SLOT_STATE;
	u32	touched;
	DLL_SLOT_STATE;

	enum sat_cache_type	type;
	union
	{
		struct
		{
			u32	body;	/* body (0,1) containing face */
			u32	face;	/* reference face 	      */
		};

		struct
		{
			u32	edge1;	/* body0 edge, body0 < body1 */
			u32	edge2;	/* body1 edge                */
		};

		struct
		{
			vec3	separation_axis;
			f32	separation;
		};
	};

	u64	key;
};

struct collision_result
{
	enum collision_result_type	type;
	struct sat_cache		sat_cache;
	struct contact_manifold 	manifold;
};

void 	contact_manifold_debug_print(FILE *file, const struct contact_manifold *cm);

/********************************** RIGID BODY METHODS  **********************************/

struct physics_pipeline;
struct rigid_body;

/* test for intersection between bodies, with each body having the given margin. returns 1 if intersection. */
u32	body_body_test(const struct physics_pipeline *pipeline, const struct rigid_body *b1, const struct rigid_body *b2, const f32 margin);
/* return closest points c1 and c2 on bodies b1 and b2 (with no margin), respectively, given no intersection */
f32 	body_body_distance(vec3 c1, vec3 c2, const struct physics_pipeline *pipeline, const struct rigid_body *b1, const struct rigid_body *b2, const f32 margin);
/* returns contact manifold or sat cache pointing from b1 to b2, given that the bodies are colliding  */
u32 	body_body_contact_manifold(struct arena *tmp, struct collision_result *result, const struct physics_pipeline *pipeline, const struct rigid_body *b1, const struct rigid_body *b2, const f32 margin);
/* Return t such that ray.origin + t*ray.dir == closest point on rigid body */
f32 	body_raycast_parameter(const struct physics_pipeline *pipeline, const struct rigid_body *b, const struct ray *ray);
/* Return 1 if ray hit body, 0 otherwise. If hit, we return the closest intersection point */
u32 	body_raycast(vec3 intersection, const struct physics_pipeline *pipeline, const struct rigid_body *b, const struct ray *ray);

/*
=================================================================================================================
|					Dynamic Bounding Volume Hierachy					|
=================================================================================================================
*/

/**
 * Basic steps for a simple dynamic bounding volume hierarchy:
 *
 * INCREMENTAL ADD
 * 	(1) Alloc leaf node
 * 	(2) Find the best sibling for the new volume
 * 	(3) Add parent to sibling and new node
 * 	(4) Rebase the tree, in order to balance it at keep the performance good
 *
 * INCREMENTAL CHECK OVERLAP
 * 	- several possibilities, we should as a first step try descendLargestVolume.
 * 	  If one goes through a whole tree at a time (depth first) one may get into really bad
 * 	  situations as comparing super small object vs relatively large ones, and check overlaps
 * 	  all the time.
 *
 * INCREMENTAL REMOVE
 * 	(1) Remove leaf
 * 	(2) Set sibling as parent leaf
 *
 * Some useful stuff for debugging purposes and performance monitoring would be:
 * 	- draw line box around AABB_2d volumes
 * 	- average number of overlaps every frame
 * 	- number of nodes
 * 	- deepest leaves
 *
 * Some general optimisations
 * 	- enlarged AABB_2ds somehow (less reinserts)
 * 	- do not recompute cost of child in balance if...
 * 	- clever strategy for how to place parant vs child nodes (left always comes after parent...)
 * 	  We want to make the cache more coherent in traversing the tree
 * 	- remove min_queue queue_indices, aren't they unnecessary?
 * 	- Another strategy for cache coherency while traversing would be double layer nodes (parent, child, child) 
 * 	- Global caching of collisions (Hash table of previous primitive collisions)
 * 	- Local caching (Every single object has a cache of collisions)
 * 	- SIMD AABB_2d operations
 */

#define DBVT_NO_NODE			U32_MAX 
#define COST_QUEUE_INITIAL_COUNT 	4096 

struct dbvt_node 
{
	POOL_SLOT_STATE;
	struct AABB	box;
	u32 		id;
	u32 		parent;
	u32 		left;
	u32 		right;
};

struct dbvt
{
	struct pool		node_pool;
	struct min_queue	cost_queue;

	u32 			proxy_count; 
	u32 			root;
};

struct dbvt_overlap
{
	u32 id1;
	u32 id2;	
};

/* If mem == NULL, standard malloc is used */
struct dbvt 		dbvt_alloc(const u32 len);
/* free allocated resources */
void 			dbvt_free(struct dbvt *tree);
/* flush / reset the hierarchy  */
void 			dbvt_flush(struct dbvt *tree);
/* id is an integer identifier from the outside, return index of added value */
u32 			dbvt_insert(struct dbvt *tree, const u32 id, const struct AABB *box);
/* remove leaf corresponding to index from tree */
void 			dbvt_remove(struct dbvt *tree, const u32 index);
/* Return overlapping ids ptr, set to NULL if no overlap. if overlap, count is set */
struct dbvt_overlap *	dbvt_push_overlap_pairs(struct arena *mem, u32 *count, struct dbvt *tree);
/* push	id:s of leaves hit by raycast. returns number of hits. -1 == out of memory */
u32			dbvt_raycast(struct arena *mem, const struct dbvt *tree, const struct ray *ray);
/* validate tree construction */
void			dbvt_validate(struct dbvt *tree);

#endif
