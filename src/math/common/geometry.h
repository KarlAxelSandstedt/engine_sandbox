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

#ifndef __GEOMETRY_H__
#define __GEOMETRY_H__

#include "kas_common.h"
#include "allocator.h"
#include "kas_math.h"

/****************** GEOMETRIC PRIMITIVES ******************/

/**
 * AABB: geomtrical primitive; Axis aligned bounding box
 * center: center of the box
 * hw: half width of the box in each dimension
 */
struct AABB 
{
	vec3 center;
	vec3 hw;
};

/**
 * plane: geomtrical primitive
 * normal: normal of plane
 * signed_distance: Signed distance to plane; plane_normal * signed_distance is point on plane.
 */
struct plane 
{
	vec3 normal;
	f32 signed_distance;
};

/**
 * ray: geomtrical primitive
 * origin: Origin of ray
 * dir.direction with length. 
 *
 * NOTE: When deriving parameters t using rays, it is always in the context origin + t * dir. For example,
 * 	If for a given ray we get a collision at a point p with parameter t, The follow will always hold:
 *
 * 	p = ray.origin + t * ray.dir.
 */
struct ray
{
	vec3 origin;
	vec3 dir;
};

/**
 * segment: geomtrical primitive; directional two point segment
 * p0: segment start
 * p1: segment end 
 * dir: non-normalized direction vector
 */
struct segment
{
	vec3 p0;	
	vec3 p1;	
	vec3 dir;	/* p1-p0 */
};

/* TODO replace c_sphere with this... */
struct sphere
{
	vec3 center;
	f32 radius;
};

/********************************** sphere **********************************/

/* constructed sphere */
struct sphere 	sphere_construct(const vec3 center, const f32 radius);
/* return t: smallest t >= 0 such that p = origin + t*dir is a point on the sphere, or F32_INF if no such t exist */
f32 		sphere_raycast_parameter(const struct sphere *sph, const struct ray *ray);

/*********************************** ray ************************************/

/* return constructed ray */
struct ray 	ray_construct(const vec3 origin, const vec3 dir);
/* return segment: s.p0 = r.origin, s.p1 = r.origin + t * r.dir */
struct segment	ray_construct_segment(const struct ray *r, const f32 t);
/* set r_c = ray.origin + t * ray.dir */
void		ray_point(vec3 r_c, const struct ray *ray, const f32 t);
/* return t: closest point on ray to p = origin + t * dir */
f32 		ray_point_closest_point_parameter(const struct ray *ray, const vec3 p);
/* return squared distance from p to ray, and set ray_point to the closest point on the ray */
f32 		ray_point_distance_sq(vec3 ray_point, const struct ray *ray, const vec3 p);
/* return squared distance from s to ray, and set r_c and s_c to the closest points on the primitives */
f32 		ray_segment_distance_sq(vec3 r_c, vec3 s_c, const struct ray *ray, const struct segment *s);
/* return squared distance from s to ray, and set r_c and s_c to the closest points on the primitives */

/********************************* segment **********************************/

/* construct segment */
struct segment 	segment_construct(const vec3 p0, const vec3 p1);
/* return squared distance between s1 and s2; set c1, c2 to closest point on s1, s2 respectively  */
f32 		segment_distance_sq(vec3 c1, vec3 c2, const struct segment *s1, const struct segment *s2);
/* return squared distance between s and p; set c to the closest point on s to p */
f32 		segment_point_distance_sq(vec3 c, const struct segment *s, const vec3 p);
/* Return parameter t of projected barycentric point p to segment s: PROJECTION_ON_LINE(p) = s.p0*(1-t) + s.p1*t */
f32		segment_point_projected_bc_parameter(const struct segment *s, const vec3 p);
/* Return parameter g of closest barycentric point p to segment s: PROJECTION_ON_SEGMENT(p) = s.p0*(1-t) + s.p1*t, 0.0f <= t <= 1.0f */
f32 		segment_point_closest_bc_parameter(const struct segment *s, const vec3 p);
/* set bc_p = s.p0*(1-t) + s.p1*t */
void 		segment_bc(vec3 bc_p, const struct segment *s, const f32 t); 	

/********************************** plane ***********************************/

/* construct plane with given normal n containing point p */
struct plane 	plane_construct(const vec3 n, const vec3 p); 
/* construct plane from CCW triangle abc */
struct plane 	plane_construct_from_ccw_triangle(const vec3 a, const vec3 b, const vec3 c);
/* return 1: If p is infront of plane, i.e. a positive signed distance, otherwise 0*/
u32 		plane_point_is_infront(const struct plane *pl, const vec3 p);
/* return 1: If p is behind plane, i.e. a negative signed distance, otherwise 0*/
u32 		plane_point_is_behind(const struct plane *pl, const vec3 p);
 /* return t: s.p0 + t*s.dir is point on plane */
f32 		plane_segment_clip_parameter(const struct plane *pl, const struct segment *s);
 /* return 1 if clip happened, otherwise 0. If 1, return valid clip point */
u32 		plane_segment_clip(vec3 clip, const struct plane *pl, const struct segment *s);
/* return 1 if clip happened, otherwise 0 */
u32 		plane_segment_test(const struct plane *pl, const struct segment *s); 
 /* return signed distance between plane and point (infront of plane == positive) */
f32 		plane_point_signed_distance(const struct plane *pl, const vec3 p);
 /* return absolute distance between plane and point */
f32 		plane_point_distance(const struct plane *pl, const vec3 p);
/* Return t such that ray->origin + t*ray->dir is a point on the given plane. If no such t exist, return F32_INFINITY. */
f32 		plane_raycast_parameter(const struct plane *plane, const struct ray *ray);
/* Return 1 if raycast hit plane, 0 otherwise. If hit, set intersection  */
u32 		plane_raycast(vec3 intersection, const struct plane *plane, const struct ray *ray);

/********************************** AABB ************************************/

/* Return smallest AABB with a given margin of the input vertex set,   */
void		AABB_vertex(struct AABB *dst, const vec3ptr v, const u32 v_count, const f32 margin);
/* Return smallest AABB that contains both a and b  */
void		AABB_union(struct AABB *box_union, const struct AABB *a, const struct AABB *b);
/* Return 1 if a and b intersect, 0 otherwise  */
u32 		AABB_test(const struct AABB *a, const struct AABB *b);
/* Return 1 if a fully contains b, 0 otherwise  */
u32 		AABB_contains(const struct AABB *a, const struct AABB *b);
/* If the ray hits aabb, return 1 and set intersection. otherwise return 0. */
u32 		AABB_raycast(vec3 intersection, const struct AABB *aabb, const struct ray *ray);

/********************************** dcel ************************************/

struct dcel_half_edge
{
	u32 	origin;	/* vertex index origin 			   */
	u32 	twin; 	/* twin half edge 			   */
	u32 	next;	/* next half edge in ccw traversal of face */
	u32 	prev;	/* prev half edge in ccw traversal of face */
};

/*
 * (Computational Geometry Algorithms and Applications, Section 2.2) 
 * dcel - doubly-connected edge list. Can represent convex 3d bodies (with no holes in polygons)
 * 	  and 2d planar graphs. A polygon in the data structure are implicitly defined by its 
 * 	  first half edge. 
 */
struct dcel
{
	const struct dcel_half_edge *	edge;			/* array[edge_count] 	    */
	const vec3ptr			vertex;			/* array[vertex_count] 	    */
	u32				edge_count;
	u32				vertex_count;
};

/* returns a dcel structure aliasing box data */
struct dcel	dcel_box(void);

/*TODO:
 * floating point utilities for checking max/min angles, vertex aliasing, max/min distances...
 */

/* debug assert dcel topology */
void		dcel_assert_topology(const struct dcel *dcel);


/* dcel_allocator: Dynamically allocates resources for dcel as required. */
struct dcel_allocator
{
	vec3ptr			v;
	struct dcel_half_edge *	he;
	u32 *			face;

	struct pool_external	v_pool;
	struct pool_external	half_edge_pool;
	struct pool_external	face_pool;
};

/********************************* vertex ***********************************/

/* Return: support of vertex set given the direction. */
void 		vertex_support(vec3 support, const vec3 dir, const vec3ptr v, const u32 v_count);

/******* TODO: Everything below this line should be re-evaluated... *********/

// TODO Cleanup dcel api

struct DCEL_half_edge {
	i32 he;		/* half edge */
	i32 origin;	/* vertex index origin */
	i32 twin; 	/* twin half edge */
	i32 face_ccw; 	/* face to the left of half edge */
	i32 next;	/* next half edge in ccw traversal of face_ccw */
	i32 prev;	/* prev half edge in ccw traversal of face_ccw */
};

/**
 * - If ccw face is in free chain, he_index == next free face index and relation_unit == -1 
 * - If face_index == related_to, the face has no current relations
 */
struct DCEL_face {
	i32 he_index; 
	i32 relation_unit;
};

/**
 * (Computational Geometry Algorithms and Applications, Section 2.2) 
 * DCEL - doubly-connected edge list. Can represent convex 3d bodies (with no holes in polygons)
 * 	  and 2d planar graphs.
 */
struct DCEL {
	struct DCEL_face *faces; 
	struct DCEL_half_edge *he_table; /* indexed table | free chain (next == next free) containing half edge information */
	i32 next_he; /* next free slot in edges, -1 == no memory left in free chain */
	i32 next_face; /* next free slot in edges, -1 == no memory left in free chain */
	i32 num_faces;
	i32 num_he;
};

#define DCEL_ALLOC_EDGES(dcel_ptr, table_arena_ptr, n) 							\
{													\
	struct DCEL_half_edge *new_table = (struct DCEL_half_edge *)arena_push_packed(table_arena_ptr, n*sizeof(struct DCEL_half_edge)); 	\
	new_table[n-1].next = (dcel_ptr)->next_he;							\
	(dcel_ptr)->next_he = (dcel_ptr)->num_he;							\
	for (i32 k = 0; k < n-1; ++k)									\
	{												\
		new_table[k].next = (dcel_ptr)->num_he + 1 + k;						\
	}												\
	assert(&((dcel_ptr)->he_table[(dcel_ptr)->next_he]) == new_table);				\
}													\
	(dcel_ptr)->num_he += n								

#define DCEL_ALLOC_FACES(dcel_ptr, faces_arena_ptr, n) 						\
{												\
	struct DCEL_face *new_faces = (struct DCEL_face *)arena_push_packed(faces_arena_ptr, n * sizeof(struct DCEL_face));	\
	new_faces[n-1].he_index = (dcel_ptr)->next_face;					\
	new_faces[n-1].relation_unit = -1;							\
	(dcel_ptr)->next_face = (dcel_ptr)->num_faces;						\
	for (i32 k = 0; k < n-1; ++k)								\
	{											\
		new_faces[k].he_index = (dcel_ptr)->num_faces + 1 + k;				\
		new_faces[k].relation_unit = -1;						\
	}											\
	assert(&((dcel_ptr)->faces[(dcel_ptr)->next_face]) == new_faces);			\
}												\
	(dcel_ptr)->num_faces += n;									

i32 DCEL_half_edge_add(struct DCEL *dcel, struct arena *table_mem, const i32 origin, const i32 twin, const i32 face_ccw, const i32 next, const i32 prev);
i32 DCEL_half_edge_reserve(struct DCEL *dcel, struct arena *table_mem);
void DCEL_half_edge_set(struct DCEL *dcel, const i32 he, const i32 origin, const i32 twin, const i32 face_ccw, const i32 next, const i32 prev);
void DCEL_half_edge_remove(struct DCEL *dcel, const i32 he);
/* returns face index */
i32 DCEL_face_add(struct DCEL *dcel, struct arena *face_mem, const i32 edge, const i32 unit);
void DCEL_face_remove(struct DCEL *dcel, const i32 face);

/****************************************************************************/

#define SIMPLEX_0	0
#define SIMPLEX_1	1
#define SIMPLEX_2	2
#define SIMPLEX_3	3

/**
 * Gilbert-Johnson-Keerthi intersection algorithm in 3D. Based on the original paper. 
 *
 * For understanding, see [ Collision Detection in Interactive 3D environments, chapter 4.3.1 - 4.3.8 ]
 */
struct gjk_simplex
{
	vec3 p[4];
	u64 id[4];
	f32 dot[4];
	u32 type;
};


u32 	GJK_test(const vec3 pos_1, vec3ptr vs_1, const u32 n_1, const vec3 pos_2, vec3ptr vs_2, const u32 n_2, const f32 abs_tol, const f32 tol); /* [Page 146] -1 on error (To few points, or no initial tetrahedron). 0 == no collision, 1 == collision. */
f32 	GJK_distance(vec3 c_1, vec3 c_2, const vec3 pos_1, vec3ptr vs_1, const u32 n_1, const vec3 pos_2, vec3ptr vs_2, const u32 n_2, const f32 rel_tol, const f32 abs_tol); /* Retrieve shortest distance between objects and the convex objects' closest points, or 0.0f if collision. */

void 	convex_centroid(vec3 centroid, vec3ptr vs, const u32 n);
u32 	convex_support(vec3 support, const vec3 dir, vec3ptr vs, const u32 n);
/* support of A-B, A,B convex */
u64 	convex_minkowski_difference_support(vec3 support, const vec3 dir, vec3ptr A, const u32 n_A, vec3ptr B, const u32 n_B);

/* Get indices for initial tetrahedron using given tolerance. return 0 on no initial tetrahedron, 1 otherwise. */
i32 	tetrahedron_indices(i32 indices[4], const vec3ptr v, const i32 v_count, const f32 tol);

/* Reorder (If necessary) triangle t such that it is CCW ([0] -> [1] -> [2] -> [0]) from p's point of view (switch [0] and [1]) */
void 	triangle_CCW_relative_to(vec3 BCA[3], const vec3 p);
void 	triangle_CCW_relative_to_origin(vec3 BCA[3]);
void 	triangle_CCW_normal(vec3 normal, const vec3 p0, const vec3 p1, const vec3 p2);

/**************************************************************/

struct OBB {
	vec3 center;
	vec3 hw;
	vec3 x_axis;
	vec3 z_axis;
};

struct cylinder {
	vec3 center;
	f32 radius;
	f32 half_height;
};

/* Shortest distance methods from point to given primitive (negative distance == behind plane) */
f32 point_plane_distance(const vec3 point, const struct plane *plane);
f32 point_plane_signed_distance(const vec3 point, const struct plane *plane);
f32 point_sphere_distance(const vec3 point, const struct sphere *sph);
f32 point_AABB_distance(const vec3 point, const struct AABB *aabb);
f32 point_OBB_distance(const vec3 point, const struct OBB *obb);
f32 point_cylinder_distance(const vec3 point, const struct cylinder *cyl);

f32 AABB_distance(const struct AABB *a, const struct AABB *b);
f32 OBB_distance(const struct OBB *a, const struct OBB *b);
//f32 sphere_distance(const struct sphere *a, const struct sphere *b);
f32 cylinder_distance(const struct cylinder *a, const struct cylinder *b);

i32 OBB_test(const struct OBB *a, const struct OBB *b);
//i32 sphere_test(const struct sphere *a, const struct sphere *b);
i32 cylinder_test(const struct cylinder *a, const struct cylinder *b);

i32 AABB_intersection(struct AABB *dst, const struct AABB *a, const struct AABB *b);
//i32 OBB_intersection(struct tmp *dst, const struct OBB *a, const struct OBB *b);
//i32 sphere_intersection(struct tmp *dst, const struct sphere *a, const struct sphere *b);
//i32 cylinder_intersection(struct tmp *dst, const struct cylinder *a, const struct cylinder *b);

/* Closest point on primitive to point */
void point_plane_closest_point(vec3 closest_point, const vec3 point, const struct plane *plane);
void point_sphere_closest_point(vec3 closest_point, const vec3 point, const struct sphere *sph);
void point_AABB_closest_point(vec3 closest_point, const vec3 point, const struct AABB *aabb);
void point_OBB_closest_point(vec3 closest_point, const vec3 point, const struct OBB *obb);
void point_cylinder_closest_point(vec3 closest_point, const vec3 point, const struct cylinder *cyl);

/* Intersection tests for ray primitive against given primitives, RETURN 0 == no intersect, 1 == intersect */
i32 ray_plane_intersection(vec3 intersection, const vec3 ray_origin, const vec3 ray_direction, const struct plane *plane);
i32 ray_sphere_intersection(vec3 intersection, const vec3 ray_origin, const vec3 ray_direction, const struct sphere * sph);
i32 ray_AABB_intersection(vec3 intersection, const vec3 ray_origin, const vec3 ray_direction, const struct AABB *abb);
i32 ray_OBB_intersection(vec3 intersection, const vec3 ray_origin, const vec3 ray_direction, const struct OBB *obb);
i32 ray_cylinder_intersection(vec3 intersection, const vec3 ray_origin, const vec3 ray_direction, const struct cylinder *cyl);

/* We make no assumption of CW or CCW ordering here, so not optimized */
u32 tetrahedron_point_test(const vec3 tetra[4], const vec3 p);

/* CCW!?? lambda is set to barocentric coordinates of the closest point */
f32 triangle_origin_closest_point(vec3 lambda, const vec3 A, const vec3 B, const vec3 C);

/* returns 0 if not internal, 1 if internal, and lambda is set to the barocentric coordinates of A,B,C */
u32 triangle_origin_closest_point_is_internal(vec3 lambda, const vec3 A, const vec3 B, const vec3 C);

#endif
