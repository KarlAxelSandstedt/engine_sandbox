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

#ifndef __KAS_CONTACT_SOLVER_H__
#define __KAS_CONTACT_SOLVER_H__

#include "kas_common.h"
#include "allocator.h"
#include "kas_math.h"
#include "island.h"
#include "physics_pipeline.h"

/*
 * Implementation of ([Iterative Dynamics with Temporal Coherence], Erin Catto, 2005) and
 * Box2D features.
 *
 * Planned Features:
 * (O) Block Solver
 * (O) Sleeping islands
 * (O) Friction Solver
 * () warmup impulse for contact points
 * (O) g_solver_config dampening constants (linear and angular)
 * (O) velocity biases: baumgarte bias linear slop (allowed position error we correct for, see def. in https://allenchou.net/2013/12/game-physics-resolution-contact-constraints/)
 * (O) Resitution base contacts [bounciness of objects, added to velocity bias in velocity constraint solver given
 * 	restitution threshold, see def. in https://allenchou.net/2013/12/game-physics-resolution-contact-constraints/
 * 	and box2D
 * () threshold for forces
 * (O) conditioning number of normal mass, must ensure stability.
 */

struct contact_solver_config
{
	u32 iteration_count;	/* velocity solver iteration count */
	u32 block_solver;	/* bool : Use block solver when applicable */
	u32 warmup_solver;	/* bool : Should warmup solver when applicable */
	vec3 gravity;
	f32 baumgarte_constant  /* Range[0.0, 1.0] : Determine how quickly contacts are resolved, 1.0f max speed */;
	f32 max_condition;	/* max condition number of block normal mass */
	f32 linear_dampening;	/* Range[0.0, inf] : coefficient in diff. eq. dv/dt = -coeff*v */
	f32 angular_dampening;	/* Range[0.0, inf] : coefficient in diff. eq. dv/dt = -coeff*v */
	f32 linear_slop;	/* Range[0.0, inf] : Allowed penetration before velocity steering gradually sets in. */
	f32 restitution_threshold; /* Range[0.0, inf] : If -seperating_velocity >= threshold, we apply the restitution effect */

	u32 sleep_enabled;	/* bool : enable sleeping of bodies  */
	f32 sleep_time_threshold; /* Range(0.0, inf] :  Time threshold for which a body must have low velocity before being able to fall asleep */
	f32 sleep_linear_velocity_sq_limit; /* Range (0.0f, inf] : maximum linear velocity squared that a body falling asleep may have */
	f32 sleep_angular_velocity_sq_limit; /* Range (0.0f, inf] : maximum angular velocity squared that a body falling asleep may have */

	/* Pending updates */
	u32 pending_block_solver;		
	u32 pending_warmup_solver;		
	u32 pending_sleep_enabled;		
	u32 pending_iteration_count;
	f32 pending_baumgarte_constant;
	f32 pending_linear_slop;
	f32 pending_restitution_threshold;
	f32 pending_linear_dampening;
	f32 pending_angular_dampening;
};

extern struct contact_solver_config *g_solver_config;

void	contact_solver_config_init(const u32 iteration_count, const u32 block_solver, const u32 warmup_solver, const vec3 gravity, const f32 baumgarte_constant, const f32 max_condition, const f32 linear_dampening, const f32 angular_dampening, const f32 linear_slop, const f32 restitution_threshold, const u32 sleep_enabled, const f32 sleep_time_threshold, const f32 sleep_linear_velocity_sq_limit, const f32 sleep_angular_velocity_sq_limit);


/*
 * Memory layout: Three distictions
 *
 * struct velocity_constraint_point 	- constraint point local data (body center to manifold point, and so on)
 * struct velocity_constraint 		- contact local data (manifold normal, body indices, and so on)
 * solver->array			- shared data between contacts, i.e temporary body changes (velocities, ...)
 */

/*
 * velocity_constraint_point - individual constraint for one point in the contact manifold
 */
struct velocity_constraint_point
{
	vec3 	r1;		/* vector from body 1's center to manifold point */
	vec3 	r2;		/* vector from body 2's center to manifold point */
	f32 	normal_impulse;	/* the normal impulse produced by the contact */
	f32	velocity_bias;	/* scale of velocity_bias along contact normal */
	f32	normal_mass;	/* 1.0f / row(J,i)*Inv(M)*J^T entry for point */
	f32	tangent_mass[2]; /* 1.0f / row(J_tangent,i)*Inv(M)*J_tangent^T entry for point */
	f32	tangent_impulse[2]; /* the tangent impulses produced by the contact */
};

struct velocity_constraint
{
	struct velocity_constraint_point *vcps;
	void * 	normal_mass;	/* mat2, mat3 or mat4 normal mass for block solver = Inv(J*Inv(M)*J^T) */
	void * 	inv_normal_mass;/* mat2, mat3 or mat4 inv normal mass for block solver = J*Inv(M)*J^T */

	/* contact base axes */
	vec3 	normal;		/* Currently shared contact manifold normal between all point constraints */
	vec3	tangent[2];	/* normalized friction directions of contact */

	u32 	lb1;		/* local body 1 index (index into solver arrays) */
	u32 	lb2;		/* local body 2 index (index into solver arrays) */
	u32 	vcp_count;	/* Number of contact points in the contact manifold */
	f32	restitution;	/* Range[0.0f, 1.0f] : higher => bouncy */
	//f32	tangent_impulse_bound;	/* TODO: contact_friction * gravity_constant * point_mass */
	f32	friction;	/* TODO: friction = f32_max(b1->friction, b2->friction) */
	u32	block_solve;	/* if config->block_solver && condition number of block normal mass is ok, then = 1 */
};

struct contact_solver
{
	f32 			timestep;
	u32			body_count;
	u32			contact_count;

	struct rigid_body **		bodies;
	mat3ptr				Iw_inv;		/* inverted world inertia tensors */
	struct velocity_constraint *	vcs;	

	/* temporary state of bodies in island, static bodies index last element */
	vec3ptr			linear_velocity;
	vec3ptr			angular_velocity;
};

struct contact_solver *		contact_solver_init_body_data(struct arena *mem, struct island *is, const f32 timestep);
void 				contact_solver_init_velocity_constraints(struct arena *mem, struct contact_solver *solver, const struct physics_pipeline *pipeline, const struct island *is);
void 				contact_solver_iterate_velocity_constraints(struct contact_solver *solver);
void 				contact_solver_warmup(struct contact_solver *solver, const struct island *is);
void 				contact_solver_cache_impulse_data(struct contact_solver *solver, const struct island *is);

#endif
