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

#include <string.h>
#include "kas_math.h"
#include "geometry.h"
#include "hash_map.h"
#include "array_list.h"
#include "queue.h"
#include "float32.h"
#include "sys_public.h"

//TODO what to do with this?
#define MIN_SEGMENT_LENGTH_SQ	(100.0f*F32_EPSILON)

void vertex_support(vec3 support, const vec3 dir, const vec3ptr v, const u32 v_count)
{
	u32 best = U32_MAX;
	f32 max_dist = F32_INFINITY;
	for (u32 i = 0; i < v_count; ++i)
	{
		const f32 dist = vec3_dot(dir, v[i]);

		if (max_dist < dist)
		{
			best = i;
			max_dist = dist;
		}
	}

	vec3_copy(support, v[best]);
}

struct ray ray_construct(const vec3 origin, const vec3 dir)
{
	assert(vec3_length_squared(dir) > 0.0f);

	struct ray r;
	vec3_copy(r.origin, origin);
	vec3_copy(r.dir, dir);
	return r;
}

struct segment ray_construct_segment(const struct ray *r, const f32 t)
{
	vec3 p;
	vec3_copy(p, r->origin);
	vec3_translate_scaled(p, r->dir, t);
	return segment_construct(r->origin, p);
}


void ray_point(vec3 ray_point, const struct ray *ray, const f32 t)
{
	vec3_copy(ray_point, ray->origin);
	vec3_translate_scaled(ray_point, ray->dir, t);
}

struct sphere sphere_construct(const vec3 center, const f32 radius)
{
	struct sphere sph = { .radius = radius };
	vec3_copy(sph.center, center);
	return sph;	
}

/*
 * | r.o + t*r.dir - s.c |^2 = s.r^2 => solve quadratic formula give the following method.
 */
f32 sphere_raycast_parameter(const struct sphere *sph, const struct ray *ray)
{
	vec3 diff;
	vec3_sub(diff, ray->origin, sph->center);

	const f32 a = vec3_dot(ray->dir, ray->dir);
	const f32 b = 2.0f * vec3_dot(ray->dir, diff);
	const f32 c = vec3_dot(diff, diff) - sph->radius*sph->radius;

	const f32 square = (b*b - 4.0f*a*c);
	if (square < 0.0f) { return F32_INFINITY; }

	const f32 root = f32_sqrt(square);

	const f32 t2 = -b + root;
	if (t2 < 0.0f) { return F32_INFINITY; }

	const f32 t1 = -b - root;
	return (t1 >= 0.0f) ? t1 / (2.0f * a) : t2 / (2.0f * a);
}

f32 ray_point_closest_point_parameter(const struct ray *ray, const vec3 p)
{ 
	vec3 diff;
	vec3_sub(diff, p, ray->origin);
	const f32 tr = vec3_dot(diff, ray->dir) / vec3_dot(ray->dir, ray->dir);
	return (tr >= 0.0f) ? tr : 0.0f;
}

f32 ray_point_distance_sq(vec3 r_c, const struct ray *ray, const vec3 p)
{
	const f32 t = ray_point_closest_point_parameter(ray, p);
	ray_point(r_c, ray, t);
	return vec3_distance_squared(r_c, p);
}

f32 ray_segment_distance_sq(vec3 r_c, vec3 s_c, const struct ray *ray, const struct segment *s)
{
	vec3 diff;
	vec3_sub(diff, s->p0, ray->origin);
	const f32 drdr = vec3_dot(ray->dir, ray->dir);
	const f32 dsds = vec3_dot(s->dir, s->dir);

	f32 tr = 0.0f;
	f32 ts = 0.0f;

	if (dsds >= MIN_SEGMENT_LENGTH_SQ)
	{
		const f32 drds = vec3_dot(ray->dir, s->dir);
		const f32 diffdr = vec3_dot(diff, ray->dir);
		const f32 diffds = vec3_dot(diff, s->dir);
		const f32 denom = drdr*dsds - drds*drds;
		/* Check that the ray and segment are not parallel */
		if (denom > 0.0f)
		{
			tr = (diffdr*dsds - diffds*drds) / denom;
			tr = (tr >= 0.0f) ? tr : 0.0f;
		}

		ts = f32_clamp(tr*drds - diffds, 0.0f, dsds);
		if (ts == 0.0f)
		{
			tr = diffdr / drdr;
			tr = (tr >= 0.0f) ? tr : 0.0f;
		}
		else if (ts == dsds)
		{
			ts = 1.0f;
			tr = (diffdr + drds) / drdr;
			tr = (tr >= 0.0f) ? tr : 0.0f;
		}	
		else
		{
			ts /= dsds;
		}
	}
	else
	{
		tr = f32_clamp(vec3_dot(diff, ray->dir) / drdr, 0.0f, 1.0f);
	}
	
	assert(0.0f <= tr);
	assert(0.0f <= ts && ts <= 1.0f);

	ray_point(r_c, ray, tr);
	segment_bc(s_c, s, ts);
	return vec3_distance_squared(r_c, s_c);
}

struct segment segment_construct(const vec3 p0, const vec3 p1)
{
	struct segment s;
	vec3_copy(s.p0, p0);
	vec3_copy(s.p1, p1);
	vec3_sub(s.dir, p1, p0);
	return s;
}

f32 segment_distance_sq(vec3 c1, vec3 c2, const struct segment *s1, const struct segment *s2)
{
	vec3 diff;
	vec3_sub(diff, s2->p0, s1->p0);
	const f32 d1d1 = vec3_length_squared(s1->dir);
	const f32 d2d2 = vec3_length_squared(s2->dir);

	f32 t1 = 0.0f;
	f32 t2 = 0.0f;

	if (d1d1 >= MIN_SEGMENT_LENGTH_SQ && d2d2 >= MIN_SEGMENT_LENGTH_SQ)
	{
		const f32 d1d2 = vec3_dot(s1->dir, s2->dir);
		const f32 diffd1 = vec3_dot(diff, s1->dir);
		const f32 diffd2 = vec3_dot(diff, s2->dir);
		const f32 denom = d1d1*d2d2 - d1d2*d1d2;
		/* Check that the segments are not parallel */
		if (denom > 0.0f)
		{
			t1 = f32_clamp((diffd1*d2d2 - diffd2*d1d2) / denom, 0.0f, 1.0f);
		}

		/*
		 *  t2 = (L1_P1*(1-t1) + L1_P2*t1 - L2_P1) * DIR2 / DIR2*DIR2
		 *     = (-DIFF + DIR1*t1) * DIR2 / DIR2*DIR2
		 *     = (-DIFF*DIR2 + DIR1*DIR2*t1) / DIR2*DIR2
		 */
		t2 = f32_clamp(t1*d1d2 - diffd2, 0.0f, d2d2);

		if (t2 == 0.0f)
		{
			/*
			 *  t1 = (L2_P1*(1-t2) + L2_P2*t2 - L1_P1) * DIR1 / DIR1*DIR1
			 *     = (DIFF + DIR2*t2) * DIR1 / DIR1*DIR1
			 *     = DIFF*DIR1 / DIR1*DIR1
			 */
			t1 = f32_clamp(diffd1 / d1d1, 0.0f, 1.0f);
		}
		else if (t2 == d2d2)
		{
			t2 = 1.0f;
			t1 = f32_clamp((diffd1 + d1d2) / d1d1, 0.0f, 1.0f);
		}	
		else
		{
			t2 /= d2d2;
		}
	}
	/* S2 is point */
	else if (d1d1 >= MIN_SEGMENT_LENGTH_SQ)
	{
		/* 
		 * SIGNED PROJECTED LENGTH 
		 * 	= (L2_P1 - L1_P1) * DIR1 / |DIR1| 
		 * 	= t1*|DIR1|
		 * => t = DIFF*DIR1 / (DIR1*DIR1) 
		 */
		t1 = f32_clamp(vec3_dot(diff, s1->dir) / d1d1, 0.0f, 1.0f);
	}
	else if (d2d2 >= MIN_SEGMENT_LENGTH_SQ)
	{
		t2 = f32_clamp(-vec3_dot(diff, s2->dir) / d2d2, 0.0f, 1.0f);
	}

	assert(0.0f <= t1 && t1 <= 1.0f);
	assert(0.0f <= t2 && t2 <= 1.0f);

	segment_bc(c1, s1, t1);
	segment_bc(c2, s2, t2);
	return vec3_distance_squared(c1, c2);
}

f32 segment_point_distance_sq(vec3 c, const struct segment *s, const vec3 p)
{
	f32 t = 0.0f;
	
	if (vec3_length_squared(s->dir) >= MIN_SEGMENT_LENGTH_SQ)
	{
		vec3 diff;
		vec3_sub(diff, p, s->p0);
		t = f32_clamp(vec3_dot(diff, s->dir) / vec3_dot(s->dir, s->dir), 0.0f, 1.0f);
	}

	segment_bc(c, s, t);
	return vec3_distance_squared(c, p);
}

void segment_bc(vec3 bc_p, const struct segment *s, const f32 t)
{
	vec3_interpolate(bc_p, s->p1, s->p0, t);
}

f32 segment_point_projected_bc_parameter(const struct segment *s, const vec3 p)
{	
	vec3 diff;
	vec3_sub(diff, p, s->p0);
	return vec3_dot(diff, s->dir) / vec3_dot(s->dir, s->dir);
}

f32 segment_point_closest_bc_parameter(const struct segment *s, const vec3 p)
{	
	vec3 diff;
	vec3_sub(diff, p, s->p0);
	return f32_clamp(vec3_dot(diff, s->dir) / vec3_dot(s->dir, s->dir), 0.0f, 1.0f);
}

struct plane plane_construct(const vec3 n, const vec3 p)
{
	struct plane pl;
	vec3_copy(pl.normal, n);
	pl.signed_distance = vec3_dot(n, p);
	return pl;
}

struct plane plane_construct_from_ccw_triangle(const vec3 a, const vec3 b, const vec3 c)
{
	vec3 ab, ac, cross;
	vec3_sub(ab, b, a);
	vec3_sub(ac, c, a);
	vec3_cross(cross, ab, ac);
	vec3_mul_constant(cross, 1.0f/vec3_length(cross));
	return plane_construct(cross, a);
}

u32 plane_point_is_infront(const struct plane *pl, const vec3 p)
{
	return (plane_point_signed_distance(pl, p) > 0.0f) ? 1 : 0;
}

u32 plane_point_is_behind(const struct plane *pl, const vec3 p)
{
	return (plane_point_signed_distance(pl, p) < 0.0f) ? 1 : 0;
}

f32 plane_segment_clip_parameter(const struct plane *pl, const struct segment *s)
{
	/*
	 * 	GIVEN: pl.normal and segment direction not orthogonal.
	 *
	 * 	s.p0 + t*s.dir = PLANE POINT
	 * =>   DOT(s.p0 + t*s.dir - pl.normal*pl.signed_distance, pl.normal) = 0
	 * =>   DOT(t*s.dir pl.normal) = DOT(pl.normal*pl.signed_distance - s.p0, pl.normal)
	 * =>   t = [pl.signed_distance - DOT(s.p0, pl.normal)] / DOT(s.dir, pl.normal)
	 *
	 * degenerate case: segment parallel to plane gives t = +-infinity, which is okay!
	 */
	return (pl->signed_distance - vec3_dot(pl->normal, s->p0)) / vec3_dot(pl->normal, s->dir);
}

u32 plane_segment_clip(vec3 clip, const struct plane *pl, const struct segment *s)
{
	const f32 t = plane_segment_clip_parameter(pl, s);
	u32 intersection = 0;

	if (0.0f <= t && t <= 1.0f)
	{
		intersection = 0;
		segment_bc(clip, s, t);
	} 
	
	return intersection;
}

u32 plane_segment_test(const struct plane *pl, const struct segment *s)
{
	const f32 t = plane_segment_clip_parameter(pl, s);
	return (0.0f <= t && t <= 1.0f) ? 1 : 0;
}

f32 plane_point_signed_distance(const struct plane *pl, const vec3 p)
{
	return vec3_dot(pl->normal, p) - pl->signed_distance;
}

f32 plane_point_distance(const struct plane *pl, const vec3 p)
{
	return f32_abs(vec3_dot(pl->normal, p) - pl->signed_distance);
}

f32 plane_raycast_parameter(const struct plane *plane, const struct ray *ray)
{
	const f32 dot = vec3_dot(ray->dir, plane->normal);
	if (dot == 0.0f) { return F32_INFINITY; }

	return (plane->signed_distance - vec3_dot(ray->origin, plane->normal)) / dot;
}

u32 plane_raycast(vec3 intersection, const struct plane *plane, const struct ray *ray)
{
	const f32 t = plane_raycast_parameter(plane, ray);
	if (t < 0.0f || t == F32_INFINITY) { return 0; }

	vec3_copy(intersection, ray->origin);
	vec3_translate_scaled(intersection, ray->dir, t);
	return 1;
}

void AABB_vertex(struct AABB *dst, const vec3ptr v, const u32 v_count, const f32 margin)
{
	vec3 min = { F32_INFINITY, F32_INFINITY, F32_INFINITY };
	vec3 max = { -F32_INFINITY, -F32_INFINITY, -F32_INFINITY };
	for (u32 i = 0; i < v_count; ++i)
	{
		min[0] = f32_min(min[0], v[i][0]); 
		min[1] = f32_min(min[1], v[i][1]);			
		min[2] = f32_min(min[2], v[i][2]);			

		max[0] = f32_max(max[0], v[i][0]);			
		max[1] = f32_max(max[1], v[i][1]);			
		max[2] = f32_max(max[2], v[i][2]);			
	}

	vec3_sub(dst->hw, max, min);
	vec3_mul_constant(dst->hw, 0.5f);
	vec3_add(dst->center, min, dst->hw);

	dst->hw[0] += margin;
	dst->hw[1] += margin;
	dst->hw[2] += margin;
}

void AABB_union(struct AABB *box_union, const struct AABB *a, const struct AABB *b)
{
	vec3 min, max;
	
	min[0] = f32_min(a->center[0] - a->hw[0], b->center[0] - b->hw[0]);
	min[1] = f32_min(a->center[1] - a->hw[1], b->center[1] - b->hw[1]);
	min[2] = f32_min(a->center[2] - a->hw[2], b->center[2] - b->hw[2]);
                                                                      
	max[0] = f32_max(a->center[0] + a->hw[0], b->center[0] + b->hw[0]);
	max[1] = f32_max(a->center[1] + a->hw[1], b->center[1] + b->hw[1]);
	max[2] = f32_max(a->center[2] + a->hw[2], b->center[2] + b->hw[2]);
	
	vec3_sub(box_union->hw, max, min);
	vec3_mul_constant(box_union->hw, 0.5f);
	vec3_add(box_union->center, box_union->hw, min);
}

u32 AABB_test(const struct AABB *a, const struct AABB *b)
{
	if (b->center[0] - b->hw[0] - (a->center[0] + a->hw[0]) > 0.0f 
			|| a->center[0] - a->hw[0] - (b->center[0] + b->hw[0]) > 0.0f) { return 0; }
	if (b->center[1] - b->hw[1] - (a->center[1] + a->hw[1]) > 0.0f 
			|| a->center[1] - a->hw[1] - (b->center[1] + b->hw[1]) > 0.0f) { return 0; }
	if (b->center[2] - b->hw[2] - (a->center[2] + a->hw[2]) > 0.0f 
			|| a->center[2] - a->hw[2] - (b->center[2] + b->hw[2]) > 0.0f) { return 0; }

	return 1;
}

u32 AABB_contains(const struct AABB *a, const struct AABB *b)
{
	if (b->center[0] - b->hw[0] < a->center[0] - a->hw[0]) { return 0; }
	if (b->center[1] - b->hw[1] < a->center[1] - a->hw[1]) { return 0; }
	if (b->center[2] - b->hw[2] < a->center[2] - a->hw[2]) { return 0; }
	
	if (b->center[0] + b->hw[0] > a->center[0] + a->hw[0]) { return 0; }
	if (b->center[1] + b->hw[1] > a->center[1] + a->hw[1]) { return 0; }
	if (b->center[2] + b->hw[2] > a->center[2] + a->hw[2]) { return 0; }

	return 1;
}

u32 AABB_raycast(vec3 intersection, const struct AABB *aabb, const struct ray *ray)
{
	vec3 p, q;
	vec3_sub(p, ray->origin, aabb->center);
	const vec3 sign = { f32_sign(p[0]), f32_sign(p[1]), f32_sign(p[2]) };

	struct ray new_ray = ray_construct(p,  ray->dir);

	vec3_abs_to(q, p);
	vec3_translate_scaled(q, aabb->hw, -1.0f);
	const u32 sx = f32_sign_bit(q[0]);
	const u32 sy = f32_sign_bit(q[1]);
	const u32 sz = f32_sign_bit(q[2]);

	/* Check planes towards */
	if (sx + sy + sz == 3)
	{
		struct plane plane;
		vec3_set(plane.normal, f32_sign(new_ray.dir[0]), 0.0f, 0.0f);
		plane.signed_distance = aabb->hw[0];
		if (new_ray.dir[0] != 0.0f
			         && plane_raycast(intersection, &plane, &new_ray)
				 && (intersection[1] >= -aabb->hw[1] && intersection[1] <= aabb->hw[1])
				 && (intersection[2] >= -aabb->hw[2] && intersection[2] <= aabb->hw[2])
				)
		{
			vec3_translate(intersection, aabb->center);
			return 1;	
		}

		vec3_set(plane.normal, 0.0f, f32_sign(new_ray.dir[1]), 0.0f);
		plane.signed_distance = aabb->hw[1];
		if (new_ray.dir[1] != 0.0f
				 && plane_raycast(intersection, &plane, &new_ray)
				 && (intersection[0] >= -aabb->hw[0] && intersection[0] <= aabb->hw[0])
				 && (intersection[2] >= -aabb->hw[2] && intersection[2] <= aabb->hw[2])
				)
		{
			vec3_translate(intersection, aabb->center);
			return 1;	
		}

		vec3_set(plane.normal, 0.0f, 0.0f, f32_sign(new_ray.dir[2]));
		plane.signed_distance = aabb->hw[2];
		if (new_ray.dir[2] != 0.0f
				 && plane_raycast(intersection, &plane, &new_ray)
				 && (intersection[0] >= -aabb->hw[0] && intersection[0] <= aabb->hw[0])
				 && (intersection[1] >= -aabb->hw[1] && intersection[1] <= aabb->hw[1])
				)
		{
			vec3_translate(intersection, aabb->center);
			return 1;	
		}

	}
	/* check planes against */
	else
	{
		struct plane plane;
		vec3_set(plane.normal, sign[0], 0.0f, 0.0f);
		plane.signed_distance = aabb->hw[0];
		if (sx == 0 && plane_raycast(intersection, &plane, &new_ray)
				 && (intersection[1] >= -aabb->hw[1] && intersection[1] <= aabb->hw[1])
				 && (intersection[2] >= -aabb->hw[2] && intersection[2] <= aabb->hw[2])
				)
		{
			vec3_translate(intersection, aabb->center);
			return 1;	
		}

		vec3_set(plane.normal, 0.0f, sign[1], 0.0f);
		plane.signed_distance = aabb->hw[1];
		if (sy == 0 && plane_raycast(intersection, &plane, &new_ray)
				 && (intersection[0] >= -aabb->hw[0] && intersection[0] <= aabb->hw[0])
				 && (intersection[2] >= -aabb->hw[2] && intersection[2] <= aabb->hw[2])
				)
		{
			vec3_translate(intersection, aabb->center);
			return 1;	
		}

		vec3_set(plane.normal, 0.0f, 0.0f, sign[2]);
		plane.signed_distance = aabb->hw[2];
		if (sz == 0 && plane_raycast(intersection, &plane, &new_ray)
				 && (intersection[0] >= -aabb->hw[0] && intersection[0] <= aabb->hw[0])
				 && (intersection[1] >= -aabb->hw[1] && intersection[1] <= aabb->hw[1])
				)
		{
			vec3_translate(intersection, aabb->center);
			return 1;	
		}
	}

	return 0;
}

i32 DCEL_half_edge_add(struct DCEL *dcel, struct arena *table_mem, const i32 origin, const i32 twin, const i32 face_ccw, const i32 next, const i32 prev)
{
	if (dcel->next_he == -1) 
	{ 
		dcel->next_he = dcel->num_he;
		arena_push_packed(table_mem, sizeof(struct DCEL_half_edge));
		dcel->he_table[dcel->next_he].next = -1;
		dcel->num_he += 1;
       	}

	const i32 tmp = dcel->he_table[dcel->next_he].next;
	dcel->he_table[dcel->next_he].he = dcel->next_he;
	dcel->he_table[dcel->next_he].origin = origin;
	dcel->he_table[dcel->next_he].twin = twin;
	dcel->he_table[dcel->next_he].face_ccw = face_ccw;
	dcel->he_table[dcel->next_he].next = next;
	dcel->he_table[dcel->next_he].prev = prev;
	dcel->next_he = tmp;

	return dcel->he_table[dcel->next_he].he;
}

i32 DCEL_half_edge_reserve(struct DCEL *dcel, struct arena *table_mem)
{
	if (dcel->next_he == -1) 
	{ 
		dcel->next_he = dcel->num_he;
		arena_push_packed(table_mem, sizeof(struct DCEL_half_edge));
		dcel->he_table[dcel->next_he].next = -1;
		dcel->num_he += 1;
	}

	const i32 he = dcel->next_he;
	dcel->next_he = dcel->he_table[he].next;

	return he;
}

void DCEL_half_edge_set(struct DCEL *dcel, const i32 he, const i32 origin, const i32 twin, const i32 face_ccw, const i32 next, const i32 prev)
{
	assert(he >= 0 && he < dcel->num_he);

	dcel->he_table[he].he = he;
	dcel->he_table[he].origin = origin;
	dcel->he_table[he].twin = twin;
	dcel->he_table[he].face_ccw = face_ccw;
	dcel->he_table[he].next = next;
	dcel->he_table[he].prev = prev;
}

i32 DCEL_face_add(struct DCEL *dcel, struct arena *face_mem, const i32 edge, const i32 unit)
{
	if (dcel->next_face == -1) 
	{ 
		dcel->next_face = dcel->num_faces;
		arena_push_packed(face_mem, sizeof(struct DCEL_face));
		dcel->num_faces += 1;
		dcel->faces[dcel->next_face].he_index = -1;
	}

	const i32 face = dcel->next_face;
	const i32 tmp = dcel->faces[face].he_index;
	dcel->faces[face].he_index = edge;
	dcel->faces[face].relation_unit = unit;
	dcel->next_face = tmp;

	return face;
}

void DCEL_half_edge_remove(struct DCEL *dcel, const i32 he)
{
	assert(he >= 0 && he < dcel->num_he);

	const i32 tmp = dcel->next_he;
	dcel->next_he = he;
	dcel->he_table[he].next = tmp;
	dcel->he_table[he].face_ccw = -1;
}

void DCEL_face_remove(struct DCEL *dcel, const i32 face)
{
	assert(face >= 0 && face < dcel->num_faces);

	const i32 tmp = dcel->next_face;
	dcel->next_face = face;
	dcel->faces[face].he_index = tmp;
	dcel->faces[face].relation_unit = -1;
}

i32 tetrahedron_indices(i32 indices[4], const vec3ptr v, const i32 v_count, const f32 tol)
{
	vec3 a, b, c, d;

	/* Find two points not to close to each other */
	for (i32 i = 1; i <= v_count; ++i)
	{
		/* all points are distance <= tol from first point */
		if (i == v_count) { return 0; }

		vec3_sub(a, v[i], v[0]);
		const f32 len = vec3_length(a);
		if (len > tol)
		{
			vec3_mul_constant(a, 1.0f / len);
			indices[1] = i;
			break;
		}
	}	

	/* Find non-collinear point */
	for (i32 i = indices[1] + 1; i <= v_count; ++i)
	{
		/* all points are collinear */
		if (i == v_count) { return 0; }

		vec3_sub(b, v[i], v[0]);
		const f32 dot = vec3_dot(a, b);
		
		if (f32_sqrt(vec3_length(b)*vec3_length(b) - dot*dot) > tol)
		{
			indices[2] = i;
			break;
		}
	}

	/* Find non-coplanar point */
	for (i32 i = indices[2] + 1; i <= v_count; ++i)
	{
		/* all points are coplanar */
		if (i == v_count) { return 0; }

		/* plane normal */
		vec3_cross(c, a, b);
		vec3_mul_constant(c, 1.0f / vec3_length(c));

		vec3_sub(d, v[i], v[0]);
		if (f32_abs(vec3_dot(d, c)) > tol)
		{
			indices[3] = i;
			break;
		}
	}

	return 1;
}

void triangle_CCW_relative_to(vec3 BCA[3], const vec3 p)
{
	vec3 AB, AC, AP, N;
	vec3_sub(AB, BCA[0], BCA[2]);
	vec3_sub(AC, BCA[1], BCA[2]);
	vec3_sub(AP, p, BCA[2]);
	vec3_cross(N, AB, AC);
	if (vec3_dot(N, AP) <= 0.0f)
	{
		vec3_copy(AB, BCA[0]);
		vec3_copy(BCA[0], BCA[1]);
		vec3_copy(BCA[1], AB);
	}
}

void triangle_CCW_relative_to_origin(vec3 BCA[3])
{
	vec3 AB, AC, AO, N;
	vec3_sub(AB, BCA[0], BCA[2]);
	vec3_sub(AC, BCA[1], BCA[2]);
	vec3_scale(AO, BCA[2], -1.0f);
	vec3_cross(N, AB, AC);
	if (vec3_dot(N, AO) <= 0.0f)
	{
		vec3_copy(AB, BCA[0]);
		vec3_copy(BCA[0], BCA[1]);
		vec3_copy(BCA[1], AB);
	}
}

void triangle_CCW_normal(vec3 normal, const vec3 p0, const vec3 p1, const vec3 p2)
{
	vec3 A, B, C;
	vec3_sub(A, p1, p0);
	vec3_sub(B, p2, p0);
	vec3_cross(C, A, B);
	vec3_normalize(normal, C);
}

void convex_centroid(vec3 centroid, vec3ptr vs, const u32 n)
{
	assert(n > 0);
	vec3_set(centroid, 0.0f, 0.0f, 0.0f);
	for (u32 i = 0; i < n; ++i)
	{
		vec3_translate(centroid, vs[i]);
	}
	vec3_mul_constant(centroid, 1.0f / n);
}

u32 convex_support(vec3 support, const vec3 dir, vec3ptr vs, const u32 n)
{
	f32 max = F32_INFINITY;
	u32 max_index = 0;
	for (u32 i = 0; i < n; ++i)
	{
		const f32 dot = vec3_dot(vs[i], dir);
		if (max < dot)
		{
			max_index = i;
			max = dot; 
		}
	}

	vec3_copy(support, vs[max_index]);
	return max_index;
}

f32 GJK_internal_tolerance(vec3ptr vs_1, const u32 n_1, vec3ptr vs_2, const u32 n_2, const f32 tol)
{
	vec3 c_1, c_2;
	convex_centroid(c_1, vs_1, n_1);
	convex_centroid(c_2, vs_2, n_2);

	/* Get max distance from centroids to convex surfaces */
	f32 l_sq_1 = 0.0f;
	for (u32 i = 0; i < n_1; ++i)
	{
		const f32 l_sq = vec3_distance_squared(c_1, vs_1[i]);
		if (l_sq_1 < l_sq)
		{
			l_sq_1 = l_sq;
		}
	}

	f32 l_sq_2 = 0.0f;
	for (u32 i = 0; i < n_2; ++i)
	{
		const f32 l_sq = vec3_distance_squared(c_2, vs_2[i]);
		if (l_sq_2 < l_sq)
		{
			l_sq_2 = l_sq;
		}
	}

	const f32 D = f32_sqrt(l_sq_1) + f32_sqrt(l_sq_2) + vec3_distance(c_1, c_2);
	return tol * D*D;
}

u64 convex_minkowski_difference_support(vec3 support, const vec3 dir, vec3ptr A, const u32 n_A, vec3ptr B, const u32 n_B)
{
	vec3 v_1, v_2, support_dir;
	const u32 i_1 = convex_support(v_1, dir, A, n_A);
	vec3_scale(support_dir, dir, -1.0f);
	const u32 i_2 = convex_support(v_2, support_dir, B, n_B);
	vec3_sub(support, v_1, v_2);

	return (u64) i_1 << 32 | (u64) i_2;
}

u64 convex_minkowski_difference_world_support(vec3 support, const vec3 dir, const vec3 pos_A, vec3ptr A, const u32 n_A, const vec3 pos_B, vec3ptr B, const u32 n_B)
{
	vec3 v_1, v_2, support_dir;
	const u32 i_1 = convex_support(v_1, dir, A, n_A);
	vec3_translate(v_1, pos_A);
	vec3_scale(support_dir, dir, -1.0f);
	const u32 i_2 = convex_support(v_2, support_dir, B, n_B);
	vec3_translate(v_2, pos_B);
	vec3_sub(support, v_1, v_2);

	return (u64) i_1 << 32 | (u64) i_2;
}

static struct gjk_simplex GJK_internal_simplex_init(void)
{
	struct gjk_simplex simplex = 
	{
		.id = {U64_MAX, U64_MAX, U64_MAX, U64_MAX},
		.dot = { -1.0f, -1.0f, -1.0f, -1.0f },
		.type = U32_MAX,
	};

	return simplex;
}

static u32 GJK_internal_johnsons_algorithm(struct gjk_simplex *simplex, vec3 c_v, vec4 lambda)
{
	vec3 a;

	if (simplex->type == 0)
	{
		vec3_copy(c_v, simplex->p[0]);
	}
	else if (simplex->type == 1)
	{
		vec3_sub(a, simplex->p[0], simplex->p[1]);
		const f32 delta_01_1 = vec3_dot(a, simplex->p[0]);

		if (delta_01_1 > 0.0f)
		{
			vec3_sub(a, simplex->p[1], simplex->p[0]);
			const f32 delta_01_0 = vec3_dot(a, simplex->p[1]);
			if (delta_01_0 > 0.0f)
			{
				const f32 delta = delta_01_0 + delta_01_1;
				lambda[0] = delta_01_0 / delta;
				lambda[1] = delta_01_1 / delta;
				vec3_set(c_v,
				       	(lambda[0]*(simplex->p[0])[0] + lambda[1]*(simplex->p[1])[0]),
				       	(lambda[0]*(simplex->p[0])[1] + lambda[1]*(simplex->p[1])[1]),
				       	(lambda[0]*(simplex->p[0])[2] + lambda[1]*(simplex->p[1])[2]));
			}
			else
			{
				simplex->type = 0;
				vec3_copy(c_v, simplex->p[1]);
				vec3_copy(simplex->p[0], simplex->p[1]);
			}
		}
		else
		{
			/* 
			 * numerical issues, new simplex should always contain newly added point
			 * of simplex, terminate next iteration. Let c_v stay the same as in the
			 * previous iteration.
			 */
			return 1;
		}
	}
	else if (simplex->type == 2)
	{
		vec3_sub(a, simplex->p[1], simplex->p[0]);
		const f32 delta_01_0 = vec3_dot(a, simplex->p[1]);
		vec3_sub(a, simplex->p[0], simplex->p[1]);
		const f32 delta_01_1 = vec3_dot(a, simplex->p[0]);
		vec3_sub(a, simplex->p[0], simplex->p[2]);
		const f32 delta_012_2 = delta_01_0 * vec3_dot(a, simplex->p[0]) + delta_01_1 * vec3_dot(a, simplex->p[1]);
		if (delta_012_2 > 0.0f)
		{
			vec3_sub(a, simplex->p[2], simplex->p[0]);
			const f32 delta_02_0 = vec3_dot(a, simplex->p[2]);
			vec3_sub(a, simplex->p[0], simplex->p[2]);
			const f32 delta_02_2 = vec3_dot(a, simplex->p[0]);
			vec3_sub(a, simplex->p[0], simplex->p[1]);
			const f32 delta_012_1 = delta_02_0 * vec3_dot(a, simplex->p[0]) + delta_02_2 * vec3_dot(a, simplex->p[2]);
			if (delta_012_1 > 0.0f)
			{
				vec3_sub(a, simplex->p[2], simplex->p[1]);
				const f32 delta_12_1 = vec3_dot(a, simplex->p[2]);
				vec3_sub(a, simplex->p[1], simplex->p[2]);
				const f32 delta_12_2 = vec3_dot(a, simplex->p[1]);
				vec3_sub(a, simplex->p[1], simplex->p[0]);
				const f32 delta_012_0 = delta_12_1 * vec3_dot(a, simplex->p[1]) + delta_12_2 * vec3_dot(a, simplex->p[2]);
				if (delta_012_0 > 0.0f)
				{
					const f32 delta = delta_012_0 + delta_012_1 + delta_012_2;
					lambda[0] = delta_012_0 / delta;
					lambda[1] = delta_012_1 / delta;
					lambda[2] = delta_012_2 / delta;
					vec3_set(c_v,
						(lambda[0]*(simplex->p[0])[0] + lambda[1]*(simplex->p[1])[0] + lambda[2]*(simplex->p[2])[0]),
						(lambda[0]*(simplex->p[0])[1] + lambda[1]*(simplex->p[1])[1] + lambda[2]*(simplex->p[2])[1]),
						(lambda[0]*(simplex->p[0])[2] + lambda[1]*(simplex->p[1])[2] + lambda[2]*(simplex->p[2])[2]));
				}
				else
				{
					if (delta_12_2 > 0.0f)
					{
						if (delta_12_1 > 0.0f)
						{
							const f32 delta = delta_12_1 + delta_12_2;
							lambda[0] = delta_12_1 / delta;
							lambda[1] = delta_12_2 / delta;
							vec3_set(c_v,
							       	(lambda[0]*(simplex->p[1])[0] + lambda[1]*(simplex->p[2])[0]),
							       	(lambda[0]*(simplex->p[1])[1] + lambda[1]*(simplex->p[2])[1]),
							       	(lambda[0]*(simplex->p[1])[2] + lambda[1]*(simplex->p[2])[2]));
							simplex->type = 1;
							vec3_copy(simplex->p[0], simplex->p[1]);
							vec3_copy(simplex->p[1], simplex->p[2]);
							simplex->id[0] = simplex->id[1];
							simplex->dot[0] = simplex->dot[1];
						}
						else
						{
							simplex->type = 0;
							vec3_copy(c_v, simplex->p[2]);
							vec3_copy(simplex->p[0], simplex->p[2]);
							simplex->id[1] = UINT32_MAX;
							simplex->dot[1] = -1.0f;
						}


					}
					else
					{
						return 1;
					}
				}

			}
			else
			{
				if (delta_02_2 > 0.0f)
				{
					if (delta_02_0 > 0.0f)
					{
						const f32 delta = delta_02_0 + delta_02_2;
						lambda[0] = delta_02_0 / delta;
						lambda[1] = delta_02_2 / delta;
						vec3_set(c_v,
						       	(lambda[0]*(simplex->p[0])[0] + lambda[1]*(simplex->p[2])[0]),
						       	(lambda[0]*(simplex->p[0])[1] + lambda[1]*(simplex->p[2])[1]),
						       	(lambda[0]*(simplex->p[0])[2] + lambda[1]*(simplex->p[2])[2]));
						simplex->type = 1;
						vec3_copy(simplex->p[1], simplex->p[2]);
					}
					else
					{
						simplex->type = 0;
						vec3_copy(c_v, simplex->p[2]);
						vec3_copy(simplex->p[0], simplex->p[2]);
						simplex->id[1] = UINT32_MAX;
						simplex->dot[1] = -1.0f;
					}
				}
			}
		}
		else
		{
			return 1;
		}
	}
	else
	{
		vec3_sub(a, simplex->p[1], simplex->p[0]);
		const f32 delta_01_0 = vec3_dot(a, simplex->p[1]);
		vec3_sub(a, simplex->p[0], simplex->p[1]);
		const f32 delta_01_1 = vec3_dot(a, simplex->p[0]);
		vec3_sub(a, simplex->p[0], simplex->p[2]);
		const f32 delta_012_2 = delta_01_0 * vec3_dot(a, simplex->p[0]) + delta_01_1 * vec3_dot(a, simplex->p[1]);

		vec3_sub(a, simplex->p[2], simplex->p[0]);
		const f32 delta_02_0 = vec3_dot(a, simplex->p[2]);
		vec3_sub(a, simplex->p[0], simplex->p[2]);
		const f32 delta_02_2 = vec3_dot(a, simplex->p[0]);
		vec3_sub(a, simplex->p[0], simplex->p[1]);
		const f32 delta_012_1 = delta_02_0 * vec3_dot(a, simplex->p[0]) + delta_02_2 * vec3_dot(a, simplex->p[2]);

		vec3_sub(a, simplex->p[2], simplex->p[1]);
		const f32 delta_12_1 = vec3_dot(a, simplex->p[2]);
		vec3_sub(a, simplex->p[1], simplex->p[2]);
		const f32 delta_12_2 = vec3_dot(a, simplex->p[1]);
		vec3_sub(a, simplex->p[1], simplex->p[0]);
		const f32 delta_012_0 = delta_12_1 * vec3_dot(a, simplex->p[1]) + delta_12_2 * vec3_dot(a, simplex->p[2]);

		vec3_sub(a, simplex->p[0], simplex->p[3]);
		const f32 delta_0123_3 = delta_012_0 * vec3_dot(a, simplex->p[0]) + delta_012_1 * vec3_dot(a, simplex->p[1]) + delta_012_2 * vec3_dot(a, simplex->p[2]);

		if (delta_0123_3 > 0.0f)
		{
			vec3_sub(a, simplex->p[0], simplex->p[3]);
			const f32 delta_013_3 = delta_01_0 * vec3_dot(a, simplex->p[0]) + delta_01_1 * vec3_dot(a, simplex->p[1]);

			vec3_sub(a, simplex->p[3], simplex->p[0]);
			const f32 delta_03_0 = vec3_dot(a, simplex->p[3]);
			vec3_sub(a, simplex->p[0], simplex->p[3]);
			const f32 delta_03_3 = vec3_dot(a, simplex->p[0]);
			vec3_sub(a, simplex->p[0], simplex->p[1]);
			const f32 delta_013_1 = delta_03_0 * vec3_dot(a, simplex->p[0]) + delta_03_3 * vec3_dot(a, simplex->p[3]);

			vec3_sub(a, simplex->p[3], simplex->p[1]);
			const f32 delta_13_1 = vec3_dot(a, simplex->p[3]);
			vec3_sub(a, simplex->p[1], simplex->p[3]);
			const f32 delta_13_3 = vec3_dot(a, simplex->p[1]);
			vec3_sub(a, simplex->p[1], simplex->p[0]);
			const f32 delta_013_0 = delta_13_1 * vec3_dot(a, simplex->p[1]) + delta_13_3 * vec3_dot(a, simplex->p[3]);

			vec3_sub(a, simplex->p[0], simplex->p[2]);
			const f32 delta_0123_2 = delta_013_0 * vec3_dot(a, simplex->p[0]) + delta_013_1 * vec3_dot(a, simplex->p[1]) + delta_013_3 * vec3_dot(a, simplex->p[3]);

			if (delta_0123_2 > 0.0f)
			{
				vec3_sub(a, simplex->p[0], simplex->p[3]);
				const f32 delta_023_3 = delta_02_0 * vec3_dot(a, simplex->p[0]) + delta_02_2 * vec3_dot(a, simplex->p[2]);

				vec3_sub(a, simplex->p[0], simplex->p[2]);
				const f32 delta_023_2 = delta_03_0 * vec3_dot(a, simplex->p[0]) + delta_03_3 * vec3_dot(a, simplex->p[3]);

				vec3_sub(a, simplex->p[3], simplex->p[2]);
				const f32 delta_23_2 = vec3_dot(a, simplex->p[3]);
				vec3_sub(a, simplex->p[2], simplex->p[3]);
				const f32 delta_23_3 = vec3_dot(a, simplex->p[2]);
				vec3_sub(a, simplex->p[2], simplex->p[0]);
				const f32 delta_023_0 = delta_23_2 * vec3_dot(a, simplex->p[2]) + delta_23_3 * vec3_dot(a, simplex->p[3]);

				vec3_sub(a, simplex->p[0], simplex->p[1]);
				const f32 delta_0123_1 = delta_023_0 * vec3_dot(a, simplex->p[0]) + delta_023_2 * vec3_dot(a, simplex->p[2]) + delta_023_3 * vec3_dot(a, simplex->p[3]);

				if (delta_0123_1 > 0.0f)
				{
					vec3_sub(a, simplex->p[3], simplex->p[1]);
					const f32 delta_123_1 = delta_23_2 * vec3_dot(a, simplex->p[2]) + delta_23_3 * vec3_dot(a, simplex->p[3]);

					vec3_sub(a, simplex->p[3], simplex->p[2]);
					const f32 delta_123_2 = delta_13_1 * vec3_dot(a, simplex->p[1]) + delta_13_3 * vec3_dot(a, simplex->p[3]);

					vec3_sub(a, simplex->p[1], simplex->p[3]);
					const f32 delta_123_3 = delta_12_1 * vec3_dot(a, simplex->p[1]) + delta_12_2 * vec3_dot(a, simplex->p[2]);

					vec3_sub(a, simplex->p[3], simplex->p[0]);
					const f32 delta_0123_0 = delta_123_1 * vec3_dot(a, simplex->p[1]) + delta_123_2 * vec3_dot(a, simplex->p[2]) + delta_123_3 * vec3_dot(a, simplex->p[3]);

					if (delta_0123_0 > 0.0f)
					{
						/* intersection */
						const f32 delta = delta_0123_0 + delta_0123_1 + delta_0123_2 + delta_0123_3;
						lambda[0] = delta_0123_0 / delta;
						lambda[1] = delta_0123_1 / delta;
						lambda[2] = delta_0123_2 / delta;
						lambda[3] = delta_0123_3 / delta;
						vec3_set(c_v,
							(lambda[0]*(simplex->p[0])[0] + lambda[1]*(simplex->p[1])[0] + lambda[2]*(simplex->p[2])[0] + lambda[3]*(simplex->p[3])[0]),
							(lambda[0]*(simplex->p[0])[1] + lambda[1]*(simplex->p[1])[1] + lambda[2]*(simplex->p[2])[1] + lambda[3]*(simplex->p[3])[1]),
							(lambda[0]*(simplex->p[0])[2] + lambda[1]*(simplex->p[1])[2] + lambda[2]*(simplex->p[2])[2] + lambda[3]*(simplex->p[3])[2]));
					}
					else
					{
						/* check 123 subset */
						if (delta_123_3 > 0.0f)
						{
							if (delta_123_2 > 0.0f)
							{
								if (delta_123_1 > 0.0f)
								{
									const f32 delta = delta_123_1 + delta_123_2 + delta_123_3;
									lambda[0] = delta_123_1 / delta;
									lambda[1] = delta_123_2 / delta;
									lambda[2] = delta_123_3 / delta;
									vec3_set(c_v,
										(lambda[0]*(simplex->p[1])[0] + lambda[1]*(simplex->p[2])[0] + lambda[2]*(simplex->p[3])[0]),
										(lambda[0]*(simplex->p[1])[1] + lambda[1]*(simplex->p[2])[1] + lambda[2]*(simplex->p[3])[1]),
										(lambda[0]*(simplex->p[1])[2] + lambda[1]*(simplex->p[2])[2] + lambda[2]*(simplex->p[3])[2]));
									simplex->type = 2;
									vec3_copy(simplex->p[0], simplex->p[1]);		
									vec3_copy(simplex->p[1], simplex->p[2]);		
									vec3_copy(simplex->p[2], simplex->p[3]);		
									simplex->dot[0] = simplex->dot[1];
									simplex->dot[1] = simplex->dot[2];
									simplex->id[0] = simplex->id[1];
									simplex->id[1] = simplex->id[2];
								}
								else
								{
									/* check 23 */
									if (delta_23_3 > 0.0f)
									{
										if (delta_23_2 > 0.0f)
										{
											const f32 delta = delta_23_2 + delta_23_3;
											lambda[0] = delta_23_2 / delta;
											lambda[1] = delta_23_3 / delta;
											vec3_set(c_v,
												(lambda[0]*(simplex->p[2])[0] + lambda[1]*(simplex->p[3])[0]),
												(lambda[0]*(simplex->p[2])[1] + lambda[1]*(simplex->p[3])[1]),
												(lambda[0]*(simplex->p[2])[2] + lambda[1]*(simplex->p[3])[2]));
											simplex->type = 1;
											vec3_copy(simplex->p[0], simplex->p[2]);		
											vec3_copy(simplex->p[1], simplex->p[3]);		
											simplex->dot[0] = simplex->dot[2];
											simplex->dot[2] = -1.0f;
											simplex->id[0] = simplex->id[2];
											simplex->id[2] = UINT32_MAX;
										}
										else
										{
											vec3_copy(c_v, simplex->p[3]);
											simplex->type = 0;
											vec3_copy(simplex->p[0], simplex->p[3]);
											simplex->dot[1] = -1.0f;
											simplex->dot[2] = -1.0f;
											simplex->id[1] = UINT32_MAX;
											simplex->id[2] = UINT32_MAX;
										}
									}
									else
									{
										return 1;
									}
								}
							}
							else
							{
								/* check 13 subset */
								if (delta_13_3 > 0.0f)
								{
									if (delta_13_1 > 0.0f)
									{
										const f32 delta = delta_13_1 + delta_13_3;
										lambda[0] = delta_13_1 / delta;
										lambda[1] = delta_13_3 / delta;
										vec3_set(c_v,
											(lambda[0]*(simplex->p[1])[0] + lambda[1]*(simplex->p[3])[0]),
											(lambda[0]*(simplex->p[1])[1] + lambda[1]*(simplex->p[3])[1]),
											(lambda[0]*(simplex->p[1])[2] + lambda[1]*(simplex->p[3])[2]));
										simplex->type = 1;
										vec3_copy(simplex->p[0], simplex->p[1]);
										vec3_copy(simplex->p[1], simplex->p[3]);		
										simplex->dot[0] = simplex->dot[1];
										simplex->dot[2] = -1.0f;
										simplex->id[0] = simplex->id[1];
										simplex->id[2] = UINT32_MAX;
									}
									else
									{
										vec3_copy(c_v, simplex->p[3]);
										simplex->type = 0;
										vec3_copy(simplex->p[0], simplex->p[3]);
										simplex->dot[1] = -1.0f;
										simplex->dot[2] = -1.0f;
										simplex->id[1] = UINT32_MAX;
										simplex->id[2] = UINT32_MAX;
									}
								}
								else
								{
									return 1;
								}
							}	
						}
						else
						{
							return 1;
						}
					}
				}
				else
				{
					/* check 023 subset */
					if (delta_023_3 > 0.0f)
					{
						if (delta_023_2 > 0.0f)
						{
							if (delta_023_0 > 0.0f)
							{
								const f32 delta = delta_023_0 + delta_023_2 + delta_023_3;
								lambda[0] = delta_023_0 / delta;
								lambda[1] = delta_023_2 / delta;
								lambda[2] = delta_023_3 / delta;
								vec3_set(c_v,
									(lambda[0]*(simplex->p[0])[0] + lambda[1]*(simplex->p[2])[0] + lambda[2]*(simplex->p[3])[0]),
									(lambda[0]*(simplex->p[0])[1] + lambda[1]*(simplex->p[2])[1] + lambda[2]*(simplex->p[3])[1]),
									(lambda[0]*(simplex->p[0])[2] + lambda[1]*(simplex->p[2])[2] + lambda[2]*(simplex->p[3])[2]));
								simplex->type = 2;
								vec3_copy(simplex->p[1], simplex->p[2]);		
								vec3_copy(simplex->p[2], simplex->p[3]);		
								simplex->dot[1] = simplex->dot[2];
								simplex->id[1] = simplex->id[2];
							}
							else
							{
								/* check 23 subset */
								if (delta_23_3 > 0.0f)
								{
									if (delta_23_2 > 0.0f)
									{
										const f32 delta = delta_23_2 + delta_23_3;
										lambda[0] = delta_23_2 / delta;
										lambda[1] = delta_23_3 / delta;
										vec3_set(c_v,
											(lambda[0]*(simplex->p[2])[0] + lambda[1]*(simplex->p[3])[0]),
											(lambda[0]*(simplex->p[2])[1] + lambda[1]*(simplex->p[3])[1]),
											(lambda[0]*(simplex->p[2])[2] + lambda[1]*(simplex->p[3])[2]));
										simplex->type = 1;
										vec3_copy(simplex->p[0], simplex->p[2]);
										vec3_copy(simplex->p[1], simplex->p[3]);
										simplex->dot[0] = simplex->dot[2];
										simplex->dot[2] = -1.0f;
										simplex->id[0] = simplex->id[2];
										simplex->id[2] = UINT32_MAX;
									}
									else
									{
										vec3_copy(c_v, simplex->p[3]);
										simplex->type = 0;
										vec3_copy(simplex->p[0], simplex->p[3]);
										simplex->dot[1] = -1.0f;
										simplex->dot[2] = -1.0f;
										simplex->id[1] = UINT32_MAX;
										simplex->id[2] = UINT32_MAX;
									}
								}
								else
								{
									return 1;
								}
							}
						}
						else
						{
							/* check 03 subset */
							if (delta_03_3 > 0.0f)
							{
								if (delta_03_0 > 0.0f)
								{
									const f32 delta = delta_03_0 + delta_03_3;
									lambda[0] = delta_03_0 / delta;
									lambda[1] = delta_03_3 / delta;
									vec3_set(c_v,
										(lambda[0]*(simplex->p[0])[0] + lambda[1]*(simplex->p[3])[0]),
										(lambda[0]*(simplex->p[0])[1] + lambda[1]*(simplex->p[3])[1]),
										(lambda[0]*(simplex->p[0])[2] + lambda[1]*(simplex->p[3])[2]));
									simplex->type = 1;
									vec3_copy(simplex->p[1], simplex->p[3]);
									simplex->dot[2] = -1.0f;
									simplex->id[2] = UINT32_MAX;
								}
								else
								{
									vec3_copy(c_v, simplex->p[3]);
									simplex->type = 0;
									vec3_copy(simplex->p[0], simplex->p[3]);
									simplex->dot[1] = -1.0f;
									simplex->dot[2] = -1.0f;
									simplex->id[1] = UINT32_MAX;
									simplex->id[2] = UINT32_MAX;
								}
							}
							else
							{
								return 1;
							}
						}
					}
					else
					{
						return 1;
					}
				}
			}
			else
			{
				/* check 013 subset */
				if (delta_013_3 > 0.0f)
				{
					if (delta_013_1 > 0.0f)
					{
						if (delta_013_0 > 0.0f)
						{
							const f32 delta = delta_013_0 + delta_013_1 + delta_013_3;
							lambda[0] = delta_013_0 / delta;
							lambda[1] = delta_013_1 / delta;
							lambda[2] = delta_013_3 / delta;
							vec3_set(c_v,
								(lambda[0]*(simplex->p[0])[0] + lambda[1]*(simplex->p[1])[0] + lambda[2]*(simplex->p[3])[0]),
								(lambda[0]*(simplex->p[0])[1] + lambda[1]*(simplex->p[1])[1] + lambda[2]*(simplex->p[3])[1]),
								(lambda[0]*(simplex->p[0])[2] + lambda[1]*(simplex->p[1])[2] + lambda[2]*(simplex->p[3])[2]));
							simplex->type = 2;
							vec3_copy(simplex->p[2], simplex->p[3]);
						}
						else
						{
							/* check 13 subset */
							if (delta_13_3 > 0.0f)
							{
								if (delta_13_1 > 0.0f)
								{
									const f32 delta = delta_13_1 + delta_13_3;
									lambda[0] = delta_13_1 / delta;
									lambda[1] = delta_13_3 / delta;
									vec3_set(c_v,
										(lambda[0]*(simplex->p[1])[0] + lambda[1]*(simplex->p[3])[0]),
										(lambda[0]*(simplex->p[1])[1] + lambda[1]*(simplex->p[3])[1]),
										(lambda[0]*(simplex->p[1])[2] + lambda[1]*(simplex->p[3])[2]));
									simplex->type = 1;
									vec3_copy(simplex->p[0], simplex->p[1]);
									vec3_copy(simplex->p[1], simplex->p[3]);
									simplex->dot[2] = -1.0f;
									simplex->id[2] = UINT32_MAX;
								}
								else
								{
									vec3_copy(c_v, simplex->p[3]);
									simplex->type = 0;
									vec3_copy(simplex->p[0], simplex->p[3]);
									simplex->dot[1] = -1.0f;
									simplex->dot[2] = -1.0f;
									simplex->id[1] = UINT32_MAX;
									simplex->id[2] = UINT32_MAX;
								}
							}
							else
							{
								return 1;
							}
						}	
					}
					else
					{
						/* check 03 subset */
						if (delta_03_3 > 0.0f)
						{
							if (delta_03_0 > 0.0f)
							{
								const f32 delta = delta_03_0 + delta_03_3;
								lambda[0] = delta_03_0 / delta;
								lambda[1] = delta_03_3 / delta;
								vec3_set(c_v,
									(lambda[0]*(simplex->p[0])[0] + lambda[1]*(simplex->p[3])[0]),
									(lambda[0]*(simplex->p[0])[1] + lambda[1]*(simplex->p[3])[1]),
									(lambda[0]*(simplex->p[0])[2] + lambda[1]*(simplex->p[3])[2]));
								simplex->type = 1;
								vec3_copy(simplex->p[1], simplex->p[3]);
								simplex->dot[2] = -1.0f;
								simplex->id[2] = UINT32_MAX;
							}
							else
							{
								vec3_copy(c_v, simplex->p[3]);
								simplex->type = 0;
								vec3_copy(simplex->p[0], simplex->p[3]);
								simplex->dot[1] = -1.0f;
								simplex->dot[2] = -1.0f;
								simplex->id[1] = UINT32_MAX;
								simplex->id[2] = UINT32_MAX;
							}
						}
						else
						{
							return 1;
						}
					}
				}
				else
				{
					return 1;
				}
			}
		}
		else
		{
			return 1;
		}
	}

	return 0;
}

u32 GJK_test(const vec3 pos_1, vec3ptr vs_1, const u32 n_1, const vec3 pos_2, vec3ptr vs_2, const u32 n_2, const f32 abs_tol, const f32 tol)
{
	struct gjk_simplex simplex = GJK_internal_simplex_init();
	/* c_v - closest point in each iteration, dir = -c_v */
	vec3 dir, c_v;
	vec4 lambda;
	u64 support_id;
	f32 ma; /* max dot product of current simplex */

	/* arbitrary starting search direction */
	vec3_set(c_v, 1.0f, 0.0f, 0.0f);

	do
	{
		simplex.type += 1;
		vec3_scale(dir, c_v, -1.0f);
		support_id = convex_minkowski_difference_world_support(simplex.p[simplex.type], dir, pos_1, vs_1, n_1, pos_2, vs_2, n_2);
		if (vec3_dot(simplex.p[simplex.type], dir) < 0.0f)
		{
			return 0;
		}

		/* Degenerate Case: Check if new support point is already in simplex */
		if (simplex.id[0] == support_id || simplex.id[1] == support_id || simplex.id[2] == support_id || simplex.id[3] == support_id)
		{
			return 0;
		}

		/* find closest point v to origin using naive Johnson's algorithm, update simplex data 
		 * Degenerate Case: due to numerical issues, determinat signs may flip, which may result
		 * either in wrong sub-simplex being chosen, or no valid simplex at all. In that case c_v
		 * stays the same, and we terminate the algorithm. [See page 142].
		 */
		if (GJK_internal_johnsons_algorithm(&simplex, c_v, lambda))
		{
			return 0;
		}

		simplex.id[simplex.type] = support_id;
		simplex.dot[simplex.type] = vec3_dot(simplex.p[simplex.type], simplex.p[simplex.type]);

		/* 
		 * If the simplex is of type 3, or a tetrahedron, we have encapsulated 0, or, if v is sufficiently
		 * close to the origin, within a margin of error, return an intersection.
		 */
		if (simplex.type == 3)
		{
			return 1;
		}
		else
		{
			ma = simplex.dot[0];
			ma = f32_max(ma, simplex.dot[1]);
			ma = f32_max(ma, simplex.dot[2]);
			ma = f32_max(ma, simplex.dot[3]);

			/* For error bound discussion, see sections 4.3.5, 4.3.6 */
			if (vec3_dot(c_v, c_v) <= tol * ma)
			{
				return 1;
			}
		}
	} while (1);
}

static void GJK_internal_closest_points_on_bodies(vec3 c_1, vec3 c_2, vec3ptr vs_1, const vec3 pos_1, vec3ptr vs_2, const vec3 pos_2, const u64 simplex_id[4], const vec4 lambda, const u32 simplex_type)
{
	vec3_copy(c_1, pos_1);
	vec3_copy(c_2, pos_2);
	if (simplex_type == 0)
	{
		vec3_translate(c_1, vs_1[simplex_id[0] >> 32]);
		vec3_translate(c_2, vs_2[simplex_id[0] & 0xffffffff]);
	}
	else
	{
		vec3 v_1, v_2;
		for (u32 i = 0; i <= simplex_type; ++i)
		{
			vec3_scale(v_1, vs_1[simplex_id[i] >> 32], lambda[i]);
			vec3_scale(v_2, vs_2[simplex_id[i] & 0xffffffff], lambda[i]);
			vec3_translate(c_1, v_1);
			vec3_translate(c_2, v_2);
		}	
	}
}

static f32 GJK_distance_internal(struct gjk_simplex *simplex, vec3 c_1, vec3 c_2, const vec3 pos_1, vec3ptr vs_1, const u32 n_1, const vec3 pos_2, vec3ptr vs_2, const u32 n_2, const f32 rel_tol, const f32 abs_tol)
{ 
	*simplex = GJK_internal_simplex_init();
	/* c_v - closest point in each iteration, dir = -c_v */
	vec3 dir, c_v;
	/* lambda = coefficients for simplex point mix */ 
	vec4 lambda;
	u64 support_id;
	f32 ma; /* max dot product of current simplex */
	f32 c_v_distance_sq = F32_INFINITY; /* closest point on simplex distance to origin */
	const f32 rel = rel_tol * rel_tol;

	/* arbitrary starting search direction */
	vec3_set(c_v, 1.0f, 0.0f, 0.0f);
	u64 old_support = U64_MAX;

	do
	{
		simplex->type += 1;
		vec3_scale(dir, c_v, -1.0f);
		support_id = convex_minkowski_difference_world_support(simplex->p[simplex->type], dir, pos_1, vs_1, n_1, pos_2, vs_2, n_2);
		if (c_v_distance_sq - vec3_dot(simplex->p[simplex->type], c_v) <= rel * c_v_distance_sq + abs_tol
				|| simplex->id[0] == support_id || simplex->id[1] == support_id 
				|| simplex->id[2] == support_id || simplex->id[3] == support_id)
		{
			assert(simplex->id != 0);
			assert(c_v_distance_sq != F32_INFINITY);
			simplex->type -= 1;
			GJK_internal_closest_points_on_bodies(c_1, c_2, vs_1, pos_1, vs_2, pos_2, simplex->id, lambda, simplex->type);
			return f32_sqrt(c_v_distance_sq);
		}

		/* find closest point v to origin using naive Johnson's algorithm, update simplex data 
		 * Degenerate Case: due to numerical issues, determinant signs may flip, which may result
		 * either in wrong sub-simplex being chosen, or no valid simplex at all. In that case c_v
		 * stays the same, and we terminate the algorithm. [See page 142].
		 */
		if (GJK_internal_johnsons_algorithm(simplex, c_v, lambda))
		{
			assert(c_v_distance_sq != F32_INFINITY);
			simplex->type -= 1;
			GJK_internal_closest_points_on_bodies(c_1, c_2, vs_1, pos_1, vs_2, pos_2, simplex->id, lambda, simplex->type);
			return f32_sqrt(c_v_distance_sq);
		}

		simplex->id[simplex->type] = support_id;
		simplex->dot[simplex->type] = vec3_dot(simplex->p[simplex->type], simplex->p[simplex->type]);

		/* 
		 * If the simplex is of type 3, or a tetrahedron, we have encapsulated 0, or, if v is sufficiently
		 * close to the origin, within a margin of error, return an intersection.
		 */
		if (simplex->type == 3)
		{
			return 0.0f;
		}
		else
		{
			ma = simplex->dot[0];
			ma = f32_max(ma, simplex->dot[1]);
			ma = f32_max(ma, simplex->dot[2]);
			ma = f32_max(ma, simplex->dot[3]);

			/* For error bound discussion, see sections 4.3.5, 4.3.6 */
			c_v_distance_sq = vec3_dot(c_v, c_v);
			if (c_v_distance_sq <= abs_tol * ma)
			{
				return 0.0f;
			}
		}
	} while (1);

	return 0.0f;
}

f32 GJK_distance(vec3 c_1, vec3 c_2, const vec3 pos_1, vec3ptr vs_1, const u32 n_1, const vec3 pos_2, vec3ptr vs_2, const u32 n_2, const f32 rel_tol, const f32 abs_tol)
{
	struct gjk_simplex simplex;
	return GJK_distance_internal(&simplex, c_1, c_2, pos_1, vs_1, n_1, pos_2, vs_2, n_2, rel_tol, abs_tol);
}

f32 point_plane_signed_distance(const vec3 point, const struct plane *plane)
{
	const vec3 relation_to_plane_point =
       	{ 
		point[0] - plane->normal[0] * plane->signed_distance,
		point[1] - plane->normal[1] * plane->signed_distance,
		point[2] - plane->normal[2] * plane->signed_distance,
	};
	
	return vec3_dot(relation_to_plane_point, plane->normal);
}

f32 point_plane_distance(const vec3 point, const struct plane *plane)
{

	return f32_abs(point_plane_signed_distance(point, plane));
}

void point_plane_closest_point(vec3 closest_point, const vec3 point, const struct plane *plane)
{
	const f32 distance = point_plane_signed_distance(point, plane);	
	vec3_scale(closest_point, plane->normal, -distance);
	vec3_translate(closest_point, point);
}

f32 point_sphere_distance(const vec3 point, const struct sphere *sph)
{
	vec3 tmp;
	vec3_sub(tmp, point, sph->center);
	const f32 distance = vec3_length(tmp) - sph->radius;
	return distance * (1 - f32_sign_bit(distance));
}

void point_sphere_closest_point(vec3 closest_point, const vec3 point, const struct sphere *sph)
{
	vec3_sub(closest_point, point, sph->center);
	f32 distance = vec3_length(closest_point) - sph->radius;
	distance *= (1 - f32_sign_bit(distance));

	/* Can be removed if we can multiply 0.0f * FLT_INF... ? */
	if (distance == 0.0f)
	{
		vec3_copy(closest_point, point);
	}
	else
	{
		vec3_mul_constant(closest_point, sph->radius / vec3_length(closest_point));
	}
	vec3_translate(closest_point, sph->center);
}

i32 ray_sphere_intersection(vec3 intersection, const vec3 ray_origin, const vec3 ray_direction, const struct sphere * sph)
{
	vec3 m;
	vec3_sub(m, ray_origin, sph->center);
	const f32 b = vec3_dot(m, ray_direction);
	const f32 c = vec3_dot(m,m) - sph->radius * sph->radius;
	const f32 discr = b*b - c;
	if (discr < 0) { return 0; }

	const f32 t = -b - f32_sign(c)*f32_sqrt(discr); 
	vec3_scale(intersection, ray_direction, t);
	vec3_translate(intersection, ray_origin);
	return 1 - f32_sign_bit(t);
}

f32 point_AABB_distance(const vec3 point, const struct AABB *aabb)
{
	f32 x,y,z;

	x = f32_abs(point[0] - aabb->center[0]);
	y = f32_abs(point[1] - aabb->center[1]);
	z = f32_abs(point[2] - aabb->center[2]);

	x -= aabb->hw[0]; 
	y -= aabb->hw[1];
	z -= aabb->hw[2];

	x *= 1 - f32_sign_bit(x);  
	y *= 1 - f32_sign_bit(y); 
	z *= 1 - f32_sign_bit(z); 

	return f32_sqrt(x*x + y*y + z*z);
}

f32 point_OBB_distance(const vec3 point, const struct OBB *obb)
{
	vec3 y_axis;
	vec3_cross(y_axis, obb->z_axis, obb->x_axis);
	mat3 transform;
	mat3_set_rows(transform, obb->x_axis, y_axis, obb->z_axis);

	vec3 tmp, point_obb_space;
	vec3_sub(tmp, point, obb->center);
	mat3_vec_mul(point_obb_space, transform, tmp);

	f32 x,y,z;

	x = f32_abs(point_obb_space[0]);
	y = f32_abs(point_obb_space[1]);
	z = f32_abs(point_obb_space[2]);

	x -= obb->hw[0]; 
	y -= obb->hw[1];
	z -= obb->hw[2];

	x *= (1 - f32_sign_bit(x));  
	y *= (1 - f32_sign_bit(y)); 
	z *= (1 - f32_sign_bit(z)); 

	return f32_sqrt(x*x + y*y + z*z);
}

void point_AABB_closest_point(vec3 closest_point, const vec3 point, const struct AABB *aabb)
{
	 f32 x,y,z;

	x = point[0] - aabb->center[0];
	y = point[1] - aabb->center[1];
	z = point[2] - aabb->center[2];

	const vec3 sign = { f32_sign(x), f32_sign(y), f32_sign(z) };

	x = f32_abs(x);
	y = f32_abs(y);
	z = f32_abs(z);

	x -= aabb->hw[0]; 
	y -= aabb->hw[1];
	z -= aabb->hw[2];

	vec3_set(closest_point
	 ,point[0]*f32_sign_bit(x) + (1-f32_sign_bit(x)) * (aabb->center[0] + sign[0] * aabb->hw[0])  
	 ,point[1]*f32_sign_bit(y) + (1-f32_sign_bit(y)) * (aabb->center[1] + sign[1] * aabb->hw[1]) 
	 ,point[2]*f32_sign_bit(z) + (1-f32_sign_bit(z)) * (aabb->center[2] + sign[2] * aabb->hw[2]));
}

void point_OBB_closest_point(vec3 closest_point, const vec3 point, const struct OBB *obb)
{
	vec3 y_axis;
	vec3_cross(y_axis, obb->z_axis, obb->x_axis);
	mat3 transform;
	mat3_set_rows(transform, obb->x_axis, y_axis, obb->z_axis);

	vec3 tmp, point_obb_space;
	vec3_sub(tmp, point, obb->center);
	mat3_vec_mul(point_obb_space, transform, tmp);

	f32 x,y,z;

	x = point_obb_space[0];
	y = point_obb_space[1];
	z = point_obb_space[2];

	const vec3 sign = { f32_sign(x), f32_sign(y), f32_sign(z) };

	x = f32_abs(x);
	y = f32_abs(y);
	z = f32_abs(z);

	x -= obb->hw[0]; 
	y -= obb->hw[1];
	z -= obb->hw[2];

	vec3_set(tmp
	 ,point_obb_space[0]*f32_sign_bit(x) + (1-f32_sign_bit(x)) * sign[0] * obb->hw[0] 
	 ,point_obb_space[1]*f32_sign_bit(y) + (1-f32_sign_bit(y)) * sign[1] * obb->hw[1] 
	 ,point_obb_space[2]*f32_sign_bit(z) + (1-f32_sign_bit(z)) * sign[2] * obb->hw[2]);
	
	mat3_set_columns(transform, obb->x_axis, y_axis, obb->z_axis);
	mat3_vec_mul(closest_point, transform, tmp);
	vec3_translate(closest_point, obb->center);
}

i32 ray_OBB_intersection(vec3 intersection, const vec3 ray_origin, const vec3 ray_direction, const struct OBB *obb)
{
	vec3 y_axis;
	vec3_cross(y_axis, obb->z_axis, obb->x_axis);
	mat3 transform;
	mat3_set_rows(transform, obb->x_axis, y_axis, obb->z_axis);

	vec3 tmp, p, ray_direction_obb;
	vec3_sub(tmp, ray_origin, obb->center);
	mat3_vec_mul(p, transform, tmp);
	mat3_vec_mul(ray_direction_obb, transform, ray_direction);

	struct ray ray = ray_construct(p, ray_direction_obb);
	mat3_set_columns(transform, obb->x_axis, y_axis, obb->z_axis);

	f32 x,y,z;

	x = p[0];
	y = p[1];
	z = p[2];

	const vec3 sign = { f32_sign(x), f32_sign(y), f32_sign(z) };

	x = f32_abs(x);
	y = f32_abs(y);
	z = f32_abs(z);

	x -= obb->hw[0]; 
	y -= obb->hw[1];
	z -= obb->hw[2];

	const u32 sx = f32_sign_bit(x);
	const u32 sy = f32_sign_bit(y);
	const u32 sz = f32_sign_bit(z);

	/* Check planes towards */
	if (sx + sy + sz == 3)
	{
		x = ray_direction_obb[0];
		struct plane plane;
		vec3_set(plane.normal, f32_sign(x), 0.0f, 0.0f);
		plane.signed_distance = obb->hw[0];
		if (ray_direction_obb[0] != 0.0f
			         && plane_raycast(tmp, &plane, &ray)
				 && (tmp[1] >= -obb->hw[1] && tmp[1] <= obb->hw[1])
				 && (tmp[2] >= -obb->hw[2] && tmp[2] <= obb->hw[2])
				)
		{
			mat3_vec_mul(intersection, transform, tmp);
			vec3_translate(intersection, obb->center);
			return 1;	
		}

		y = ray_direction_obb[1];
		vec3_set(plane.normal, 0.0f, f32_sign(y), 0.0f);
		plane.signed_distance = obb->hw[1];
		if (ray_direction_obb[1] != 0.0f
			         && plane_raycast(tmp, &plane, &ray)
				 && (tmp[0] >= -obb->hw[0] && tmp[0] <= obb->hw[0])
				 && (tmp[2] >= -obb->hw[2] && tmp[2] <= obb->hw[2])
				)
		{
			mat3_vec_mul(intersection, transform, tmp);
			vec3_translate(intersection, obb->center);
			return 1;	
		}

		z = ray_direction_obb[2];
		vec3_set(plane.normal, 0.0f, 0.0f, f32_sign(z));
		plane.signed_distance = obb->hw[2];
		if (ray_direction_obb[2] != 0.0f
			         && plane_raycast(tmp, &plane, &ray)
				 && (tmp[0] >= -obb->hw[0] && tmp[0] <= obb->hw[0])
				 && (tmp[1] >= -obb->hw[1] && tmp[1] <= obb->hw[1])
				)
		{
			mat3_vec_mul(intersection, transform, tmp);
			vec3_translate(intersection, obb->center);
			return 1;	
		}

	}
	/* check planes against */
	else
	{
		struct plane plane;
		vec3_set(plane.normal, sign[0], 0.0f, 0.0f);
		plane.signed_distance = obb->hw[0];
		if (sx == 0 
			         && plane_raycast(tmp, &plane, &ray)
				 && (tmp[1] >= -obb->hw[1] && tmp[1] <= obb->hw[1])
				 && (tmp[2] >= -obb->hw[2] && tmp[2] <= obb->hw[2])
				)
		{
			mat3_vec_mul(intersection, transform, tmp);
			vec3_translate(intersection, obb->center);
			return 1;	
		}

		vec3_set(plane.normal, 0.0f, sign[1], 0.0f);
		plane.signed_distance = obb->hw[1];
		if (sy == 0 
			         && plane_raycast(tmp, &plane, &ray)
				 && (tmp[0] >= -obb->hw[0] && tmp[0] <= obb->hw[0])
				 && (tmp[2] >= -obb->hw[2] && tmp[2] <= obb->hw[2])
				)
		{
			mat3_vec_mul(intersection, transform, tmp);
			vec3_translate(intersection, obb->center);
			return 1;	
		}

		vec3_set(plane.normal, 0.0f, 0.0f, sign[2]);
		plane.signed_distance = obb->hw[2];
		if (sz == 0 
			         && plane_raycast(tmp, &plane, &ray)
				 && (tmp[0] >= -obb->hw[0] && tmp[0] <= obb->hw[0])
				 && (tmp[1] >= -obb->hw[1] && tmp[1] <= obb->hw[1])
				)
		{
			mat3_vec_mul(intersection, transform, tmp);
			vec3_translate(intersection, obb->center);
			return 1;	
		}
	}

	return 0;
}

f32 point_cylinder_distance(const vec3 point, const struct cylinder *cyl)
{
	vec3 tmp;
	vec3_sub(tmp, point, cyl->center);
	
	f32 r,y;
	
	r = f32_sqrt(tmp[0]*tmp[0] + tmp[2]*tmp[2]) - cyl->radius;

	y = tmp[1];
	y = f32_abs(y);
	y -= cyl->half_height;

	return f32_sqrt(y*y*(1-f32_sign_bit(y)) + r*r*(1-f32_sign_bit(r)));
}

void point_cylinder_closest_point(vec3 closest_point, const vec3 point, const struct cylinder *cyl)
{
	vec3 tmp;
	vec3_sub(tmp, point, cyl->center);
	
	f32 r,y;
	
	const f32 xz_dist = f32_sqrt(tmp[0]*tmp[0] + tmp[2]*tmp[2]);
	r = xz_dist - cyl->radius;

	y = tmp[1];
	const f32 y_sign = f32_sign(y);
	y = f32_abs(y);
	y -= cyl->half_height;

	closest_point[0] = tmp[0] * f32_sign_bit(r) + (1 - f32_sign_bit(r)) * cyl->radius * tmp[0] / xz_dist;
	closest_point[1] = tmp[1] * f32_sign_bit(y) + (1 - f32_sign_bit(y)) * cyl->half_height * y_sign;
	closest_point[2] = tmp[2] * f32_sign_bit(r) + (1 - f32_sign_bit(r)) * cyl->radius * tmp[2] / xz_dist;
	vec3_translate(closest_point, cyl->center);
}

i32 ray_cylinder_intersection(vec3 intersection, const vec3 ray_origin, const vec3 ray_direction, const struct cylinder *cyl)
{
	vec3_sub(intersection, ray_origin, cyl->center);

	f32 y, r;
	
	r = intersection[0]*intersection[0] + intersection[2]*intersection[2] - cyl->radius*cyl->radius;

	y = intersection[1];
	const f32 y_sign = f32_sign(y);
	y = f32_abs(y); 
	y -= cyl->half_height;

	switch ((f32_sign_bit(y) << 1) + f32_sign_bit(r))
	{
		/* above/below AND infront/behind cylinder  */
		case 0:
		{
			/* ray turned away from surface plane */
			const f32 d = y_sign * ray_direction[1];
			if (d >= 0.0f) { return 0; }
			
			vec3 tmp = { intersection[0], intersection[1], intersection[2] };
			vec3_translate_scaled(tmp, ray_direction, y / (-d));
			f32 r2 = tmp[0]*tmp[0] + tmp[2]*tmp[2] - cyl->radius*cyl->radius;
			if (f32_sign_bit(r2)) 
			{ 
				vec3_add(intersection, tmp, cyl->center);
				return 1;
		       	}

			if (ray_direction[0] == 0.0f && ray_direction[2] == 0.0f) { return 0; }
			const f32 r_dist = f32_sqrt(ray_direction[0]*ray_direction[0] + ray_direction[2]*ray_direction[2]);
			const f32 n_d = (intersection[0]*ray_direction[0] + intersection[2]*ray_direction[2]) / r_dist;
			//const f32 n_n = intersection[0]*intersection[0] + intersection[2]*intersection[2];
			const f32 discr = n_d*n_d - r;
			if (discr < 0.0f) { return 0; }

			const f32 t = -n_d - f32_sqrt(discr); 
			vec3_translate_scaled(intersection, ray_direction, t / r_dist);
			y = f32_abs(intersection[1]);
			y -= cyl->half_height;
			vec3_translate(intersection, cyl->center);
			return f32_sign_bit(y);
		}
		/* directly above or below cylinder  */
		case 1:
		{
			/* ray turned away from surface plane */
			const f32 d = y_sign * ray_direction[1];
			if (d >= 0.0f) { return 0; }

			vec3_translate_scaled(intersection, ray_direction, y / (-d));
			r = intersection[0]*intersection[0] + intersection[2]*intersection[2] - cyl->radius*cyl->radius;
			vec3_translate(intersection, cyl->center);
			return f32_sign_bit(r);
		}
		/* infront of cylinder (same height as the cylinder) */
		case 2:
		{
			if (ray_direction[0] == 0.0f && ray_direction[2] == 0.0f) { return 0; }
			const f32 r_dist = f32_sqrt(ray_direction[0]*ray_direction[0] + ray_direction[2]*ray_direction[2]);
			const f32 n_d = (intersection[0]*ray_direction[0] + intersection[2]*ray_direction[2]) / r_dist;
			const f32 discr = n_d*n_d - r;
			if (discr < 0.0f) { return 0; }

			const f32 t = -n_d - f32_sqrt(discr); 
			vec3_translate_scaled(intersection, ray_direction, t / r_dist);
			y = f32_abs(intersection[1]);
			y -= cyl->half_height;
			vec3_translate(intersection, cyl->center);
			return f32_sign_bit(y);
		}
		/* inside cylinder */
		case 3:
		{
			f32 t = F32_INFINITY;

			if (ray_origin[1] != 0.0f)
			{
				t = ray_direction[1];
				t = ((1 - f32_sign_bit(t)) * (cyl->half_height - intersection[1]) + f32_sign_bit(t) * (cyl->half_height + intersection[1])) / (f32_sign(t) * t);
			}

			if (ray_direction[0] != 0.0f || ray_direction[2] != 0.0f) 
			{
				const f32 r_dist = f32_sqrt(ray_direction[0]*ray_direction[0] + ray_direction[2]*ray_direction[2]);
				const f32 n_d = (intersection[0]*ray_direction[0] + intersection[2]*ray_direction[2]) / r_dist;
				const f32 discr = n_d*n_d - r;
				assert(discr >= 0.0f);
				t = f32_min(t, (-n_d + f32_sqrt(discr)) / r_dist);
			}
	
			vec3_translate_scaled(intersection, ray_direction, t);
			vec3_translate(intersection, cyl->center);
			return 1;
		}
		default:
		{
			assert(false && "should not happen");
			return 0;
		}
	}
}


f32 AABB_distance(const struct AABB *a, const struct AABB *b)
{
	f32 t, m, dist = 0.0f;

	t = b->center[0] - b->hw[0] - (a->center[0] + a->hw[0]);
	m = a->center[0] - a->hw[0] - (b->center[0] + b->hw[0]);
	if (t > 0.0f || m > 0.0f) { t = f32_max(t,m); dist += t*t; }

	t = b->center[1] - b->hw[1] - (a->center[1] + a->hw[1]);
	m = a->center[1] - a->hw[1] - (b->center[1] + b->hw[1]);
	if (t > 0.0f || m > 0.0f) { t = f32_max(t,m); dist += t*t; }

	t = b->center[2] - b->hw[2] - (a->center[2] + a->hw[2]);
	m = a->center[2] - a->hw[2] - (b->center[2] + b->hw[2]);
	if (t > 0.0f || m > 0.0f) { t = f32_max(t,m); dist += t*t; }

	return dist;
}

i32 AABB_intersection(struct AABB *dst, const struct AABB *a, const struct AABB *b)
{
	vec3 interpolation;
	for (i32 i = 0; i < 3; ++i)
	{
		const f32 t = b->center[i] - b->hw[i] - (a->center[i] + a->hw[i]);
		const f32 m = a->center[i] - a->hw[i] - (b->center[i] + b->hw[i]);
		if (t > 0.0f || m > 0.0f) 
		{ 
			return 0;
       		}
		else
		{
			const f32 dist = f32_abs(a->center[i] - b->center[i]);
			if (dist + a->hw[i] <= b->hw[i]) 
			{ 
				dst->hw[i] = a->hw[i];
				interpolation[i] = 1.0f; 
			}
			else if (dist + b->hw[i] <= a->hw[i])
		       	{
			       	dst->hw[i] = b->hw[i];
				interpolation[i] = 0.0f;
			}
			else  
			{
				dst->hw[i] = (a->hw[i] + b->hw[i] - dist) / 2.0f;
				interpolation[i] = (b->hw[i] - dst->hw[i]) / dist; 
			}
		}
	}

	vec3_interpolate_piecewise(dst->center, a->center, b->center, interpolation);
	assert(dst->hw[0] > 0.0f && dst->hw[1] > 0.0f && dst->hw[2]);
	return 1;

}

f32 cylinder_distance(const struct cylinder *a, const struct cylinder *b)
{
	vec2 dist = { 0.0f, 0.0f };
	f32 m, t = (a->center[0] - b->center[0]) * (a->center[0] - b->center[0])
	       	+ (a->center[2] - b->center[2]) * (a->center[2] - b->center[2]);
	if (t > (a->radius+b->radius)*(a->radius+b->radius)) { dist[0] = f32_sqrt(t) - a->radius - b->radius; }
	
	m = b->center[1] - b->half_height - (a->center[1] + a->half_height);
	t = a->center[1] - a->half_height - (b->center[1] + b->half_height);
	if (m > 0.0f || t > 0.0f) { dist[1] = f32_max(m,t); }

	return vec2_length(dist);
}

i32 cylinder_test(const struct cylinder *a, const struct cylinder *b)
{
	f32 m, t = (a->center[0] - b->center[0]) * (a->center[0] - b->center[0])
	       	+ (a->center[2] - b->center[2]) * (a->center[2] - b->center[2])
	       	- (a->radius + b->radius) * (a->radius + b->radius);
	if (t > 0.0f) { return 0; }

	m = b->center[1] - b->half_height - (a->center[1] + a->half_height);
	t = a->center[1] - a->half_height - (b->center[1] + b->half_height);
	if (m > 0.0f || t > 0.0f) { return 0; }

	return 1;
}

//i32 cylinder_intersection(struct tmp *dst, const struct cylinder *a, const struct cylinder *b)
//{
//	return 0;
//}

//f32 sphere_distance(const struct sphere *a, const struct sphere *b)
//{
//	return 0.0f;
//}
//
//i32 sphere_test(const struct sphere *a, const struct sphere *b)
//{
//	return 0;
//}
//
//i32 sphere_intersection(struct tmp *dst, const struct sphere *a, const struct sphere *b)
//{
//	return 0;
//}
//
//f32 OBB_distance(const struct OBB *a, const struct OBB *b)
//{
//	return 0.0f;
//}
//
//i32 OBB_test(const struct OBB *a, const struct OBB *b)
//{
//	return 0;
//}
//
//i32 OBB_intersection(struct tmp *dst, const struct OBB *a, const struct OBB *b)
//{
//	return 0;
//}

u32 tetrahedron_point_test(const vec3 tetra[4], const vec3 p)
{
	vec3 v[4], n;
	f32 d_1, d_2;
	
	vec3_sub(v[0], tetra[0], p);
	vec3_sub(v[1], tetra[1], p);
	vec3_sub(v[2], tetra[2], p);
	vec3_sub(v[3], tetra[3], p);

	vec3_recenter_cross(n, v[0], v[1], v[2]);
	d_1 = vec3_dot(n, v[0]);
	d_2 = vec3_dot(n, v[3]);
	if ( (d_1 < 0.0f && d_2 < 0.0f) || (d_1 > 0.0f && d_2 > 0.0f) ) { return 0; }

	vec3_recenter_cross(n, v[1], v[0], v[3]);
	d_1 = vec3_dot(n, v[1]);
	d_2 = vec3_dot(n, v[2]);
	if ( (d_1 < 0.0f && d_2 < 0.0f) || (d_1 > 0.0f && d_2 > 0.0f) ) { return 0; }

	vec3_recenter_cross(n, v[2], v[0], v[3]);
	d_1 = vec3_dot(n, v[2]);
	d_2 = vec3_dot(n, v[1]);
	if ( (d_1 < 0.0f && d_2 < 0.0f) || (d_1 > 0.0f && d_2 > 0.0f) ) { return 0; }

	vec3_recenter_cross(n, v[3], v[1], v[2]);
	d_1 = vec3_dot(n, v[3]);
	d_2 = vec3_dot(n, v[0]);
	if ( (d_1 < 0.0f && d_2 < 0.0f) || (d_1 > 0.0f && d_2 > 0.0f) ) { return 0; }

	return 1;
}

f32 triangle_origin_closest_point(vec3 lambda, const vec3 A, const vec3 B, const vec3 C)
{
	vec3 a;

	vec3_sub(a, B, A);
	const f32 delta_01_0 = vec3_dot(a, B);
	vec3_sub(a, A, B);
	const f32 delta_01_1 = vec3_dot(a, A);
	vec3_sub(a, A, C);
	const f32 delta_012_2 = delta_01_0 * vec3_dot(a, A) + delta_01_1 * vec3_dot(a, B);

	vec3_sub(a, C, A);
	const f32 delta_02_0 = vec3_dot(a, C);
	vec3_sub(a, A, C);
	const f32 delta_02_2 = vec3_dot(a, A);
	vec3_sub(a, A, B);
	const f32 delta_012_1 = delta_02_0 * vec3_dot(a, A) + delta_02_2 * vec3_dot(a, C);
		
	vec3_sub(a, C, B);
	const f32 delta_12_1 = vec3_dot(a, C);
	vec3_sub(a, B, C);
	const f32 delta_12_2 = vec3_dot(a, B);
	vec3_sub(a, B, A);
	const f32 delta_012_0 = delta_12_1 * vec3_dot(a, B) + delta_12_2 * vec3_dot(a, C);
		
	const f32 delta = delta_012_0 + delta_012_1 + delta_012_2;
	lambda[0] = delta_012_0 / delta;
	lambda[1] = delta_012_1 / delta;
	lambda[2] = delta_012_2 / delta;

	return delta;
}

u32 triangle_origin_closest_point_is_internal(vec3 lambda, const vec3 A, const vec3 B, const vec3 C)
{
	triangle_origin_closest_point(lambda, A, B, C);
	return (lambda[0] < 0.0f || lambda[1] < 0.0f || lambda[2] < 0.0f) ? 0 : 1;
}

static vec3 box_vertex[] =
{
	{  0.5f,  0.5f, -0.5f },
	{  0.5f,  0.5f,  0.5f },	
	{ -0.5f,  0.5f,  0.5f },	
	{ -0.5f,  0.5f, -0.5f },	
	{  0.5f, -0.5f, -0.5f },
	{  0.5f, -0.5f,  0.5f },	
	{ -0.5f, -0.5f,  0.5f },	
	{ -0.5f, -0.5f, -0.5f },	
};

static const struct dcel_half_edge box_edge[] =
{
	{ .origin = 0, .twin =  7, /* .face = 0,*/ .next =  1, . prev =  3, },
	{ .origin = 1, .twin = 11, /* .face = 0,*/ .next =  2, . prev =  0, },
	{ .origin = 2, .twin = 15, /* .face = 0,*/ .next =  3, . prev =  1, },
	{ .origin = 3, .twin = 19, /* .face = 0,*/ .next =  0, . prev =  2, },

	{ .origin = 0, .twin = 18, /* .face = 1,*/ .next =  5, . prev =  7, },
	{ .origin = 4, .twin = 21, /* .face = 1,*/ .next =  6, . prev =  4, },
	{ .origin = 5, .twin =  8, /* .face = 1,*/ .next =  7, . prev =  5, },
	{ .origin = 1, .twin =  0, /* .face = 1,*/ .next =  4, . prev =  6, },

	{ .origin = 1, .twin =  5, /* .face = 2,*/ .next =  9, . prev = 11, },
	{ .origin = 5, .twin = 20, /* .face = 2,*/ .next = 10, . prev =  8, },
	{ .origin = 6, .twin = 12, /* .face = 2,*/ .next = 11, . prev =  9, },
	{ .origin = 2, .twin =  1, /* .face = 2,*/ .next =  8, . prev = 10, },

	{ .origin = 2, .twin =  6, /* .face = 3,*/ .next = 13, . prev = 15, },
	{ .origin = 6, .twin = 23, /* .face = 3,*/ .next = 14, . prev = 12, },
	{ .origin = 7, .twin = 16, /* .face = 3,*/ .next = 15, . prev = 13, },
	{ .origin = 3, .twin =  2, /* .face = 3,*/ .next = 12, . prev = 14, },

	{ .origin = 3, .twin = 14, /* .face = 4,*/ .next = 17, . prev = 19, },
	{ .origin = 7, .twin = 22, /* .face = 4,*/ .next = 18, . prev = 16, },
	{ .origin = 4, .twin =  4, /* .face = 4,*/ .next = 19, . prev = 17, },
	{ .origin = 0, .twin =  3, /* .face = 4,*/ .next = 16, . prev = 18, },

	{ .origin = 6, .twin =  9, /* .face = 5,*/ .next = 21, . prev = 23, },
	{ .origin = 5, .twin =  5, /* .face = 5,*/ .next = 22, . prev = 20, },
	{ .origin = 4, .twin = 17, /* .face = 5,*/ .next = 23, . prev = 21, },
	{ .origin = 7, .twin = 13, /* .face = 5,*/ .next = 20, . prev = 22, },
};

/* 
const u32 box_face_to_edge_map[] = { 0, 4, 8, 12, 16, 20 };
*/

struct dcel dcel_box(void)
{
	struct dcel box = 
	{
		.vertex = box_vertex,
		.edge = box_edge,
		.vertex_count = 8,
		.edge_count = 24,
	};

	return box; 
}

void dcel_assert_topology(const struct dcel *dcel)
{
	struct arena tmp = arena_alloc_1MB();

	u32 face_count = 0;
	u32 vertex_count = 0;

	u32 *vertex_check = arena_push_zero(&tmp, dcel->vertex_count * sizeof(u32));
	u32 *edge_check = arena_push_zero(&tmp, dcel->edge_count * sizeof(u32));
	u32 *face_check = arena_push_zero(&tmp, 3*dcel->vertex_count * sizeof(u32));

	for (u32 i = 0; i < dcel->edge_count; ++i)
	{
		if (!edge_check[i])
		{
			face_count += 1;
			u32 next;
		        u32 prev; 
			u32 current = i;
			u32 edge_count = 0;
			do
			{
				edge_count += 1;
				const struct dcel_half_edge *c = dcel->edge + current;
				const struct dcel_half_edge *p = dcel->edge + c->prev;
				const struct dcel_half_edge *n = dcel->edge + c->next;
				const struct dcel_half_edge *t = dcel->edge + c->twin;

				current = c->next;

				kas_assert(c->origin < dcel->vertex_count);
				kas_assert(p->next == i);
				kas_assert(n->prev == i);
				kas_assert(t->twin == i);
				kas_assert(t->origin == n->origin);

				edge_check[current] = 1;
				vertex_count += (1 - vertex_check[c->origin]);
				vertex_check[c->origin] = 1;
			} while (current != i);

			kas_assert(edge_count >= 3);
		}
	}

	arena_free_1MB(&tmp);

	kas_assert(vertex_count == dcel->vertex_count);
	kas_assert(face_count >= 4);
}
