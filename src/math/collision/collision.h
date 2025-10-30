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

/********************************** COLLISION GLOBALS AND FRAME STATE **********************************/

/* If two points are within this length, say the end-points of a normal, view the difference as a zero vector  */
#define COLLISION_POINT_DIST_SQ		100.0f * FLT_EPSILON

struct collision_state
{
	/* persistent state */
	u32 margin_on;
	f32 margin;	/* margin for collision shapes (may be negative or positive) */

	/* frame state */
	struct dbvt_overlap *proxy_overlap;	/* proxy overlaps */
	struct contact_manifold *cm;		/* contact manifolds from collisions  */

	u32 overlap_count;
	u32 cm_count;
};

void collision_state_clear_frame(struct collision_state *collision_state);

/********************************** COLLISION DEBUGGING **********************************/

struct plane_visual
{
	vec4 color;
	struct plane plane;
	vec3 center;
	f32 hw;
};

struct collision_debug
{
	vec4 segment_color;
	vec4 bounding_box_color;
	vec4 dynamic_tree_color;
	vec4 collision_color;
	vec4 contact_manifold_color;

	vec4 island_static_color;
	vec4 island_sleeping_color;
	vec4 island_awake_color;

	vec3 (*segment)[2];
	struct plane_visual *plane_visuals;

	u32 segment_max_count;
	u32 segment_count;
	u32 plane_max_count;
	u32 plane_count;

	u32 draw_island;
	u32 draw_sleeping;
	u32 draw_dynamic_tree;
	u32 draw_bounding_box;
	u32 draw_segment;
	u32 draw_collision;		
	u32 draw_contact_manifold;
	u32 draw_plane;

	u32 pending_draw_island;
	u32 pending_draw_sleeping;
	u32 pending_draw_dynamic_tree;
	u32 pending_draw_bounding_box;
	u32 pending_draw_segment;
	u32 pending_draw_collision;		
	u32 pending_draw_contact_manifold;
	u32 pending_draw_plane;
};

extern struct collision_debug *g_collision_debug;

void collision_debug_init(struct arena *mem, const u32 max_bodies, const u32 max_primitives);
void collision_debug_clear(void);
#ifdef KAS_PHYSICS_DEBUG 
void collision_debug_add_segment(struct segment s);
void *collision_debug_add_segment_callback(void *segment_addr);
void collision_debug_add_contact_manifold(struct contact_manifold *cm);
void collision_debug_add_AABB_outline(const struct AABB aabb);
void collision_debug_add_plane(const struct plane p, const vec3 center, const f32 hw, const vec4 color);

#define	COLLISION_DEBUG_MAX_SEGMENTS 	10000
#define COLLISION_DEBUG_INIT(mem, max_bodies, max_segments) 	collision_debug_init(mem, max_bodies, max_segments)
#define COLLISION_DEBUG_CLEAR() 				collision_debug_clear()
#define COLLISION_DEBUG_ADD_SEGMENT(s) 				collision_debug_add_segment(s)

#define COLLISION_DEBUG_ADD_SEGMENT_TIMED(callbacks, s, current_time, lifetime)	\
{										\
	struct segment __tmp_s__ = (s);						\
	callback_add((callbacks),						\
			&collision_debug_add_segment_callback,			\
			&__tmp_s__,						\
			sizeof(struct segment),					\
			CALLBACK_FRAME,						\
			(current_time),						\
			(lifetime));						\
}

#define COLLISION_DEBUG_ADD_PLANE(pl, center, w, r, g, b, a)			\
{										\
	const vec4 __plane_debug_color__ = { r, g, b, a };			\
	collision_debug_add_plane(pl, center, w, __plane_debug_color__);	\
}

#define COLLISION_DEBUG_ADD_AABB_OUTLINE(aabb)			collision_debug_add_AABB_outline(aabb)
#else
#define COLLISION_DEBUG_INIT(mem, max_bodies, max_segments) 	collision_debug_init(mem, max_bodies, 0)
#define COLLISION_DEBUG_CLEAR()
#define COLLISION_DEBUG_ADD_SEGMENT(s)
#define COLLISION_DEBUG_ADD_SEGMENT_TIMED(s, t) 		
#define COLLISION_DEBUG_ADD_AABB_OUTLINE(aabb)
#define COLLISION_DEBUG_ADD_PLANE(pl, center, w, r, g, b, a)
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

struct collision_sphere
{
	f32 radius;
};

struct collision_capsule
{
	vec3 p1; /* p2 = -p1 */
	f32 radius;
};

struct hull_face
{
	u32 first;	/* first half edge */
	u32 count;	/* edge count */
};

struct hull_half_edge
{
	u32 origin;	/* vertex index origin */
	u32 twin; 	/* twin half edge */
	u32 face_ccw; 	/* face to the left of half edge */
};

struct collision_hull
{
	struct hull_face *f;		/* f[i] = half-edge of face i */
	struct hull_half_edge *e;
	vec3ptr	v;
	u32 f_count;
	u32 e_count;
	u32 v_count;
};

struct collision_shape
{
	STRING_DATABASE_SLOT_STATE;
	enum	collision_shape_type type;
	union
	{
		struct collision_sphere 	sphere;
		struct collision_capsule 	capsule;
		struct collision_hull		hull;
	};
};

struct collision_hull collision_box(struct arena *mem, const vec3 hw);

void collision_hull_face_direction(vec3 dir, const struct collision_hull *h, const u32 fi); /* not normalized */
void collision_hull_face_normal(vec3 normal, const struct collision_hull *h, const u32 fi); /* normalized */
struct plane collision_hull_face_plane(const struct collision_hull *h, mat3 rot, const vec3 pos, const u32 fi);
struct plane collision_hull_face_clip_plane(const struct collision_hull *h, mat3 rot, const vec3 pos, const vec3 face_normal, const u32 e0, const u32 e1); /* Return clip plane of face containing edge e0e1, orthogonal to the face normal */
struct segment collision_hull_face_clip_segment(const struct collision_hull *h, mat3 rot, const vec3 pos, const u32 fi, const struct segment *s); /* clip segment against face fi's edge-planes (No projection onto face plane!) */
u32 collision_hull_face_projected_point_test(const struct collision_hull *h, mat3 rot, const vec3 pos, const u32 fi, const vec3 p); /* Project p onto face plane and test if it is on the face */

void collision_hull_half_edge_normal(vec3 dir, const struct collision_hull *h, const u32 ei);
void collision_hull_half_edge_direction(vec3 dir, const struct collision_hull *h, const u32 ei);
struct segment collision_hull_half_edge_segment(const struct collision_hull *h, mat3 rot, const vec3 pos, const u32 ei);

#ifdef KAS_DEBUG
void collision_hull_assert(struct collision_hull *hull);
#define COLLISION_HULL_ASSERT(hull)	collision_hull_assert(hull)
#else
#define COLLISION_HULL_ASSERT(hull)	
#endif

void sphere_world_support(vec3 support, const vec3 dir, const struct collision_sphere *sph, const vec3 pos);
void capsule_world_support(vec3 support, const vec3 dir, const struct collision_capsule *cap, mat3 rot, const vec3 pos);
u64 collision_hull_world_support(vec3 support, const vec3 dir, const struct collision_hull *hull, mat3 rot, const vec3 pos);

struct contact_manifold
{
	vec3 v[4];
	f32 depth[4];
	vec3 n;		/* B1 -> B2 */
	u32 v_count;
	u32 i1;
	u32 i2;
};

void contact_manifold_debug_print(FILE *file, const struct contact_manifold *cm);

/********************************** RIGID BODY METHODS  **********************************/

struct physics_pipeline;
struct rigid_body;

/* test for intersection between bodies, with each body having the given margin */
u32 body_body_test(const struct physics_pipeline *pipeline, const struct rigid_body *b1, const struct rigid_body *b2, const f32 margin);
/* return closest points c1 and c2 on bodies b1 and b2 (with no margin), respectively, given no intersection */
f32 body_body_distance(vec3 c1, vec3 c2, const struct physics_pipeline *pipeline, const struct rigid_body *b1, const struct rigid_body *b2, const f32 margin);
/* returns contact manifold pointing from b1 to b2, given that the bodies are colliding  */
u32 body_body_contact_manifold(struct arena *tmp, struct contact_manifold *cm, const struct physics_pipeline *pipeline, const struct rigid_body *b1, const struct rigid_body *b2, const f32 margin);
/* Return t such that ray.origin + t*ray.dir == closest point on rigid body */
f32 body_raycast_parameter(const struct physics_pipeline *pipeline, const struct rigid_body *b, const struct ray *ray);
/* Return 1 if ray hit body, 0 otherwise. If hit, we return the closest intersection point */
u32 body_raycast(vec3 intersection, const struct physics_pipeline *pipeline, const struct rigid_body *b, const struct ray *ray);

#endif
