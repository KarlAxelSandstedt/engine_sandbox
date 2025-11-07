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

#ifndef __RIGID_BODY_H__
#define __RIGID_BODY_H__

#include "kas_common.h"
#include "kas_math.h"
#include "allocator.h"
#include "collision.h"
#include "geometry.h"
#include "array_list.h"

#define RB_ACTIVE		(0x1u << 0u)
#define RB_DYNAMIC		(0x1u << 1u)
#define RB_AWAKE		(0x1u << 2u)
#define RB_ISLAND		(0x1u << 3u)

#define RB_IS_ACTIVE(b)		((b->flags & RB_ACTIVE) >> 0u)
#define RB_IS_DYNAMIC(b)	((b->flags & RB_DYNAMIC) >> 1u)
#define RB_IS_AWAKE(b)		((b->flags & RB_AWAKE) >> 2u)
#define RB_IS_ISLAND(b)		((b->flags & RB_ISLAND) >> 3u)

#define IS_ACTIVE(flags)	((flags & RB_ACTIVE) >> 0u)
#define IS_DYNAMIC(flags)	((flags & RB_DYNAMIC) >> 1u)
#define IS_AWAKE(flags)		((flags & RB_AWAKE) >> 2u)
#define IS_ISLAND(flags)	((flags & RB_ISLAND) >> 3u)

struct rigid_body
{
	struct array_list_intrusive_node header;	/* node header, MUST BE AT THE TOP OF STRUCT! */
	struct AABB local_box;	/* bounding AABB */

	u32 entity;
	i32 proxy;
	f32 margin;
	u32 flags;

	enum collision_shape_type	shape_type;
	u32				shape_handle;

	mat3 inertia_tensor;		/* intertia tensor of body frame */
	mat3 inv_inertia_tensor;
	f32 mass;			/* total body mass */
	f32 restitution;
	f32 friction;			/* Range [0.0, 1.0f] : bound tangent impulses to mix(b1->friction, b2->friction)*(normal impuse) */
	f32 low_velocity_time;		/* Current uninterrupted time body has been in a low velocity state */

	/* dynamic state */
	quat rotation;		
	vec3 velocity;
	vec3 angular_velocity;

	quat angular_momentum;	/* TODO: */
	vec3 position;	/* center of mass world frame position */
	vec3 linear_momentum;   /* L = mv */

	u32	first_contact_index;
	u32	island_index;

#ifdef	KAS_PHYSICS_DEBUG
	vec4 	color;
#endif
};

void rigid_body_update_local_box(struct rigid_body *body, const struct collision_shape *shape);
void rigid_body_proxy(struct AABB *proxy, struct rigid_body *body);

void statics_print(FILE *file, struct rigid_body *body);
void statics_setup(struct rigid_body *body, const struct collision_shape *shape,  const f32 density);

/*
rigid_body_prefab
=================
rigid body prefabs: used within editor and level editor file format.
*/
struct rigid_body_prefab
{
	STRING_DATABASE_SLOT_STATE;
	u32		shape;

	struct AABB 	bounding_box;		/* bounding AABB */
	mat3 		inertia_tensor;		/* intertia tensor of body frame */
	mat3 		inv_inertia_tensor;
	vec3		center_of_mass;
	f32 		mass;			/* total body mass */
	f32		density;
	f32 		restitution;
	f32 		friction;		/* Range [0.0, 1.0f] : bound tangent impulses to mix(b1->friction, b2->friction)*(normal impuse) */
	u32		dynamic;		/* dynamic body is true, static if false */
};

void prefab_statics_setup(struct rigid_body_prefab *prefab, const struct collision_shape *shape, const f32 density);

#endif
