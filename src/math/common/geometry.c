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

u32 sphere_raycast(vec3 intersection, const struct sphere *sph, const struct ray *ray)
{
	const f32 t = sphere_raycast_parameter(sph, ray);
	if (t < 0.0f || t == F32_INFINITY) { return 0; }

	vec3_copy(intersection, ray->origin);
	vec3_translate_scaled(intersection, ray->dir, t);
	return 1;
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

u32 vertex_support(vec3 support, const vec3 dir, const vec3ptr v, const u32 v_count)
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
	return best;
}

void vertex_centroid(vec3 centroid, const vec3ptr vs, const u32 n)
{
	vec3_set(centroid, 0.0f, 0.0f, 0.0f);
	for (u32 i = 0; i < n; ++i)
	{
		vec3_translate(centroid, vs[i]);
	}
	vec3_mul_constant(centroid, 1.0f / n);
}

void tri_ccw_normal(vec3 normal, const vec3 p0, const vec3 p1, const vec3 p2)
{
	vec3 A, B, C;
	vec3_sub(A, p1, p0);
	vec3_sub(B, p2, p0);
	vec3_cross(C, A, B);
	vec3_normalize(normal, C);
}

vec3 box_stub_vertex[8] =
{
	{  0.5f,  0.5f,  0.5f }, 
	{  0.5f,  0.5f, -0.5f },	
	{ -0.5f,  0.5f, -0.5f },	
	{ -0.5f,  0.5f,  0.5f },	
	{  0.5f, -0.5f,  0.5f },
	{  0.5f, -0.5f, -0.5f },	
	{ -0.5f, -0.5f, -0.5f },	
	{ -0.5f, -0.5f,  0.5f },	
};


static struct dcel_face box_face[] =
{
	{ .first  =  0, .count = 4 },
	{ .first  =  4, .count = 4 },
	{ .first  =  8, .count = 4 },
	{ .first  = 12, .count = 4 },
	{ .first  = 16, .count = 4 },
	{ .first  = 20, .count = 4 },
};

static struct dcel_half_edge box_edge[] =
{
	{ .origin = 0, .twin =  7,  .face_ccw = 0, },
	{ .origin = 1, .twin = 11,  .face_ccw = 0, },
	{ .origin = 2, .twin = 15,  .face_ccw = 0, },
	{ .origin = 3, .twin = 19,  .face_ccw = 0, },

	{ .origin = 0, .twin = 18,  .face_ccw = 1, },
	{ .origin = 4, .twin = 21,  .face_ccw = 1, },
	{ .origin = 5, .twin =  8,  .face_ccw = 1, },
	{ .origin = 1, .twin =  0,  .face_ccw = 1, },

	{ .origin = 1, .twin =  6,  .face_ccw = 2, },
	{ .origin = 5, .twin = 20,  .face_ccw = 2, },
	{ .origin = 6, .twin = 12,  .face_ccw = 2, },
	{ .origin = 2, .twin =  1,  .face_ccw = 2, },

	{ .origin = 2, .twin = 10,  .face_ccw = 3, },
	{ .origin = 6, .twin = 23,  .face_ccw = 3, },
	{ .origin = 7, .twin = 16,  .face_ccw = 3, },
	{ .origin = 3, .twin =  2,  .face_ccw = 3, },

	{ .origin = 3, .twin = 14,  .face_ccw = 4, },
	{ .origin = 7, .twin = 22,  .face_ccw = 4, },
	{ .origin = 4, .twin =  4,  .face_ccw = 4, },
	{ .origin = 0, .twin =  3,  .face_ccw = 4, },

	{ .origin = 6, .twin =  9,  .face_ccw = 5, },
	{ .origin = 5, .twin =  5,  .face_ccw = 5, },
	{ .origin = 4, .twin = 17,  .face_ccw = 5, },
	{ .origin = 7, .twin = 13,  .face_ccw = 5, },
};

struct dcel dcel_box_stub(void)
{
	struct dcel box = 
	{
		.v = box_stub_vertex,
		.e = box_edge,
		.f = box_face,
		.e_count = 24,
		.v_count = 8,
		.f_count = 6,
	};

	return box; 
}

struct dcel dcel_box(struct arena *mem, const vec3 hw)
{
	vec3ptr box_vertex = arena_push(mem, 8*sizeof(vec3));

	vec3_set(box_vertex[0],  hw[0],  hw[1],  hw[2]); 
	vec3_set(box_vertex[1],  hw[0],  hw[1], -hw[2]);	
	vec3_set(box_vertex[2], -hw[0],  hw[1], -hw[2]);	
	vec3_set(box_vertex[3], -hw[0],  hw[1],  hw[2]);	
	vec3_set(box_vertex[4],  hw[0], -hw[1],  hw[2]);
	vec3_set(box_vertex[5],  hw[0], -hw[1], -hw[2]);	
	vec3_set(box_vertex[6], -hw[0], -hw[1], -hw[2]);	
	vec3_set(box_vertex[7], -hw[0], -hw[1],  hw[2]);	

	struct dcel box = 
	{
		.v = box_vertex,
		.e = box_edge,
		.f = box_face,
		.e_count = 24,
		.v_count = 8,
		.f_count = 6,
	};

	return box; 
}

void dcel_face_direction(vec3 dir, const struct dcel *h, const u32 fi)
{
	vec3 a, b;
	struct dcel_half_edge *e0 = h->e + h->f[fi].first;
	struct dcel_half_edge *e1 = h->e + h->f[fi].first + 1;
	struct dcel_half_edge *e2 = h->e + h->f[fi].first + 2;
	vec3_sub(a, h->v[e1->origin], h->v[e0->origin]);
	vec3_sub(b, h->v[e2->origin], h->v[e0->origin]);
	vec3_cross(dir, a, b);
	assert(vec3_length(dir) >= 100.0f*F32_EPSILON);
}

void dcel_face_normal(vec3 normal, const struct dcel *h, const u32 fi)
{
	dcel_face_direction(normal, h, fi);
	vec3_mul_constant(normal, 1.0f/vec3_length(normal));	
}

struct plane dcel_face_plane(const struct dcel *h, mat3 rot, const vec3 pos, const u32 fi)
{
	vec3 n, p;
	dcel_face_normal(p, h, fi);
	mat3_vec_mul(n, rot, p);
	mat3_vec_mul(p, rot, h->v[h->e[h->f[fi].first].origin]);
	vec3_translate(p, pos);
	return plane_construct(n, p);
}

struct segment dcel_face_clip_segment(const struct dcel *h, mat3 rot, const vec3 pos, const u32 fi, const struct segment *s)
{
	vec3 f_n, p_n, p_p0, p_p1;

	dcel_face_normal(p_n, h, fi);
	mat3_vec_mul(f_n, rot, p_n);

	f32 min_p = 0.0f;
	f32 max_p = 1.0f;

	struct dcel_face *f = h->f + fi;
	for (u32 i = 0; i < f->count; ++i)
	{
		const u32 e0 = f->first + i;
		const u32 e1 = f->first + ((i + 1) % f->count);
		struct plane clip_plane = dcel_face_clip_plane(h, rot, pos, f_n, e0, e1);

		const f32 bc_c = plane_segment_clip_parameter(&clip_plane, s);
		if (min_p <= bc_c && bc_c <= max_p)
		{
			if (vec3_dot(s->dir, clip_plane.normal) >= 0.0f)
			{
				max_p = bc_c;
			}
			else
			{
				min_p = bc_c;
			}
		}
	}	

	segment_bc(p_p0, s, min_p);
	segment_bc(p_p1, s, max_p);
	return segment_construct(p_p0, p_p1);
}

struct plane dcel_face_clip_plane(const struct dcel *h, mat3 rot, const vec3 pos, const vec3 face_normal, const u32 e0, const u32 e1)
{
	vec3 diff, p0, p1;
	struct dcel_half_edge *edge0 = h->e + e0; 
	struct dcel_half_edge *edge1 = h->e + e1; 

	mat3_vec_mul(p0, rot, h->v[edge0->origin]);
	mat3_vec_mul(p1, rot, h->v[edge1->origin]);
	vec3_translate(p0, pos);
	vec3_translate(p1, pos);
	vec3_sub(diff, p1, p0);
	vec3_cross(p1, diff, face_normal);
	vec3_mul_constant(p1, 1.0f/vec3_length(p1));

	return plane_construct(p1, p0);
}

u32 dcel_face_projected_point_test(const struct dcel *h, mat3 rot, const vec3 pos, const u32 fi, const vec3 p)
{
	vec3 f_n, p_n;

	dcel_face_normal(p_n, h, fi);
	mat3_vec_mul(f_n, rot, p_n);

	f32 min_p = 0.0f;
	f32 max_p = 1.0f;

	struct dcel_face *f = h->f + fi;
	for (u32 i = 0; i < f->count; ++i)
	{
		const u32 e0 = f->first + i;
		const u32 e1 = f->first + ((i + 1) % f->count);
		struct plane clip_plane = dcel_face_clip_plane(h, rot, pos, f_n, e0, e1);
		if (vec3_dot(clip_plane.normal, p) > clip_plane.signed_distance)
		{
			return 0;
		}
	}	

	return 1;
}

void dcel_half_edge_direction(vec3 dir, const struct dcel *h, const u32 ei)
{
	struct dcel_half_edge *e0 = h->e + ei;
	struct dcel_face *f = h->f + e0->face_ccw;
	const u32 next = f->first + ((ei - f->first + 1) % f->count);
	struct dcel_half_edge *e1 = h->e + next;
	vec3_sub(dir, h->v[e1->origin], h->v[e0->origin]);
	assert(vec3_length(dir) >= 100.0f*F32_EPSILON);
}

void dcel_half_edge_normal(vec3 dir, const struct dcel *h, const u32 ei)
{
	dcel_half_edge_direction(dir, h, ei);
	vec3_mul_constant(dir, 1.0f / vec3_length(dir));
}

struct segment dcel_half_edge_segment(const struct dcel *h, mat3 rot, const vec3 pos, const u32 ei)
{
	vec3 p0, p1;
	const u32 first = h->f[h->e[ei].face_ccw].first;
	const u32 count = h->f[h->e[ei].face_ccw].count;
	const u32 e0 = ei;
	const u32 e1 = first + ((ei - first + 1) % count); 

	mat3_vec_mul(p0, rot, h->v[h->e[e0].origin]);
	mat3_vec_mul(p1, rot, h->v[h->e[e1].origin]);
	vec3_translate(p0, pos);
	vec3_translate(p1, pos);

	return segment_construct(p0, p1);
}

void sphere_support(vec3 support, const vec3 dir, const struct sphere *sph, const vec3 pos)
{
	vec3_scale(support, dir, sph->radius / vec3_length(dir));
	vec3_translate(support, pos);
}

void capsule_support(vec3 support, const vec3 dir, const struct capsule *cap, mat3 rot, const vec3 pos)
{
	vec3 p1, p2;
	p1[0] = rot[1][0] * cap->half_height,	
	p1[1] = rot[1][1] * cap->half_height,	
	p1[2] = rot[1][2] * cap->half_height,	
	vec3_negative_to(p2, p1);

	vec3_scale(support, dir, cap->radius / vec3_length(dir));
	vec3_translate(support, pos);
	(vec3_dot(dir, p1) > vec3_dot(dir, p2))
		? vec3_translate(support, p1) 
		: vec3_translate(support, p2);
}

u32 dcel_support(vec3 support, const vec3 dir, const struct dcel *dcel, mat3 rot, const vec3 pos)
{
	f32 max = -F32_INFINITY;
	u32 max_index = 0;
	vec3 p;
	for (u32 i = 0; i < dcel->v_count; ++i)
	{
		mat3_vec_mul(p, rot, dcel->v[i]);
		const f32 dot = vec3_dot(p, dir);
		if (max < dot)
		{
			max_index = i;
			max = dot; 
		}
	}

	mat3_vec_mul(support, rot, dcel->v[max_index]);
	vec3_translate(support, pos);
	return max_index;
}

struct dcel dcel_empty(void)
{
	struct dcel dcel = { 0 };

	return dcel;
}

void dcel_assert_topology(struct dcel *dcel)
{
	struct dcel_face *f;
	struct dcel_half_edge *e;
	for (u32 i = 0; i < dcel->f_count; ++i)
	{
		f = dcel->f + i;
		e = dcel->e + f->first;
		for (u32 j = 0; j < f->count; ++j)
		{
			assert(e->face_ccw == i);
 			e = dcel->e + f->first + j + 1;
		}

		if (f->first + f->count < dcel->e_count)
		{
			assert(e->face_ccw != i);
		}
	}

	for (u32 i = 0; i < dcel->e_count; ++i)
	{
		e = dcel->e + i;
		assert(i == (dcel->e + e->twin)->twin);
	}
}

//void dcel_assert_topology(const struct dcel *dcel)
//{
//	struct arena tmp = arena_alloc_1MB();
//
//	u32 face_count = 0;
//	u32 vertex_count = 0;
//
//	u32 *vertex_check = arena_push_zero(&tmp, dcel->vertex_count * sizeof(u32));
//	u32 *edge_check = arena_push_zero(&tmp, dcel->edge_count * sizeof(u32));
//	u32 *face_check = arena_push_zero(&tmp, 3*dcel->vertex_count * sizeof(u32));
//
//	for (u32 i = 0; i < dcel->edge_count; ++i)
//	{
//		if (!edge_check[i])
//		{
//			face_count += 1;
//			u32 next;
//		        u32 prev; 
//			u32 current = i;
//			u32 edge_count = 0;
//			do
//			{
//				edge_count += 1;
//				const struct dcel_half_edge *c = dcel->edge + current;
//				const struct dcel_half_edge *p = dcel->edge + c->prev;
//				const struct dcel_half_edge *n = dcel->edge + c->next;
//				const struct dcel_half_edge *t = dcel->edge + c->twin;
//
//				kas_assert(c->origin < dcel->vertex_count);
//				kas_assert(p->next == current);
//				kas_assert(n->prev == current);
//				kas_assert(t->twin == current);
//				kas_assert(t->origin == n->origin);
//
//				edge_check[current] = 1;
//				vertex_count += (1 - vertex_check[c->origin]);
//				vertex_check[c->origin] = 1;
//
//				current = c->next;
//			} while (current != i);
//
//			kas_assert(edge_count >= 3);
//		}
//	}
//
//	arena_free_1MB(&tmp);
//
//	kas_assert(vertex_count == dcel->vertex_count);
//	kas_assert(face_count >= 4);
//}
