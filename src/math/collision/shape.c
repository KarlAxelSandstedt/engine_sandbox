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

#include "collision.h"
#include "hash_map.h"
#include "kas_random.h"
#include "sys_public.h"

/* two-way relation. next == -1 means this is the last relation in the chain. If
 * the entry is in the free chain, next is the next free entry, == -1 means no memory left. 
 */
struct relation_unit {
	i32 related_to; /* if head, index to outside, otherwise related unit */
	i32 next;
};

/**
 * static relationship graph. any relationships added by a node must be added when the node is added.
 * A node already in the graph may not create new relationships, and may only gain new relations when
 * a new node is added to the graph.
 */
struct relation_list {
	struct arena *mem;
	struct relation_unit *r;
	i32 free_chain_len;
	i32 free_chain;
	i32 len;
};

//TODO remove this shit 
/* initialize given number of units with no relations */
struct relation_list relation_list_init(struct arena *mem, const i32 num_relations);
void relation_list_free(struct relation_list *list);
/* Add empty relation_unit to relation list */
i32 relation_list_add_relation_unit_empty(struct relation_list *list, const i32 outside_index);
/* Assumes added relation is not already in relationship with the unit and that unit is in list */
void relation_list_add_to_relation_unit(struct relation_list *list, const i32 unit, const i32 relation);
/* add unit with outside index and the given relations */
i32 relation_list_add_relation_unit(struct relation_list *list, const i32 outside_index, const i32 *relations, const i32 num_relations);
/* Assumes unit exists in list */
void relation_list_remove_relation_unit(struct relation_list *list, const i32 unit);
/* Assumes unit exists in list and is related to relation */
void relation_list_internal_remove_from_relation_unit(struct relation_list *list, const i32 unit, const i32 relation);
/* Returns 1 if u1, u2 related, 0 otherwise. Does not check if u1 or u2 is within bounds or in free chain */
i32 relation_list_is_related(const struct relation_list *list, const i32 u1, const i32 u2);
/* Returns number of units in both u1 and u2's relations. NOTE that duplicates may occur. the union is pushed onto mem */
/* copy relation to unit, does not check if relations already exist */
void relation_list_copy_relations(struct relation_list *list, const i32 copy_to, const i32 copy_from);
i32 relation_list_push_union(struct arena *mem, const struct relation_list *list, const i32 u1, const i32 u2);

struct relation_list relation_list_init(struct arena *mem, const i32 num_relations)
{
	struct relation_list list =
	{
		.mem = mem,
		.free_chain = -1,
		.free_chain_len = 0,
		.len = 0,
		.r = NULL,
	};

	if (num_relations > 0)
	{
		list.len = num_relations;
		list.r = arena_push_packed(mem, num_relations * sizeof(struct relation_unit));
		for (i32 i = 0; i < num_relations; ++i)
	       	{ 
			list.r[i].related_to = i;
			list.r[i].next = -1;
	       	}
	}

	return list;
}

void relation_list_free(struct relation_list *list)
{
	arena_pop_packed(list->mem, list->len * sizeof(struct relation_unit));
	assert((u8 *) list->r == list->mem->stack_ptr);
}

i32 relation_list_add_relation_unit_empty(struct relation_list *list, const i32 outside_index)
{
	i32 unit;
	if (list->free_chain_len > 0)
	{
		unit = list->free_chain;
		list->free_chain = list->r[unit].next;
		list->free_chain_len -= 1;
	}
	else
	{
		unit = list->len;
		list->len += 1;
		arena_push_packed(list->mem, sizeof(struct relation_unit));
	}

	list->r[unit].related_to = outside_index;
	list->r[unit].next = -1;

	return unit;
}

i32 relation_list_is_related(const struct relation_list *list, const i32 u1, const i32 u2)
{
	assert(u1 >= 0 && u2 >= 0 && u1 < list->len && u2 < list->len);
	
	for (i32 i = list->r[u1].next; i != -1; i = list->r[i].next)
	{
		if (list->r[i].related_to == u2) { return 1; }
	}

	return 0;
}

i32 relation_list_push_union(struct arena *mem, const struct relation_list *list, const i32 u1, const i32 u2)
{
	assert(0 <= u1 && u1 < list->len);
	assert(0 <= u2 && u2 < list->len);

	i32 union_len = 0;

	for (i32 i = list->r[u1].next; i != -1; i = list->r[i].next) 
	{ 
		arena_push_packed_memcpy(mem, &list->r[i].related_to, sizeof(i32));
		union_len += 1;
	}

	for (i32 i = list->r[u2].next; i != -1; i = list->r[i].next) 
	{ 
		arena_push_packed_memcpy(mem, &list->r[i].related_to, sizeof(i32));
		union_len += 1;
	}

	return union_len;
}

i32 relation_list_add_relation_unit(struct relation_list *list, const i32 outside_index, const i32 *relations, const i32 num_relations)
{
	const i32 unit = relation_list_add_relation_unit_empty(list, outside_index);

	if (num_relations > 0)
	{
		i32 added = 0;
		if (list->free_chain_len < num_relations)
		{
			added = num_relations - list->free_chain_len;
			arena_push_packed(list->mem, added * sizeof(struct relation_unit));
			i32 i = list->len;
			const i32 old_chain = list->free_chain;

			list->len += added;
			list->free_chain = i;
			list->free_chain_len += added;
			for (; i < list->len-1; ++i)
			{
				list->r[i].next = i+1;			
			}
			list->r[list->len-1].next = old_chain;
		}
	
		//list->r[unit].next = list->free_chain;
		//u32 last = list->free_chain;
		//for (i32 i = 0; i < num_relations; ++i)
		//{
		//	list->r[list->free_chain].related_to = relations[i];
		//	last = list->free_chain;
		//	list->free_chain = list->r[last].next;
		//}
		//list->r[last].next = -1;
		//list->free_chain_len -= num_relations;
		//assert(list->free_chain == -1 || list->free_chain_len > 0);

		for (i32 i = 0; i < num_relations; ++i)
		{
			assert(0 <= relations[i] && relations[i] < list->len);
			//relation_list_add_to_relation_unit(list, relations[i], unit);
			relation_list_add_to_relation_unit(list, unit, relations[i]);
		}

		//if (list->free_chain_len < num_relations)
		//{
		//	added = num_relations - list->free_chain_len;
		//	arena_push_packed(list->mem, NULL, added * sizeof(struct relation_unit));
		//}
	
		//i32 chain = list->free_chain;
		//if (chain == -1) { chain = list->len; }
		//list->r[unit].next = chain;

		//i32 i = 0;
		//i32 prev = list->free_chain;
		//for (; i < list->free_chain_len; ++i)
		//{
		//	list->r[list->free_chain].related_to = relations[i];
		//	prev = list->free_chain;
		//	list->free_chain = list->r[prev].next;
		//}
	
		//for (i32 j = 0; j < added; ++j)
		//{
		//	kas_assert(list->free_chain_len > 0);
		//	list->r[prev].next = list->len + j;
		//	prev = list->len + j;
		//	list->r[list->len + j].related_to = relations[i];
		//	i += 1;
		//}
	
		//assert(list->free_chain == -1 || list->free_chain_len > num_relations);
	
		//list->r[prev].next = -1;
		//list->free_chain_len -= num_relations - added;
		//list->len += added;
	
		//for (i32 j = 0; j < num_relations; ++j)
		//{
		//	assert(0 <= relations[j] && relations[j] < list->len);
		//	relation_list_add_to_relation_unit(list, relations[j], unit);
		//}
	}

	return unit;
}

void relation_list_copy_relations(struct relation_list *list, const i32 copy_to, const i32 copy_from)
{
	assert(0 <= copy_to && copy_to < list->len);
	assert(0 <= copy_from && copy_from < list->len);

	for (i32 unit = list->r[copy_from].next; unit != -1; unit = list->r[unit].next)
	{
		relation_list_add_to_relation_unit(list, copy_to, list->r[unit].related_to);
	}
}

void relation_list_add_to_relation_unit(struct relation_list *list, const i32 unit, const i32 relation)
{
	assert(0 <= unit && unit < list->len);
	assert(0 <= relation && relation < list->len);

	const i32 tmp = list->r[unit].next;
	if (list->free_chain_len > 0)
	{
		const i32 i = list->free_chain;
		list->r[unit].next = i;
		list->free_chain = list->r[i].next;
		list->r[i].next = tmp;
		list->r[i].related_to = relation;
		list->free_chain_len -= 1;
	}
	else
	{
		arena_push_packed(list->mem, sizeof(struct relation_unit));
		list->r[unit].next = list->len;
		list->r[list->len].next = tmp;
		list->r[list->len].related_to = relation; 
		list->len += 1;
	}
}

void relation_list_remove_relation_unit(struct relation_list *list, const i32 unit)
{
	assert(0 <= unit && unit < list->len);
	if (list->r[unit].related_to == -1) { return; }
	list->r[unit].related_to = -1;

	i32 end = unit;
	i32 chain_len = 1;
	while (list->r[end].next != -1)
	{
		end = list->r[end].next;
		relation_list_internal_remove_from_relation_unit(list, list->r[end].related_to, unit);
		chain_len += 1;
	}

	list->r[end].next = list->free_chain;
	list->free_chain = unit;
	list->free_chain_len += chain_len;
}

void relation_list_internal_remove_from_relation_unit(struct relation_list *list, const i32 unit, const i32 relation)
{
	assert(0 <= unit && unit < list->len);
	assert(0 <= relation && relation < list->len);

	i32 prev = unit;
	for (i32 i = list->r[unit].next; i != -1; i = list->r[i].next)
	{
		if (list->r[i].related_to == relation)
		{
			const i32 tmp = list->r[i].next;
			list->r[i].related_to = -1;
			list->r[i].next = list->free_chain;
			list->free_chain = i;
			list->free_chain_len += 1;
			list->r[prev].next = tmp;
			return;
		}
		prev = i;
	}

	assert(0 && "tried to delete relation from unit that does not exist!");
}

static struct hull_face box_face[] =
{
	{ .first  =  0, .count = 4 },
	{ .first  =  4, .count = 4 },
	{ .first  =  8, .count = 4 },
	{ .first  = 12, .count = 4 },
	{ .first  = 16, .count = 4 },
	{ .first  = 20, .count = 4 },
};

static struct hull_half_edge box_edge[] =
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

struct collision_hull collision_box(struct arena *mem, const vec3 hw)
{
	vec3ptr box_vertex = arena_push(mem, 8*sizeof(vec3));

	vec3_set(box_vertex[0],  hw[0],  hw[1], -hw[2]); 
	vec3_set(box_vertex[1],  hw[0],  hw[1],  hw[2]);	
	vec3_set(box_vertex[2], -hw[0],  hw[1],  hw[2]);	
	vec3_set(box_vertex[3], -hw[0],  hw[1], -hw[2]);	
	vec3_set(box_vertex[4],  hw[0], -hw[1], -hw[2]);
	vec3_set(box_vertex[5],  hw[0], -hw[1],  hw[2]);	
	vec3_set(box_vertex[6], -hw[0], -hw[1],  hw[2]);	
	vec3_set(box_vertex[7], -hw[0], -hw[1], -hw[2]);	

	struct collision_hull box = 
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
void collision_hull_face_direction(vec3 dir, const struct collision_hull *h, const u32 fi)
{
	vec3 a, b;
	struct hull_half_edge *e0 = h->e + h->f[fi].first;
	struct hull_half_edge *e1 = h->e + h->f[fi].first + 1;
	struct hull_half_edge *e2 = h->e + h->f[fi].first + 2;
	vec3_sub(a, h->v[e1->origin], h->v[e0->origin]);
	vec3_sub(b, h->v[e2->origin], h->v[e0->origin]);
	vec3_cross(dir, a, b);
	assert(vec3_length(dir) >= 100.0f*F32_EPSILON);
}

void collision_hull_face_normal(vec3 normal, const struct collision_hull *h, const u32 fi)
{
	collision_hull_face_direction(normal, h, fi);
	vec3_mul_constant(normal, 1.0f/vec3_length(normal));	
}

struct plane collision_hull_face_plane(const struct collision_hull *h, mat3 rot, const vec3 pos, const u32 fi)
{
	vec3 n, p;
	collision_hull_face_normal(p, h, fi);
	mat3_vec_mul(n, rot, p);
	mat3_vec_mul(p, rot, h->v[h->e[h->f[fi].first].origin]);
	vec3_translate(p, pos);
	return plane_construct(n, p);
}

struct segment collision_hull_face_clip_segment(const struct collision_hull *h, mat3 rot, const vec3 pos, const u32 fi, const struct segment *s)
{
	vec3 f_n, p_n, p_p0, p_p1;

	collision_hull_face_normal(p_n, h, fi);
	mat3_vec_mul(f_n, rot, p_n);

	f32 min_p = 0.0f;
	f32 max_p = 1.0f;

	struct hull_face *f = h->f + fi;
	for (u32 i = 0; i < f->count; ++i)
	{
		const u32 e0 = f->first + i;
		const u32 e1 = f->first + ((i + 1) % f->count);
		struct plane clip_plane = collision_hull_face_clip_plane(h, rot, pos, f_n, e0, e1);

		//struct hull_half_edge *e0 = h->e + f->first + i;
		//struct hull_half_edge *e1 = h->e + f->first + ((i + 1) % f->count);

		//mat3_vec_mul(p_p0, rot, h->v[e0->origin]);
		//mat3_vec_mul(p_p1, rot, h->v[e1->origin]);
		//vec3_translate(p_p0, pos);
		//vec3_translate(p_p1, pos);
		//vec3_sub(diff, p_p1, p_p0);
		//vec3_cross(p_n, diff, f_n);

		//struct plane clip_plane = plane_construct(p_n, p_p0);
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

struct plane collision_hull_face_clip_plane(const struct collision_hull *h, mat3 rot, const vec3 pos, const vec3 face_normal, const u32 e0, const u32 e1)
{
	vec3 diff, p0, p1;
	struct hull_half_edge *edge0 = h->e + e0; 
	struct hull_half_edge *edge1 = h->e + e1; 

	mat3_vec_mul(p0, rot, h->v[edge0->origin]);
	mat3_vec_mul(p1, rot, h->v[edge1->origin]);
	vec3_translate(p0, pos);
	vec3_translate(p1, pos);
	vec3_sub(diff, p1, p0);
	vec3_cross(p1, diff, face_normal);
	vec3_mul_constant(p1, 1.0f/vec3_length(p1));

	return plane_construct(p1, p0);
}

u32 collision_hull_face_projected_point_test(const struct collision_hull *h, mat3 rot, const vec3 pos, const u32 fi, const vec3 p)
{
	vec3 f_n, p_n;

	collision_hull_face_normal(p_n, h, fi);
	mat3_vec_mul(f_n, rot, p_n);

	f32 min_p = 0.0f;
	f32 max_p = 1.0f;

	struct hull_face *f = h->f + fi;
	for (u32 i = 0; i < f->count; ++i)
	{
		const u32 e0 = f->first + i;
		const u32 e1 = f->first + ((i + 1) % f->count);
		struct plane clip_plane = collision_hull_face_clip_plane(h, rot, pos, f_n, e0, e1);
		if (vec3_dot(clip_plane.normal, p) > clip_plane.signed_distance)
		{
			return 0;
		}
	}	

	return 1;
}

void collision_hull_half_edge_direction(vec3 dir, const struct collision_hull *h, const u32 ei)
{
	struct hull_half_edge *e0 = h->e + ei;
	struct hull_face *f = h->f + e0->face_ccw;
	const u32 next = f->first + ((ei - f->first + 1) % f->count);
	struct hull_half_edge *e1 = h->e + next;
	vec3_sub(dir, h->v[e1->origin], h->v[e0->origin]);
	assert(vec3_length(dir) >= 100.0f*F32_EPSILON);
}

void collision_hull_half_edge_normal(vec3 dir, const struct collision_hull *h, const u32 ei)
{
	collision_hull_half_edge_direction(dir, h, ei);
	vec3_mul_constant(dir, 1.0f / vec3_length(dir));
}

struct segment collision_hull_half_edge_segment(const struct collision_hull *h, mat3 rot, const vec3 pos, const u32 ei)
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

void sphere_world_support(vec3 support, const vec3 dir, const struct collision_sphere *sph, const vec3 pos)
{
	vec3_scale(support, dir, sph->radius / vec3_length(dir));
	vec3_translate(support, pos);
}

void capsule_world_support(vec3 support, const vec3 dir, const struct collision_capsule *cap, mat3 rot, const vec3 pos)
{
	vec3 p1, p2;
	vec3_copy(p2, cap->p1);
	mat3_vec_mul(p1, rot, p2);	
	vec3_negative_to(p2, p1);
	
	vec3_scale(support, dir, cap->radius / vec3_length(dir));
	vec3_translate(support, pos);
	(vec3_dot(dir, p1) > vec3_dot(dir, p2))
		? vec3_translate(support, p1) 
		: vec3_translate(support, p2);
}

u64 collision_hull_world_support(vec3 support, const vec3 dir, const struct collision_hull *hull, mat3 rot, const vec3 pos)
{
	f32 max = -FLT_MAX;
	u64 max_index = 0;
	vec3 p;
	for (u32 i = 0; i < hull->v_count; ++i)
	{
		mat3_vec_mul(p, rot, hull->v[i]);
		const f32 dot = vec3_dot(p, dir);
		if (max < dot)
		{
			max_index = i;
			max = dot; 
		}
	}

	mat3_vec_mul(support, rot, hull->v[max_index]);
	vec3_translate(support, pos);
	return max_index;
}

struct collision_hull collision_hull_empty(void)
{
	struct collision_hull hull = { 0 };

	return hull;
}

static struct DCEL convex_hull_internal_setup_tetrahedron_DCEL(struct arena *table_mem, struct arena *face_mem, const i32 init_i[4], vec3ptr v)
{
	struct DCEL dcel =
	{ 
		.next_he = -1,
		.next_face = -1,
		.num_he = 0,
		.num_faces = 0,
	};

	dcel.faces = (struct DCEL_face *) face_mem->stack_ptr;
	dcel.he_table = (struct DCEL_half_edge *) table_mem->stack_ptr;
	DCEL_ALLOC_EDGES(&dcel, table_mem, 12);
	
	const vec3 inside =
	{
		0.25f * (v[init_i[0]][0] + v[init_i[1]][0] + v[init_i[2]][0] + v[init_i[3]][0]) - v[0][0],
		0.25f * (v[init_i[0]][1] + v[init_i[1]][1] + v[init_i[2]][1] + v[init_i[3]][1]) - v[0][1],
		0.25f * (v[init_i[0]][2] + v[init_i[1]][2] + v[init_i[2]][2] + v[init_i[3]][2]) - v[0][2],
	};

	vec3 a, b, c, d, cr;
	vec3_sub(a, v[0], v[0]);
	vec3_sub(b, v[init_i[1]], v[0]);
	vec3_sub(c, v[init_i[2]], v[0]);
	vec3_sub(d, v[init_i[3]], v[0]);
	vec3_cross(cr, b, c);

	/* CCW == inside gives negative dot product for any polygon on a convex polyhedron */
	if (vec3_dot(cr, inside) < 0.0f)
	{
		/* a -> b -> c */	
		i32 face = DCEL_face_add(&dcel, face_mem, 0, -1);
		DCEL_half_edge_add(&dcel, table_mem, init_i[0], 3, face, 1, 2);
		DCEL_half_edge_add(&dcel, table_mem, init_i[1], 6, face, 2, 0);
		DCEL_half_edge_add(&dcel, table_mem, init_i[2], 9, face, 0, 1);

		/* b -> a -> d */
		face = DCEL_face_add(&dcel, face_mem, 3, -1);
		DCEL_half_edge_add(&dcel, table_mem, init_i[1], 0,  face, 4, 5);
		DCEL_half_edge_add(&dcel, table_mem, init_i[0], 11, face, 5, 3);
		DCEL_half_edge_add(&dcel, table_mem, init_i[3], 7,  face, 3, 4);

		/* c -> b -> d */
		face = DCEL_face_add(&dcel, face_mem, 6, -1);
		DCEL_half_edge_add(&dcel, table_mem, init_i[2], 1, face, 7, 8);
		DCEL_half_edge_add(&dcel, table_mem, init_i[1], 5, face, 8, 6);
		DCEL_half_edge_add(&dcel, table_mem, init_i[3], 10, face, 6, 7);

		/* a -> c -> d */
		face = DCEL_face_add(&dcel, face_mem, 9, -1);
		DCEL_half_edge_add(&dcel, table_mem, init_i[0], 2, face, 10, 11);
		DCEL_half_edge_add(&dcel, table_mem, init_i[2], 8, face, 11, 9);
		DCEL_half_edge_add(&dcel, table_mem, init_i[3], 4, face, 9, 10);
	}
	else
	{
		/* a -> c -> b */
		i32 face = DCEL_face_add(&dcel, face_mem, 0, -1);
		DCEL_half_edge_add(&dcel, table_mem, init_i[0], 3, face, 1, 2);
		DCEL_half_edge_add(&dcel, table_mem, init_i[2], 6, face, 2, 0);
		DCEL_half_edge_add(&dcel, table_mem, init_i[1], 9, face, 0, 1);
	
		/* c -> a -> d */
		face = DCEL_face_add(&dcel, face_mem, 3, -1);
		DCEL_half_edge_add(&dcel, table_mem, init_i[2], 0,  face, 4, 5);
		DCEL_half_edge_add(&dcel, table_mem, init_i[0], 11, face, 5, 3);
		DCEL_half_edge_add(&dcel, table_mem, init_i[3], 7,  face, 3, 4);

		/* b -> c -> d */
		face = DCEL_face_add(&dcel, face_mem, 6, -1);
		DCEL_half_edge_add(&dcel, table_mem, init_i[1], 1, face, 7, 8);
		DCEL_half_edge_add(&dcel, table_mem, init_i[2], 5, face, 8, 6);
		DCEL_half_edge_add(&dcel, table_mem, init_i[3], 10, face, 6, 7);

		/* a -> b -> d */
		face = DCEL_face_add(&dcel, face_mem, 9, -1);
		DCEL_half_edge_add(&dcel, table_mem, init_i[0], 2, face, 10, 11);
		DCEL_half_edge_add(&dcel, table_mem, init_i[1], 8, face, 11, 9);
		DCEL_half_edge_add(&dcel, table_mem, init_i[3], 4, face, 9, 10);
	}

	return dcel;
}

static struct relation_list convex_hull_internal_tetrahedron_conflicts(struct DCEL *dcel, struct arena *conflict_mem, const i32 *permutation, struct arena *stack, const vec3ptr v, const i32 v_count, const f32 EPSILON)
{
	struct relation_list conflict_graph = relation_list_init(conflict_mem, v_count);
	for (i32 i = 0; i < v_count; ++i) { relation_list_add_relation_unit_empty(&conflict_graph, i); }

	vec3 b, c, cr;

	for (i32 face = 0; face < 4; ++face)
	{
		const i32 a_i = dcel->he_table[3*face + 0].origin;
		const i32 b_i = dcel->he_table[3*face + 1].origin;
		const i32 c_i = dcel->he_table[3*face + 2].origin;

		/* a -> b -> c, CCW, cross points outwards  */
		vec3_sub(b, v[b_i], v[a_i]);
		vec3_sub(c, v[c_i], v[a_i]);
		vec3_cross(cr, b, c);
		vec3_mul_constant(cr, 1.0f / vec3_length(cr));

		i32 *conflicts = (i32 *) stack->stack_ptr;
		i32 num_conflicts = 0;
		for (i32 v_i = 4; v_i < v_count; ++v_i)
		{
			const i32 index = permutation[v_i];
			vec3_sub(b, v[index], v[a_i]);
			/* If point is "in front" of face, we have a conflict */
			if (vec3_dot(cr, b) > EPSILON)
			{
				arena_push_packed_memcpy(stack, &v_i, sizeof(i32));
				num_conflicts += 1;
			}
		}

		if (num_conflicts)  
		{ 
			dcel->faces[face].relation_unit = relation_list_add_relation_unit(&conflict_graph, face, conflicts, num_conflicts); 
			for (i32 ti = 0; ti < num_conflicts; ++ti)
			{
				relation_list_add_to_relation_unit(&conflict_graph, conflicts[ti], dcel->faces[face].relation_unit);
			}
		}
		else
		{
			dcel->faces[face].relation_unit = relation_list_add_relation_unit_empty(&conflict_graph, face); 
		}
			
		assert(face == conflict_graph.r[dcel->faces[face].relation_unit].related_to);
		
		/* pop conflict indices */
		arena_pop_packed(stack, num_conflicts * sizeof(i32));
	}

	return conflict_graph;
}

static void convex_hull_internal_random_permutation(i32 *permutation, const i32 indices[4], const i32 num_vs)
{
	for (i32 i = 0; i < num_vs; ++i) { permutation[i] = i; }
	permutation[0] = indices[0];
	permutation[1] = indices[1];
	permutation[2] = indices[2];
	permutation[3] = indices[3];
	permutation[indices[0]] = 0;
	permutation[indices[1]] = 1;
	permutation[indices[2]] = 2;
	permutation[indices[3]] = 3;

	for (i32 i = 4; i < num_vs; ++i)
	{
		const i32 r = (i32) rng_f32_range((f32) i, ((f32) num_vs) - 0.0001f);
		const i32 tmp = permutation[i];
		permutation[i] = permutation[r];
		permutation[r] = tmp;
	}
}

/* returns new face relation_unit, MAKE SURE ADDED VERTICES ARRAY contain enough memory  */
void convex_hull_internal_add_possible_conflicts(const i32 *permutation, struct relation_list *conflict_graph, u32 *added_vertices, struct arena *mem, const i32 face_unit, const u32 num_possible_conflicts, const i32 *possible_conflicts, const vec3 origin, const vec3 normal, const vec3ptr v, const f32 EPSILON)
{
	vec3 p;
	i32 num_added = 0;
	for (u32 i = 0; i < num_possible_conflicts; ++i)
	{
		if (added_vertices[possible_conflicts[i]] == U32_MAX)
		{
			vec3_sub(p, v[permutation[possible_conflicts[i]]], origin);
			const f32 dot = vec3_dot(normal, p);

			if (dot > EPSILON)
			{
				num_added += 1;
				arena_push_packed_memcpy(mem, &possible_conflicts[i], sizeof(i32));
				added_vertices[possible_conflicts[i]] = possible_conflicts[i];
				relation_list_add_to_relation_unit(conflict_graph, face_unit, possible_conflicts[i]);
				relation_list_add_to_relation_unit(conflict_graph, possible_conflicts[i], face_unit);
			}
		}
	}

	/* reset array */
	i32 *ptr = ((i32 *) mem->stack_ptr) - num_added;
	for (i32 i = 0; i < num_added; ++i)
	{
		added_vertices[ptr[i]] = -1;
	}
	arena_pop_packed(mem, num_added * sizeof(i32));
}

static i32 convex_hull_internal_push_conflict_faces(struct DCEL *dcel, const struct relation_list *conflict_graph, struct hash_map *horizon_map, struct arena *mem_1, struct arena *mem_2, const i32 v)
{
	i32 num_conflict_faces = 0;
	i32 *edges_to_remove = (i32 *) arena_push_packed(mem_2, sizeof(i32));
	*edges_to_remove = 0;

	/* Delete all faces that conflict with the vertex */
	for (i32 i = conflict_graph->r[v].next; i != -1; i = conflict_graph->r[i].next)
	{
		/* (5) Keep track of visibility horizion */
		const i32 face_unit = conflict_graph->r[i].related_to;
		const i32 conflict_face = conflict_graph->r[face_unit].related_to;
		const i32 start = dcel->faces[conflict_face].he_index;

		i32 edge = start;
		while (1)
		{
			/* add / remove edge (non removed edges will become horizon */
			const u32 key = dcel->he_table[edge].origin;
			const u32 twin = dcel->he_table[edge].twin;
			const u32 key_twin = dcel->he_table[twin].origin;
			i32 twin_in = 0;
			for (u32 index = hash_map_first(horizon_map, key_twin); index != HASH_NULL; index = hash_map_next(horizon_map, index))
			{
				if (index == twin)
				{
					twin_in = 1;
					break;
				}
			}

			const i32 tmp = dcel->he_table[edge].next;
			if (twin_in) 
			{ 
				hash_map_remove(horizon_map, key_twin, twin); 
				*edges_to_remove += 2;
				arena_push_packed_memcpy(mem_2, &twin, sizeof(i32));
				arena_push_packed_memcpy(mem_2, &edge, sizeof(i32));
			}
			else 
			{ 
				hash_map_add(horizon_map, key, edge); 
			}

			edge = tmp;
			if (edge == start) 
			{ 
				break;
		       	}
		}

		num_conflict_faces += 1;
		arena_push_packed_memcpy(mem_1, &conflict_face, sizeof(i32));
	}
	
	return num_conflict_faces;
}

static void convex_hull_internal_DCEL_add_coplanar(struct DCEL *dcel, const i32 horizon_edge_1, const i32 horizon_edge_2, const i32 last_edge, const i32 prev_edge)
{
	const i32 twin_1 = dcel->he_table[horizon_edge_1].twin;
	const i32 twin_2 = dcel->he_table[horizon_edge_2].twin;
	assert(dcel->he_table[twin_1].face_ccw == dcel->he_table[twin_2].face_ccw);

	/* connect new face edges and planar neighbor face edges  */
	dcel->he_table[last_edge].next = dcel->he_table[twin_1].next;
	dcel->he_table[prev_edge].prev = dcel->he_table[twin_2].prev;

	dcel->he_table[dcel->he_table[twin_1].next].prev = last_edge; 
	dcel->he_table[dcel->he_table[twin_2].prev].next = prev_edge;
}

void hull_add_edge_and_vertex(struct collision_hull *hull, u32 *ei, u32 *vi, u32 *dcel_he_to_he, u32 *dcel_v_to_v, struct DCEL *dcel, vec3ptr vs, const u32 he_index, const u32 fi)
{
	struct DCEL_half_edge *he = dcel->he_table + he_index;

	/* (2) add first edge to hull */
	hull->e[*ei].face_ccw = fi;

	// vertex not yet added 
	if (dcel_v_to_v[he->origin] == UINT32_MAX)
	{
		dcel_v_to_v[he->origin] = *vi;
		vec3_copy(hull->v[*vi], vs[he->origin]);
		*vi += 1;
	}
	hull->e[*ei].origin = dcel_v_to_v[he->origin];

	// vertex not yet added 
	const u32 hull_twin = dcel_he_to_he[he->twin];
	if (hull_twin == UINT32_MAX)
	{
		dcel_he_to_he[he_index] = *ei;
	}
	else
	{
		hull->e[*ei].twin = hull_twin;
		hull->e[hull_twin].twin = *ei;
	}
	*ei += 1;
}

void collision_hull_assert(struct collision_hull *hull)
{
	struct hull_face *f;
	struct hull_half_edge *e;
	for (u32 i = 0; i < hull->f_count; ++i)
	{
		f = hull->f + i;
		e = hull->e + f->first;
		for (u32 j = 0; j < f->count; ++j)
		{
			assert(e->face_ccw == i);
 			e = hull->e + f->first + j + 1;
		}

		if (f->first + f->count < hull->e_count)
		{
			assert(e->face_ccw != i);
		}
	}

	for (u32 i = 0; i < hull->e_count; ++i)
	{
		e = hull->e + i;
		assert(i == (hull->e + e->twin)->twin);
	}
}

