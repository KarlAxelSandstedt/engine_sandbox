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
#include "kas_random.h"

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

static struct dcel_edge box_edge[] =
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
	struct dcel_edge *e0 = h->e + h->f[fi].first;
	struct dcel_edge *e1 = h->e + h->f[fi].first + 1;
	struct dcel_edge *e2 = h->e + h->f[fi].first + 2;
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
	struct dcel_edge *edge0 = h->e + e0; 
	struct dcel_edge *edge1 = h->e + e1; 

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

void dcel_edge_direction(vec3 dir, const struct dcel *h, const u32 ei)
{
	struct dcel_edge *e0 = h->e + ei;
	struct dcel_face *f = h->f + e0->face_ccw;
	const u32 next = f->first + ((ei - f->first + 1) % f->count);
	struct dcel_edge *e1 = h->e + next;
	vec3_sub(dir, h->v[e1->origin], h->v[e0->origin]);
	assert(vec3_length(dir) >= 100.0f*F32_EPSILON);
}

void dcel_edge_normal(vec3 dir, const struct dcel *h, const u32 ei)
{
	dcel_edge_direction(dir, h, ei);
	vec3_mul_constant(dir, 1.0f / vec3_length(dir));
}

struct segment dcel_edge_segment(const struct dcel *h, mat3 rot, const vec3 pos, const u32 ei)
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
	struct dcel_edge *e;
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

u32 internal_convex_hull_tetrahedron_indices(u32 indices[4], const vec3ptr v, const u32 v_count, const f32 tol)
{
	vec3 a, b, n;

	const f32 tol_sq = tol*tol;
	indices[0] = 0;
	u32 i = 1;
	/* Find two points not to close to each other */
	for (; i < v_count; ++i)
	{
		vec3_sub(a, v[i], v[0]);
		const f32 dist_sq = vec3_dot(a, a);
		if (dist_sq > tol_sq)
		{
			//vec3_mul_constant(a, 1.0f / len);
			indices[1] = i;
			i += 1;
			break;
		}
	}	

	/* Find non-collinear point */
	for (; i < v_count; ++i)
	{
		vec3_sub(b, v[i], v[0]);
		vec3_cross(n, a, b);
		const f32 dist = vec3_length(n);
		const f32 area = dist / 2.0f;
		if (area > tol_sq)
		{
			indices[2] = i;
			i += 1;
			vec3_mul_constant(n, 1.0f / dist);
			break;
		}
	}

	/* Find non-coplanar point */
	for (; i < v_count; ++i)
	{
		vec3_sub(a, v[i], v[0]);
		const f32 height = vec3_dot(a, n);
		if (f32_abs(height) > tol)
		{
			indices[3] = i;
			break;
		}
	}

	return i < v_count;
}

struct ddcel_face
{
	POOL_SLOT_STATE;
	struct dll	ce_list;
	vec3		normal;
	u32 		first;	/* first half edge */
	u32 		count;	/* edge count */
};

struct ddcel_edge
{
	POOL_SLOT_STATE;
	u32 		origin;		/* vertex index origin */
	u32 		twin; 		/* twin half edge */
	u32 		next;		/* next ccw edge */
	u32 		prev;		/* prev ccw edge */
	u32 		face_ccw; 	/* face to the left of half edge */
	u32		horizon; /* used in horizon derivation step */
};

struct conflict_edge
{
	DLL_SLOT_STATE;
	DLL2_SLOT_STATE;
	u32 vertex;
	u32 face;
	POOL_SLOT_STATE;
};

struct conflict_vertex
{
	struct dll	ce_list;
	u32		index;
};

struct horizon_vertex
{
	u32	edge1;			/* new edge; going INTO conflict vertex */
	u32	edge2;			/* new edge; going OUT OF  conflict vertex */
	u32 	edge_in;		/* edge going in to vertex */
	u32 	edge_out;		/* edge going out from vertex */
	u32 	edge_out_twin_face;	/* face of edge_out's twin */
	u32	colinear;		/* is twin face colinear */
};

/*
 * (Computational Geometry Algorithms and Applications, Section 2.2) 
 * ddcel - **dynamic** doubly-connected edge list. similar to a dcel but contains extra information in order to
 * construct itself iteratively. 
 */
struct ddcel
{
	struct pool 		face_pool;
	struct pool		edge_pool;
	/* pools are not growable, so safe to use these */
	struct ddcel_face *	f;		
	struct ddcel_edge *	e;
	const vec3ptr		v;
	u32 			v_count;

	/* internal */
	struct arena		tmp1;
	struct arena		tmp2;
	struct pool 		ce_pool;
	struct conflict_edge *	ce;
	struct conflict_vertex *cv;
	struct horizon_vertex * hv;
};

static void ddcel_face_set(struct ddcel_face *face, const u32 first, const u32 count)
{
	face->first = first;
	face->count = count;
	face->ce_list = dll2_init(struct conflict_edge);
}

static void ddcel_edge_set(struct ddcel_edge *edge, const u32 origin, const u32 twin, const u32 prev, const u32 next, const u32 face_ccw)
{
	edge->origin = origin;
	edge->twin = twin;
	edge->next = next;
	edge->prev = prev;
	edge->face_ccw = face_ccw;
	edge->horizon = 0;
}

static void ddcel_assert_topology(const struct ddcel *dcel)
{
	struct arena tmp = arena_alloc_1MB();

	u32 face_count = 0;
	u32 vertex_count = 0;

	u32 *vertex_check = arena_push_zero(&tmp, dcel->v_count * sizeof(u32));
	u32 *edge_check = arena_push_zero(&tmp, dcel->edge_pool.count * sizeof(u32));
	u32 *face_check = arena_push_zero(&tmp, 3*dcel->v_count * sizeof(u32));

	for (u32 i = 0; i < dcel->edge_pool.count; ++i)
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
				const struct ddcel_edge *c = dcel->e + current;
				const struct ddcel_edge *p = dcel->e + c->prev;
				const struct ddcel_edge *n = dcel->e + c->next;
				const struct ddcel_edge *t = dcel->e + c->twin;

				kas_assert(c->horizon == 0);
				kas_assert(c->origin < dcel->v_count);
				kas_assert(p->next == current);
				kas_assert(n->prev == current);
				kas_assert(t->twin == current);
				kas_assert(t->origin == n->origin);

				edge_check[current] = 1;
				vertex_count += (1 - vertex_check[c->origin]);
				vertex_check[c->origin] = 1;

				current = c->next;
			} while (current != i);

			kas_assert(edge_count >= 3);
		}
	}

	arena_free_1MB(&tmp);

	kas_assert(face_count >= 4);
}

static void internal_convex_hull_tetrahedron_ddcel(struct ddcel *ddcel, const vec3ptr v, const u32 v_count)
{
	vec3 a, b, c, cr;
	vec3_sub(a, v[ddcel->cv[1].index], v[ddcel->cv[0].index]);
	vec3_sub(b, v[ddcel->cv[2].index], v[ddcel->cv[0].index]);
	vec3_sub(c, v[ddcel->cv[3].index], v[ddcel->cv[0].index]);
	vec3_cross(cr, a, b);

	/* CCW == inside gives negative dot product for any polygon on a convex polyhedron */
	if (vec3_dot(cr, c) > 0.0f)
	{
		/* Make 0->1->2->0 CCW */
		const u32 tmp = ddcel->cv[1].index;
		ddcel->cv[1].index = ddcel->cv[2].index;
		ddcel->cv[2].index = tmp;
	}

	struct ddcel_face *f0 = pool_add(&ddcel->face_pool).address;
	struct ddcel_face *f1 = pool_add(&ddcel->face_pool).address;
	struct ddcel_face *f2 = pool_add(&ddcel->face_pool).address;
	struct ddcel_face *f3 = pool_add(&ddcel->face_pool).address;

	struct ddcel_edge *e0 = pool_add(&ddcel->edge_pool).address;
	struct ddcel_edge *e1 = pool_add(&ddcel->edge_pool).address;
	struct ddcel_edge *e2 = pool_add(&ddcel->edge_pool).address;
	struct ddcel_edge *e3 = pool_add(&ddcel->edge_pool).address;
	struct ddcel_edge *e4 = pool_add(&ddcel->edge_pool).address;
	struct ddcel_edge *e5 = pool_add(&ddcel->edge_pool).address;
	struct ddcel_edge *e6 = pool_add(&ddcel->edge_pool).address;
	struct ddcel_edge *e7 = pool_add(&ddcel->edge_pool).address;
	struct ddcel_edge *e8 = pool_add(&ddcel->edge_pool).address;
	struct ddcel_edge *e9 = pool_add(&ddcel->edge_pool).address;
	struct ddcel_edge *e10 = pool_add(&ddcel->edge_pool).address;
	struct ddcel_edge *e11 = pool_add(&ddcel->edge_pool).address;

	ddcel_face_set(f0, 0, 3);
	ddcel_face_set(f1, 3, 3);
	ddcel_face_set(f2, 6, 3);
	ddcel_face_set(f3, 9, 3);

	ddcel_edge_set(e0,  ddcel->cv[0].index,  4,  2,  1, 0);
	ddcel_edge_set(e1,  ddcel->cv[1].index, 10,  0,  2, 0);
	ddcel_edge_set(e2,  ddcel->cv[2].index,  7,  1,  0, 0);

	ddcel_edge_set(e3,  ddcel->cv[3].index, 11,  5,  4, 1);
	ddcel_edge_set(e4,  ddcel->cv[1].index,  0,  3,  5, 1);
	ddcel_edge_set(e5,  ddcel->cv[0].index,  6,  4,  3, 1);

	ddcel_edge_set(e6,  ddcel->cv[3].index,  5,  8,  7, 2);
	ddcel_edge_set(e7,  ddcel->cv[0].index,  2,  6,  8, 2);
	ddcel_edge_set(e8,  ddcel->cv[2].index,  9,  7,  6, 2);

	ddcel_edge_set(e9,  ddcel->cv[3].index,  8, 11, 10, 3);
	ddcel_edge_set(e10, ddcel->cv[2].index,  1,  9, 11, 3);
	ddcel_edge_set(e11, ddcel->cv[1].index,  3, 10,  9, 3);

	for (u32 i = 0; i < 4; ++i)
	{
		vec3_sub(a, ddcel->v[e1->origin], ddcel->v[e0->origin]);
		vec3_sub(b, ddcel->v[e2->origin], ddcel->v[e0->origin]);
		vec3_cross(cr, b, a);
		vec3_normalize(ddcel->f[i].normal, cr);
	}

	ddcel_assert_topology(ddcel);
}

static void internal_convex_hull_tetrahedron_conflicts(struct ddcel *ddcel, const vec3ptr v, const u32 v_count, const f32 tol)
{
	/*
	 * Bipartite vertex-face conflict graph:
	 * Since the conflict graph is bipartite, each edge belongs to two lists, the edge list of the vertex, and
	 * the edge list of the face. When we choose a conflict vertex, we traverse all of its edges, and for each
	 * edge, remove all edges in the face-edge list it belongs to:
	 *
	 * v = vertex_choice;
	 * for (edge in v.edge_list)
	 * 	f = edge.face
	 * 	for (edge in f.edge_list)
	 * 		edge.remove
	 * 	f.remove
	 */

	vec3 b, c, cr[4];
	for (u32 f_i = 0; f_i < 4; ++f_i)
	{
		const u32 v0_i = ddcel->e[3*f_i + 0].origin;
		const u32 v1_i = ddcel->e[3*f_i + 1].origin;
		const u32 v2_i = ddcel->e[3*f_i + 2].origin;

		/* a -> b -> c, CCW, cross points outwards  */
		vec3_sub(b, v[v1_i], v[v0_i]);
		vec3_sub(c, v[v2_i], v[v0_i]);
		vec3_cross(cr[f_i], b, c);
		vec3_mul_constant(cr[f_i], 1.0f / vec3_length(cr[f_i]));
	}

	for (u32 cv_i = 4; cv_i < v_count; ++cv_i)
	{
		struct conflict_vertex *cv = ddcel->cv + cv_i;
		for (u32 f_i = 0; f_i < 4; ++f_i)
		{
			const u32 v0_i = ddcel->e[3*f_i + 0].origin;
			vec3_sub(b, v[cv->index], v[v0_i]);
			/* If point is "in front" of face, we have a conflict */
			if (vec3_dot(cr[f_i], b) > tol)
			{
				struct slot slot = pool_add(&ddcel->ce_pool);
				dll_append(&cv->ce_list, ddcel->ce_pool.buf, slot.index);
				dll_append(&ddcel->f[f_i].ce_list, ddcel->ce_pool.buf, slot.index);

				struct conflict_edge *edge = slot.address;
				edge->vertex = cv_i;
				edge->face = f_i;
			}
		}
	}
}

void convex_hull_iteration(struct ddcel *ddcel, const u32 cvi, const f32 tol)
{
	if (ddcel->cv[cvi].ce_list.count == 0) { return; }

	struct conflict_vertex *cv = ddcel->cv + cvi;
	struct conflict_edge *ce = NULL;

	/* (5) Get horizon edges:
	 * At this point, all edges has horizon set to 0. Whenever we visit an edge, 
	 * we flip the edge and its twin's horizon value. After the loop the horizon
	 * will consist of the edges with value 1.  */
	for (u32 i = cv->ce_list.first; i != DLL_NULL; i = DLL_NEXT(ce))
	{
		ce = ddcel->ce + i;
		const u32 fi = ce->face;
		struct ddcel_edge *e = ddcel->e + ddcel->f[fi].first;
		for (u32 j = 0; j < ddcel->f[fi].count; ++j)
		{
			struct ddcel_edge *twin = ddcel->e + e->twin;
			e->horizon = !e->horizon;
			twin->horizon = !twin->horizon;
			e = ddcel->e + e->next;
		}
	}

	/* (6) loop through and remove all faces and non-horizon edges. Sort horizon edges in a 
	 * ad-hoc dll structure horizon vertex and cache additional data.
	 */
	u32 horizon_start = 0;
	u32 horizon_count = 0;
	ce = NULL;
	for (u32 i = cv->ce_list.first; i != DLL_NULL; i = DLL_NEXT(ce))
	{
		ce = ddcel->ce + i;
		const u32 fi = ce->face;
		struct ddcel_face *f = ddcel->f + fi;

		u32 ej = f->first;
		for (u32 j = 0; j < f->count; ++j)
		{
			struct ddcel_edge *e = ddcel->e + ej;

			if (!e->horizon)
			{
				pool_remove(&ddcel->edge_pool, e->next);	
			}
			else
			{
				struct ddcel_edge *e_twin = ddcel->e + e->twin;
				struct ddcel_face *f_twin = ddcel->f + e_twin->face_ccw;
				struct ddcel_edge *e_next = ddcel->e + e->next;
				struct ddcel_edge *e_prev = ddcel->e + e->prev;

				horizon_count += 1;
				horizon_start = e->origin;

				e->horizon = 0;
				e_twin->horizon = 0;

				ddcel->hv[e->origin].edge_out_twin_face = e_twin->face_ccw;
				ddcel->hv[e->origin].edge_out = ej;
				ddcel->hv[e_next->origin].edge_in = ej;

				ddcel->hv[e->origin].colinear = (vec3_dot(f_twin->normal, ddcel->v[cv->index]) < tol)
					? 1
					: 0;
			}
			ej = e->next;
		}
	}

	/* (7) We don't want to start in the middle of a colinear_face, so we traverse the horizon until we find a 
	 * new triangle.
	 */
	const u32 face = ddcel->hv[horizon_start].edge_out_twin_face;
	for (u32 i = 0; i < horizon_count; ++i)
	{
		horizon_start = ddcel->e[ddcel->hv[horizon_start].edge_out].origin;
		if (ddcel->hv[horizon_start].edge_out_twin_face != face)
		{
			break;
		}
	}

	/* (8) Traverse the horizon creaing new faces and removing any colinear horizon
	 * twin edges. 
	 */
	vec3 a, b, cr;
	u32 horizon = horizon_start;
	u32 prev_horizon = ddcel->e[ddcel->hv[horizon].edge_in].origin;
	for (u32 i = 0; i < horizon_count; ++i)
	{	
		const u32 twin_face = ddcel->hv[horizon].edge_out_twin_face;
		const u32 ccw_face = ddcel->e[ddcel->hv[horizon].edge_out].face_ccw;
		struct ddcel_face *f_ccw = ddcel->f + ccw_face;
		struct ddcel_face *f_twin = ddcel->f + twin_face;

		struct slot se1 = pool_add(&ddcel->edge_pool);
		struct slot se2 = pool_add(&ddcel->edge_pool);

		struct ddcel_edge *e1 = se1.address;
		struct ddcel_edge *e2 = se2.address;

		ddcel->hv[horizon].edge1 = se1.index;
		ddcel->hv[horizon].edge2 = se2.index;

		if (ddcel->hv[horizon].colinear)
		{
			u32 prev_horizon_edge1 = U32_MAX;
			if (i != 0)
			{ 
				prev_horizon_edge1 = ddcel->hv[prev_horizon].edge1;
				ddcel->e[ddcel->hv[prev_horizon].edge1].twin = se2.index;
			}

			const u32 start = horizon;
			u32 end = start;

			u32 twin_start_next = ddcel->e[
					      ddcel->e[
					      ddcel->hv[start].edge_out]
						      	      .twin]
							      .next;
			u32 twin_end_prev;
			i -= 1;
			do
			{
				horizon = end;
				f_twin->count -= 1;
				i += 1;
				const u32 edge = ddcel->hv[end].edge_out;
				twin_end_prev = ddcel->e[
					        ddcel->e[edge].twin]
					                      .prev;
				end = ddcel->e[
				      ddcel->e[edge].next]
						    .origin;

				pool_remove(&ddcel->edge_pool, ddcel->e[edge].twin);
				pool_remove(&ddcel->edge_pool, edge);
			} while (ddcel->hv[end].edge_out_twin_face == twin_face);

			f_twin->count += 2;

			ddcel_edge_set(e1, end, U32_MAX, twin_end_prev, se2.index, twin_face);
			ddcel_edge_set(e2, cv->index, prev_horizon_edge1, se1.index, twin_start_next, twin_face);
			ddcel->e[twin_end_prev].next = se1.index;
			ddcel->e[twin_start_next].prev = se2.index;

			ddcel->hv[horizon].edge1 = se1.index;
			ddcel->hv[horizon].edge2 = se2.index;

			/* Note: Since we reuse the twin face, we also reuse its list of conflicts, so we are done. */
		}
		else
		{
			struct slot sf = pool_add(&ddcel->face_pool);

			const u32 e0i = ddcel->hv[horizon].edge_out;

			struct ddcel_face *f = sf.address;
			struct ddcel_edge *e0 = ddcel->e + e0i;

			if (i != 0)
			{
				ddcel->e[ddcel->hv[prev_horizon].edge1].twin = se2.index;
			}

			ddcel_edge_set(e2, cv->index, ddcel->hv[prev_horizon].edge1, se1.index, e0i, sf.index);
			ddcel_edge_set(e1, ddcel->e[e0->next].origin, U32_MAX, e0i, se2.index, sf.index);
			ddcel_edge_set(e0, e0->origin, e0->twin, se2.index, se1.index, sf.index);

			ddcel_face_set(f, ddcel->hv[horizon].edge_out, 3);
			vec3_sub(a, ddcel->v[e1->origin], ddcel->v[e0->origin]);
			vec3_sub(b, ddcel->v[e2->origin], ddcel->v[e0->origin]);
			vec3_cross(cr, b, a);
			vec3_normalize(f->normal, cr);
		
			struct conflict_edge *ce = NULL;
			for (u32 j = f_ccw->ce_list.first; j != DLL_NULL; )
			{
				ce = ddcel->ce + j;
				const u32 next = DLL2_NEXT(ce);

				if (vec3_dot(f->normal, ddcel->v[ce->vertex]) > tol)
				{
					struct slot slot = pool_add(&ddcel->ce_pool);
					dll_append(&f->ce_list, &ddcel->ce_pool.buf, slot.index);
					dll_append(&ddcel->cv[ce->vertex].ce_list, &ddcel->ce_pool.buf, slot.index);

					struct conflict_edge *new = slot.address;
					new->vertex = ce->vertex;
					new->face = sf.index;
				}

				j = next;
			}
		}

		prev_horizon = horizon;
		horizon = ddcel->e[ddcel->hv[horizon].edge_out].origin;

	}
	ddcel->e[ddcel->hv[prev_horizon].edge1].twin = ddcel->hv[horizon].edge2;
	ddcel->e[ddcel->hv[horizon].edge2].twin = ddcel->hv[prev_horizon].edge1;

	/* (9) Update all conflict_edge lists of vertices conflicting with face,
	 * and remove all conflicting edges to face.  */

	for (u32 i = cv->ce_list.first; i != DLL_NULL;)
	{
		ce = ddcel->ce + i;
		i = DLL_NEXT(ce);
		const u32 fi = ce->face;
		struct ddcel_face *f = ddcel->f + fi;

		struct conflict_edge *ce = NULL;
		for (u32 j = f->ce_list.first; j != DLL_NULL; )
		{
			ce = ddcel->ce + j;
			const u32 next = DLL2_NEXT(ce);

			struct conflict_vertex *cvj = ddcel->cv + ce->vertex;
			dll_remove(&cvj->ce_list, ddcel->ce_pool.buf, j);
			pool_remove(&ddcel->ce_pool, j);

			j = next;
		}
		pool_remove(&ddcel->face_pool, fi);
	}
	kas_assert(cv->ce_list.count == 0);
}

struct dcel dcel_convex_hull(struct arena *mem, const vec3ptr v, const u32 v_count, const f32 tol)
{
	struct dcel dcel = dcel_empty();
	if (v_count < 4) { goto end; }	

	struct arena tmp1 = arena_alloc_1MB();
	struct arena tmp2 = arena_alloc_1MB();

	const u32 edge_count_upper_bound = 6*v_count - 12;
	const u32 face_count_upper_bound = 2*v_count - 4;
	struct ddcel ddcel =
	{
		.face_pool = pool_alloc(mem, 2*face_count_upper_bound, struct ddcel_face, NOT_GROWABLE),	/* add additional space for easier memory management */
		.edge_pool = pool_alloc(mem, 2+edge_count_upper_bound, struct ddcel_edge, NOT_GROWABLE),	/* add additional space for easier memory management */
		.v = v,
		.v_count = v_count,
		.ce_pool = pool_alloc(&tmp2, tmp2.mem_size / sizeof(struct conflict_edge), struct conflict_edge, NOT_GROWABLE),
		.cv = arena_push(&tmp1, v_count * sizeof(struct conflict_vertex)),
		.hv = arena_push(&tmp1, v_count * sizeof(struct horizon_vertex)),
	};

	ddcel.tmp1 = tmp1;
	ddcel.tmp2 = tmp2;
	ddcel.e = (struct ddcel_edge *) ddcel.edge_pool.buf;
	ddcel.f = (struct ddcel_face *) ddcel.face_pool.buf;
	ddcel.ce = (struct conflict_edge *) ddcel.ce_pool.buf;

	/* (1) permutation - Random permutation of remaining points */
	for (u32 i = 0; i < v_count; ++i)
	{
		ddcel.cv[i].ce_list = dll_init(struct conflict_edge);
		ddcel.cv[i].index = i;
	}
	for (u32 i = 0; i < v_count; ++i)
	{
		const u32 rng = (u32) rng_u64_range(i, v_count-1);
		const u32 tmp = ddcel.cv[i].index;
		ddcel.cv[i].index = ddcel.cv[rng].index;
		ddcel.cv[rng].index = tmp;
	}

	/* (2) Get inital points for tetrahedron */
	u32 tetra[4] = { 0 };
	if (internal_convex_hull_tetrahedron_indices(tetra, v, v_count, tol) == 0) { goto end; }

	for (u32 i = 0; i < 4; ++i)
	{
		const u32 tmp = ddcel.cv[i].index;
		ddcel.cv[i].index = ddcel.cv[tetra[i]].index;
		ddcel.cv[tetra[i]].index = tmp;
	}

	/* (3) initiate DCEL from points */
	internal_convex_hull_tetrahedron_ddcel(&ddcel, v, v_count);

	/* (4) setup conflict graph */
	internal_convex_hull_tetrahedron_conflicts(&ddcel, v, v_count, tol);

	/* iteratetively solve and add conflicts until no vertices left */
	for (u32 i = 4; i < v_count; ++i)
	{
		convex_hull_iteration(&ddcel, i, tol);
	}
end:
	arena_free_1MB(&tmp1);
	arena_free_1MB(&tmp2);

	return dcel;
}
