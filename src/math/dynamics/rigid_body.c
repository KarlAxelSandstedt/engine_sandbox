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

#include <float.h>
#include "float32.h"

#include "rigid_body.h"

void rigid_body_update_local_box(struct rigid_body *body, const struct collision_shape *shape)
{
	//TODO: Place in lookup table 
	//TODO: Does not take into account rotations

	vec3 min = { FLT_MAX, FLT_MAX, FLT_MAX };
	vec3 max = { -FLT_MAX, -FLT_MAX, -FLT_MAX };

	vec3 v;
	mat3 rot;
	quat_to_mat3(rot, body->rotation);

	if (body->shape_type == COLLISION_SHAPE_CONVEX_HULL)
	{
		for (u32 i = 0; i < shape->hull.v_count; ++i)
		{
			mat3_vec_mul(v, rot, shape->hull.v[i]);
			min[0] = f32_min(min[0], v[0]); 
			min[1] = f32_min(min[1], v[1]);			
			min[2] = f32_min(min[2], v[2]);			
                                                   
			max[0] = f32_max(max[0], v[0]);			
			max[1] = f32_max(max[1], v[1]);			
			max[2] = f32_max(max[2], v[2]);			
		}
	}
	else if (body->shape_type == COLLISION_SHAPE_SPHERE)
	{
		const f32 r = shape->sphere.radius;
		vec3_set(min, -r, -r, -r);
		vec3_set(max, r, r, r);
	}
	else if (body->shape_type == COLLISION_SHAPE_CAPSULE)
	{
		mat3_vec_mul(v, rot, shape->capsule.p1);
		vec3_set(max, 
			f32_max(-v[0], v[0]),
			f32_max(-v[1], v[1]),
			f32_max(-v[2], v[2]));
		vec3_add_constant(max, shape->capsule.radius);
		vec3_negative_to(min, max);
	}

	vec3_sub(body->local_box.hw, max, min);
	vec3_mul_constant(body->local_box.hw, 0.5f);
	vec3_add(body->local_box.center, min, body->local_box.hw);
}

void rigid_body_proxy(struct AABB *proxy, struct rigid_body *body)
{
	vec3_add(proxy->center, body->local_box.center, body->position);
	vec3_set(proxy->hw, body->local_box.hw[0] + body->margin,
			body->local_box.hw[1] + body->margin,
			body->local_box.hw[2] + body->margin);
}

#define VOL	0 
#define T_X 	1
#define T_Y 	2
#define T_Z 	3
#define T_XX	4
#define T_YY	5
#define T_ZZ	6
#define T_XY	7
#define T_YZ	8
#define T_ZX	9
	    
//TODO: REPLACE using table
static u32 comb(const u32 o, const u32 u)
{
	assert(u <= o);

	u32 v1 = 1;
	u32 v2 = 1;
	u32 rep = (u <= o-u) ? u : o-u;

	for (u32 i = 0; i < rep; ++i)
	{
		v1 *= (o-i);
		v2 *= (i+1);
	}

	assert(v1 % v2 == 0);

	return v1 / v2;
}

void statics_print(FILE *file, struct rigid_body *body)
{
	mat3_print("intertia tensor", body->inertia_tensor);
	fprintf(stderr, "mass: %f\n", body->mass);
}

static f32 statics_internal_line_integrals(const vec2 v0, const vec2 v1, const vec2 v2, const u32 p, const u32 q, const vec3 int_scalars)
{
       assert(p <= 4 && q <= 4);
       
       f32 sum = 0.0f;
       for (u32 i = 0; i <= p; ++i)
       {
               for (u32 j = 0; j <= q; ++j)
               {
                       sum += int_scalars[0] * comb(p, i) * comb(q, j) * f32_pow(v1[0], (f32) i) * f32_pow(v0[0], (f32) (p-i)) * f32_pow(v1[1], (f32) j) * f32_pow(v0[1], (f32) (q-j)) / comb(p+q, i+j);
                       sum += int_scalars[1] * comb(p, i) * comb(q, j) * f32_pow(v2[0], (f32) i) * f32_pow(v1[0], (f32) (p-i)) * f32_pow(v2[1], (f32) j) * f32_pow(v1[1], (f32) (q-j)) / comb(p+q, i+j);
                       sum += int_scalars[2] * comb(p, i) * comb(q, j) * f32_pow(v0[0], (f32) i) * f32_pow(v2[0], (f32) (p-i)) * f32_pow(v0[1], (f32) j) * f32_pow(v2[1], (f32) (q-j)) / comb(p+q, i+j);
               }
       }

       return sum / (p+q+1);
}

/*
 *  alpha beta gamma CCW
 */ 
static void statics_internal_calculate_face_integrals(f32 integrals[10], struct rigid_body *body, const struct collision_shape *shape, const u32 fi)
{
	f32 P_1   = 0.0f;
	f32 P_a   = 0.0f;
	f32 P_aa  = 0.0f;
	f32 P_aaa = 0.0f;
	f32 P_b   = 0.0f;
	f32 P_bb  = 0.0f;
	f32 P_bbb = 0.0f;
	f32 P_ab  = 0.0f;
	f32 P_aab = 0.0f;
	f32 P_abb = 0.0f;

	vec3 n, a, b;
	vec2 v0, v1, v2;

	vec3ptr v = shape->hull.v;
	struct hull_face *f = shape->hull.f + fi;
	struct hull_half_edge *e0 = shape->hull.e + f->first;
	struct hull_half_edge *e1 = shape->hull.e + f->first + 1;
	struct hull_half_edge *e2 = shape->hull.e + f->first + 2;

	vec3_sub(a, v[e1->origin], v[e0->origin]);
	vec3_sub(b, v[e2->origin], v[e0->origin]);
	vec3_cross(n, a, b);
	vec3_mul_constant(n, 1.0f / vec3_length(n));
	const f32 d = -vec3_dot(n, v[e0->origin]);

	u32 max_index = 0;
	if (n[max_index]*n[max_index] < n[1]*n[1]) { max_index = 1; }
	if (n[max_index]*n[max_index] < n[2]*n[2]) { max_index = 2; }

	/* maxized normal direction determines projected surface integral axes (we maximse the projected surface area) */
	
	const u32 a_i = (1+max_index) % 3;
	const u32 b_i = (2+max_index) % 3;
	const u32 y_i = max_index % 3;

	//vec3_set(n, n[a_i], n[b_i], n[y_i]);

	/* TODO: REPLACE */
	union { f32 f; u32 bits; } val = { .f = n[y_i] };
	const f32 n_sign = (val.bits >> 31) ? -1.0f : 1.0f;

	const u32 tri_count = f->count - 2;
	for (u32 i = 0; i < tri_count; ++i)
	{
		e0 = shape->hull.e + f->first;
		e1 = shape->hull.e + f->first + 1 + i;
		e2 = shape->hull.e + f->first + 2 + i;

		vec2_set(v0, v[e0->origin][a_i], v[e0->origin][b_i]);
		vec2_set(v1, v[e1->origin][a_i], v[e1->origin][b_i]);
		vec2_set(v2, v[e2->origin][a_i], v[e2->origin][b_i]);
		
		const vec3 delta_a =
		{
			v1[0] - v0[0],
			v2[0] - v1[0],
			v0[0] - v2[0],
		};
		
		const vec3 delta_b = 
		{
			v1[1] - v0[1],
			v2[1] - v1[1],
			v0[1] - v2[1],
		};

		/* simplify cross product of v1-v0, v2-v0 to get this */
		P_1   += ((v0[0] + v1[0])*delta_b[0] + (v1[0] + v2[0])*delta_b[1] + (v0[0] + v2[0])*delta_b[2]) / 2.0f;
		P_a   +=  statics_internal_line_integrals(v0, v1, v2, 2, 0, delta_b);
		P_aa  +=  statics_internal_line_integrals(v0, v1, v2, 3, 0, delta_b);
		P_aaa +=  statics_internal_line_integrals(v0, v1, v2, 4, 0, delta_b);
		P_b   += -statics_internal_line_integrals(v0, v1, v2, 0, 2, delta_a);
		P_bb  += -statics_internal_line_integrals(v0, v1, v2, 0, 3, delta_a);
		P_bbb += -statics_internal_line_integrals(v0, v1, v2, 0, 4, delta_a);
		P_ab  +=  statics_internal_line_integrals(v0, v1, v2, 2, 1, delta_b);
		P_aab +=  statics_internal_line_integrals(v0, v1, v2, 3, 1, delta_b);
		P_abb +=  statics_internal_line_integrals(v0, v1, v2, 1, 3, delta_b);
	}

	P_1   *= n_sign;
	P_a   *= (n_sign / 2.0f); 
	P_aa  *= (n_sign / 3.0f); 
	P_aaa *= (n_sign / 4.0f); 
	P_b   *= (n_sign / 2.0f); 
	P_bb  *= (n_sign / 3.0f); 
	P_bbb *= (n_sign / 4.0f); 
	P_ab  *= (n_sign / 2.0f); 
	P_aab *= (n_sign / 3.0f); 
	P_abb *= (n_sign / 3.0f); 

	const f32 a_y_div = n_sign / n[y_i];
	const f32 n_y_div = 1.0f / n[y_i];

	/* surface integrals */
	const f32 S_a 	= a_y_div * P_a;
	const f32 S_aa 	= a_y_div * P_aa;
	const f32 S_aaa = a_y_div * P_aaa;
	const f32 S_aab = a_y_div * P_aab;
	const f32 S_b 	= a_y_div * P_b;
	const f32 S_bb 	= a_y_div * P_bb;
	const f32 S_bbb = a_y_div * P_bbb;
	const f32 S_bby = -a_y_div * n_y_div * (n[a_i]*P_abb + n[b_i]*P_bbb + d*P_bb);
	const f32 S_y 	= -a_y_div * n_y_div * (n[a_i]*P_a + n[b_i]*P_b + d*P_1);
	const f32 S_yy 	= a_y_div * n_y_div * n_y_div * (n[a_i]*n[a_i]*P_aa + 2.0f*n[a_i]*n[b_i]*P_ab + n[b_i]*n[b_i]*P_bb 
			+ 2.0f*d*n[a_i]*P_a + 2.0f*d*n[b_i]*P_b + d*d*P_1);	
	const f32 S_yyy = -a_y_div * n_y_div * n_y_div * n_y_div * (n[a_i]*n[a_i]*n[a_i]*P_aaa + 3.0f*n[a_i]*n[a_i]*n[b_i]*P_aab
			+ 3.0f*n[a_i]*n[b_i]*n[b_i]*P_abb + n[b_i]*n[b_i]*n[b_i]*P_bbb + 3.0f*d*n[a_i]*n[a_i]*P_aa 
			+ 6.0f*d*n[a_i]*n[b_i]*P_ab + 3.0f*d*n[b_i]*n[b_i]*P_bb + 3.0f*d*d*n[a_i]*P_a
		       	+ 3.0f*d*d*n[b_i]*P_b + d*d*d*P_1);
	const f32 S_yya = a_y_div * n_y_div * n_y_div * (n[a_i]*n[a_i]*P_aaa + 2.0f*n[a_i]*n[b_i]*P_aab + n[b_i]*n[b_i]*P_abb 
			+ 2.0f*d*n[a_i]*P_aa + 2.0f*d*n[b_i]*P_ab + d*d*P_a);	

	if (max_index == 2)
	{
		integrals[VOL] += S_a * n[0];
	}
	else if (max_index == 1)
	{
		integrals[VOL] += S_b * n[0];
	}
	else
	{
		integrals[VOL] += S_y * n[0];
	}

	integrals[T_X + a_i] += S_aa * n[a_i] / 2.0f;
	integrals[T_X + b_i] += S_bb * n[b_i] / 2.0f;
	integrals[T_X + y_i] += S_yy * n[y_i] / 2.0f;

	integrals[T_XX + a_i] += S_aaa * n[a_i] / 3.0f;
	integrals[T_XX + b_i] += S_bbb * n[b_i] / 3.0f;
	integrals[T_XX + y_i] += S_yyy * n[y_i] / 3.0f;

	integrals[T_XY + a_i] += S_aab * n[a_i] / 2.0f;
	integrals[T_XY + b_i] += S_bby * n[b_i] / 2.0f;
	integrals[T_XY + y_i] += S_yya * n[y_i] / 2.0f;
}

/*
 * Mirtich's Algorithm (Dynamic Solutions to Multibody Systems, Appendix D)
 */
void statics_setup(struct rigid_body *body, const struct collision_shape *shape,  const f32 density)
{
	f32 I_xx = 0.0f;
	f32 I_yy = 0.0f;
	f32 I_zz = 0.0f;
	f32 I_xy = 0.0f;
	f32 I_xz = 0.0f;
	f32 I_yz = 0.0f;
	vec3 com = VEC3_ZERO;

	if (body->shape_type == COLLISION_SHAPE_CONVEX_HULL)
	{
		f32 integrals[10] = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f }; 

		for (u32 fi = 0; fi < shape->hull.f_count; ++fi)
		{
			statics_internal_calculate_face_integrals(integrals, body, shape, fi);
		}

//		fprintf(stderr, "c_hull Volume integrals: %f, %f, %f, %f, %f, %f, %f, %f, %f, %f\n",
//				 integrals[VOL ],
//				 integrals[T_X ],
//				 integrals[T_Y ],
//				 integrals[T_Z ],
//				 integrals[T_XX],
//				 integrals[T_YY],
//				 integrals[T_ZZ],
//      	                   integrals[T_XY],
//      	                   integrals[T_YZ],
//      	                   integrals[T_ZX]);

		body->mass = integrals[VOL] * density;
		assert(body->mass >= 0.0f);

		/* center of mass */
		vec3_set(com,
			integrals[T_X] * density / body->mass,
		       	integrals[T_Y] * density / body->mass,
		       	integrals[T_Z] * density / body->mass
		);

		I_xx = density * (integrals[T_YY] + integrals[T_ZZ]) - body->mass * (com[1]*com[1] + com[2]*com[2]);
		I_yy = density * (integrals[T_XX] + integrals[T_ZZ]) - body->mass * (com[0]*com[0] + com[2]*com[2]);
		I_zz = density * (integrals[T_XX] + integrals[T_YY]) - body->mass * (com[0]*com[0] + com[1]*com[1]);
		I_xy = density * integrals[T_XY] - body->mass * com[0] * com[1];
		I_xz = density * integrals[T_ZX] - body->mass * com[0] * com[2];
		I_yz = density * integrals[T_YZ] - body->mass * com[1] * com[2];
	
		/* set local frame coordinates */
		vec3_copy(body->position, com);
		vec3_negative(com);

		for (u32 i = 0; i < shape->hull.v_count; ++i)
		{
			vec3_translate(shape->hull.v[i], com);
		}

		mat3_set(body->inertia_tensor, I_xx, -I_xy, -I_xz,
			       		 	 -I_xy,  I_yy, -I_yz,
						 -I_xz, -I_yz, I_zz);
	}
	else if (body->shape_type == COLLISION_SHAPE_SPHERE)
	{
		//TODO 
		const f32 r = shape->sphere.radius;
		const f32 rr = r*r;
		const f32 rrr = rr*r;
		body->mass = density * 4.0f * F32_PI * rrr / 3.0f;
		I_xx = 2.0f * body->mass * rr / 5.0f;
		I_yy = I_xx;
		I_zz = I_xx;
		I_xy = 0.0f;
		I_yz = 0.0f;
		I_xz = 0.0f;

		mat3_set(body->inertia_tensor, I_xx, -I_xy, -I_xz,
			       		 	 -I_xy,  I_yy, -I_yz,
						 -I_xz, -I_yz, I_zz);
	}
	else if (body->shape_type == COLLISION_SHAPE_CAPSULE)
	{
		vec3 p, rot_axis;
		vec3 y = { 0.0f, 1.0f, 0.0f };
		vec3_normalize(p, shape->capsule.p1);
		vec3_cross(rot_axis, y, p);
		mat3 rot, rot_T, m1, m2;

		if (vec3_dot(rot_axis, rot_axis) > 0.0f)
		{
			quat toY;
			unit_axis_angle_to_quaternion(toY, rot_axis, f32_acos(vec3_dot(p, y)));
			quat_to_mat3(rot, body->rotation);
			mat3_transpose_to(rot_T, rot);
		}
		else
		{
			mat3_identity(rot);
			mat3_identity(rot_T);
		}

		const f32 r = shape->capsule.radius;
		const f32 h = vec3_length(shape->capsule.p1);
		const f32 hpr = h+r;
		const f32 hmr = h-r;

		body->mass = density * 4.0f * F32_PI * r*r*r / 3.0f + density * 2.0f *h * F32_PI * r*r;

		const f32 I_xx_cap_up = (4.0f * F32_PI * r*r * h*h*h + 3.0f * F32_PI * r*r*r*r * h) / 6.0f;
		const f32 I_xx_sph_up = 2.0f * F32_PI * r*r * (hpr*hpr*hpr - hmr*hmr*hmr) / 3.0f + F32_PI * r*r*r*r*r;
		const f32 I_xx_up = I_xx_sph_up + I_xx_cap_up;
		const f32 I_zz_up = I_xx_up;

		const f32 I_yy_cap_up = F32_PI * r*r*r*r * h;
		const f32 I_yy_sph_up = 2.0f * F32_PI * r*r*r*r*r;
		const f32 I_yy_up = I_yy_cap_up + I_yy_sph_up;

		const f32 I_xy_up = 0;
		const f32 I_yz_up = 0;
		const f32 I_xz_up = 0;

		/* Derive */
		mat3_set(m1, I_xx_up, -I_xy_up, -I_xz_up,
			       		 	 -I_xy_up,  I_yy_up, -I_yz_up,
						 -I_xz_up, -I_yz_up,  I_zz_up);

		mat3_mult(m2, m1, rot);
		mat3_mult(body->inertia_tensor, rot_T, m2);
	}

	mat3_inverse(body->inv_inertia_tensor, body->inertia_tensor);
}
