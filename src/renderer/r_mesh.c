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

#include "r_local.h"

f32 stub_vertices[] =
{
		 0.5f, -0.5f, -0.5f, /* 0.8f, 0.5f, 0.1f, 1.0f, */0.0f, 0.0f, -1.0f, 
		-0.5f, -0.5f, -0.5f, /* 0.8f, 0.5f, 0.1f, 1.0f, */0.0f, 0.0f, -1.0f,
		-0.5f,  0.5f, -0.5f, /* 0.8f, 0.5f, 0.1f, 1.0f, */0.0f, 0.0f, -1.0f,
		 0.5f,  0.5f, -0.5f, /* 0.8f, 0.5f, 0.1f, 1.0f, */0.0f, 0.0f, -1.0f,
		-0.5f, -0.5f,  0.5f, /* 0.8f, 0.5f, 0.1f, 1.0f, */0.0f, 0.0f,  1.0f,
		 0.5f, -0.5f,  0.5f, /* 0.8f, 0.5f, 0.1f, 1.0f, */0.0f, 0.0f,  1.0f,
		 0.5f,  0.5f,  0.5f, /* 0.8f, 0.5f, 0.1f, 1.0f, */0.0f, 0.0f,  1.0f,
		-0.5f,  0.5f,  0.5f, /* 0.8f, 0.5f, 0.1f, 1.0f, */0.0f, 0.0f,  1.0f,

		 0.5f, -0.5f, -0.5f, /* 0.8f, 0.5f, 0.1f, 1.0f, */0.0f, -1.0f, 0.0f, 
		-0.5f, -0.5f, -0.5f, /* 0.8f, 0.5f, 0.1f, 1.0f, */0.0f, -1.0f, 0.0f,
		-0.5f,  0.5f, -0.5f, /* 0.8f, 0.5f, 0.1f, 1.0f, */0.0f,  1.0f, 0.0f,
		 0.5f,  0.5f, -0.5f, /* 0.8f, 0.5f, 0.1f, 1.0f, */0.0f,  1.0f, 0.0f,
		-0.5f, -0.5f,  0.5f, /* 0.8f, 0.5f, 0.1f, 1.0f, */0.0f, -1.0f, 0.0f,
		 0.5f, -0.5f,  0.5f, /* 0.8f, 0.5f, 0.1f, 1.0f, */0.0f, -1.0f, 0.0f,
		 0.5f,  0.5f,  0.5f, /* 0.8f, 0.5f, 0.1f, 1.0f, */0.0f,  1.0f, 0.0f,
		-0.5f,  0.5f,  0.5f, /* 0.8f, 0.5f, 0.1f, 1.0f, */0.0f,  1.0f, 0.0f,

		 0.5f, -0.5f, -0.5f, /* 0.8f, 0.5f, 0.1f, 1.0f, */ 1.0f, 0.0f, 0.0f, 
		-0.5f, -0.5f, -0.5f, /* 0.8f, 0.5f, 0.1f, 1.0f, */-1.0f, 0.0f, 0.0f,
		-0.5f,  0.5f, -0.5f, /* 0.8f, 0.5f, 0.1f, 1.0f, */-1.0f, 0.0f, 0.0f,
		 0.5f,  0.5f, -0.5f, /* 0.8f, 0.5f, 0.1f, 1.0f, */ 1.0f, 0.0f, 0.0f,
		-0.5f, -0.5f,  0.5f, /* 0.8f, 0.5f, 0.1f, 1.0f, */-1.0f, 0.0f, 0.0f,
		 0.5f, -0.5f,  0.5f, /* 0.8f, 0.5f, 0.1f, 1.0f, */ 1.0f, 0.0f, 0.0f,
		 0.5f,  0.5f,  0.5f, /* 0.8f, 0.5f, 0.1f, 1.0f, */ 1.0f, 0.0f, 0.0f,
		-0.5f,  0.5f,  0.5f, /* 0.8f, 0.5f, 0.1f, 1.0f, */-1.0f, 0.0f, 0.0f,
};

u32 stub_indices[] = 
{
	0, 1, 2, 0, 2, 3,
	6, 7, 4, 6, 4, 5,
	8 + 3, 8 + 2, 8 + 7, 8 + 3, 8 + 7, 8 + 6,
	8 + 5, 8 + 4, 8 + 1, 8 + 5, 8 + 1, 8 + 0,
	16 + 5, 16 + 0, 16 + 3, 16 + 5, 16 + 3, 16 + 6,
	16 + 1, 16 + 4, 16 + 7, 16 + 1, 16 + 7, 16 + 2,
};

void r_mesh_set_stub_box(struct r_mesh *mesh_stub)
{
	mesh_stub->attribute_flags = R_MESH_ATTRIBUTE_POSITION /* | R_MESH_ATTRIBUTE_COLOR */ | R_MESH_ATTRIBUTE_NORMAL;
	mesh_stub->index_max_used = 16 + 7;
	mesh_stub->index_count = sizeof(stub_indices) / sizeof(stub_indices[0]); 
	mesh_stub->index_data = stub_indices;
	mesh_stub->vertex_count = sizeof(stub_vertices) / sizeof(stub_vertices[0]);
	mesh_stub->vertex_data = stub_vertices;
}

static void internal_r_mesh_set_sphere(u32 *b_i, u8 *vertex_data, u32 *index_data, const f32 radius, const vec3 translation, const u32 refinement/*, const vec4 color*/)
{
	const u32 points_per_strip = 2 * refinement;
	const u32 num_strips = refinement;
	const f32 inc_angle = MM_PI_F / refinement;

	vec3 tmp, vertex = { 0.0f, radius, 0.0f };
	vec3_translate(vertex, translation);
	vec3 n = { 0.0f, 1.0f, 0.0f };

	u64 offset = 0;
	const u64 p_offset = 0;
	/* const u64 c_offset = sizeof(vec3); */
	const u64 n_offset = sizeof(vec3) /* + sizeof(vec4)*/;

	memcpy(vertex_data + offset + p_offset, vertex, sizeof(vec3));
	/* memcpy(vertex_data + offset + c_offset, color, sizeof(vec4)); */
	memcpy(vertex_data + offset + n_offset, n, sizeof(vec3));
	offset += sizeof(vec3) /* + sizeof(vec4) */ + sizeof(vec3);

	for (u32 i = 1; i < num_strips; ++i)
	{
		const f32 k = inc_angle * i;
		for (u32 j = 0; j < points_per_strip; ++j)
		{
			const f32 t = inc_angle * j;
			vec3_set(tmp, f32_cos(t), 0.0f, -f32_sin(t));
			vec3_normalize(vertex, tmp);
			vec3_mul_constant(vertex, f32_sin(k));
			vertex[1] = f32_cos(k);
			vec3_normalize(n, vertex);
			vec3_mul_constant(vertex, radius);
			vec3_translate(vertex, translation);

			memcpy(vertex_data + offset + p_offset, vertex, sizeof(vec3));
			//memcpy(vertex_data + offset + c_offset, color, sizeof(vec4));
			memcpy(vertex_data + offset + n_offset, n, sizeof(vec3));
			offset += sizeof(vec3) /*+ sizeof(vec4)*/ + sizeof(vec3);
		}
	}

	vec3_set(vertex, 0.0f, -radius, 0.0f);
	vec3_translate(vertex, translation);
	vec3_normalize(n, vertex);
	memcpy(vertex_data + offset + p_offset, vertex, sizeof(vec3));
	//memcpy(vertex_data + offset + c_offset, color, sizeof(vec4));
	memcpy(vertex_data + offset + n_offset, n, sizeof(vec3));
	offset += sizeof(vec3) /*+ sizeof(vec4) */ + sizeof(vec3);

	*b_i = 0;	
	u32 k = 0;

	for (u32 i = 0; i < points_per_strip; ++i)
	{
		index_data[k++] = *b_i + 1 + ((i + 1) % points_per_strip);
		index_data[k++] = *b_i;
		index_data[k++] = *b_i + i + 1;
	}

	*b_i += 1;
	for (u32 i = 1; i < (num_strips-1); ++i)
	{
		*b_i += points_per_strip;
		for (u32 j = 0; j < points_per_strip; ++j)
		{
			index_data[k++] = *b_i + ((j + 1) % points_per_strip);
			index_data[k++] = *b_i + j - points_per_strip;
			index_data[k++] = *b_i + j;
			index_data[k++] = *b_i + ((j + 1) % points_per_strip);
			index_data[k++] = *b_i + ((j + 1) % points_per_strip) - points_per_strip;
			index_data[k++] = *b_i + j - points_per_strip;
		}
	}

	for (u32 i = 0; i < points_per_strip; ++i)
	{
		index_data[k++] = *b_i + points_per_strip;
		index_data[k++] = *b_i + ((i + 1) % points_per_strip);
		index_data[k++] = *b_i + i;
	}

	*b_i += points_per_strip;
}

/* const_circle_points - number of vertices on single circle of sphere */
void r_mesh_set_sphere(struct arena *mem, struct r_mesh *mesh, const f32 radius, const u32 refinement/*, const vec4 color*/)
{
	kas_assert(refinement >= 3);

	const u32 points_per_strip = 2 * refinement;
	const u32 num_strips = refinement;
	const f32 inc_angle = MM_PI_F / refinement;

	const u32 vertex_count = 2 + (num_strips - 1) * points_per_strip;
	const u64 vertex_size = sizeof(vec3) + /*sizeof(vec4) */+ sizeof(vec3);
	u8 *vertex_data = arena_push(mem, vertex_count * vertex_size);

	const u32 index_count = 2 * 3 * points_per_strip + (num_strips-1-1) * points_per_strip * 6;
	u32 *index_data = arena_push(mem, index_count * sizeof(u32));

	u32 max_used = 0;
	const vec3 translation = { 0.0f, 0.0f, 0.0f };
	//internal_r_mesh_set_sphere(&max_used, vertex_data, index_data, radius, translation, refinement, color);
	internal_r_mesh_set_sphere(&max_used, vertex_data, index_data, radius, translation, refinement);

	mesh->attribute_flags = R_MESH_ATTRIBUTE_POSITION /* | R_MESH_ATTRIBUTE_COLOR */ | R_MESH_ATTRIBUTE_NORMAL;
	mesh->index_max_used = max_used;
	mesh->index_count = index_count;
	mesh->index_data = index_data;
	mesh->vertex_count = vertex_count;
	mesh->vertex_data = (void *) vertex_data;
}

void r_mesh_set_capsule(struct arena *mem, struct r_mesh *mesh, const vec3 p1, const f32 radius, const quat rotation, const u32 refinement/*, const vec4 color*/)
{
	kas_assert(refinement >= 3);

	const f32 half_length = vec3_length(p1);

	const u32 points_per_strip = 2 * refinement;
	const u32 num_strips = refinement;
	const f32 inc_angle = MM_PI_F / refinement;

	const u32 vertex_count = 2*(2 + (num_strips - 1) * points_per_strip) + 2*refinement;
	const u64 vertex_size = sizeof(vec3) /*+ sizeof(vec4)*/ + sizeof(vec3);
	u8 *vertex_data = arena_push(mem, vertex_count * vertex_size);

	const u32 index_count = 2*(2 * 3 * points_per_strip + (num_strips-1-1) * points_per_strip * 6) + 3*2*(refinement-2) + 6*refinement;
	u32 *index_data = arena_push(mem, index_count * sizeof(u32));

	u32 off_i = 0; 
	u64 offset = 0;
	const u64 p_offset = 0;
	//const u64 c_offset = sizeof(vec3);
	const u64 n_offset = sizeof(vec3) /*+ sizeof(vec4)*/;

	mat3 rot;
	quat_to_mat3(rot, rotation);

	vec3 p, tmp;
	vec3_copy(p, p1);
	u32 max_used;
	//internal_r_mesh_set_sphere(&max_used, vertex_data + offset, index_data + off_i, radius, p, refinement, color);
	internal_r_mesh_set_sphere(&max_used, vertex_data + offset, index_data + off_i, radius, p, refinement);
	off_i = (2 * points_per_strip + (num_strips-1-1) * points_per_strip * 6);
	offset += (2 + (num_strips - 1) * points_per_strip) * (sizeof(vec3) + sizeof(vec4) + sizeof(vec3));

	quat cyl_rot; 
	vec3 v, v_rot, cross, n = { 0.0f, 1.0f, 0.0f }; 
	if (p[0] != 0.0f || p[2] != 0.0f)
	{
		vec3_cross(cross, p, n);
		vec3_mul_constant(cross, 1.0f/vec3_length(cross));
		const f32 theta = f32_acos(vec3_dot(n, p) / half_length);
		unit_axis_angle_to_quaternion(cyl_rot, cross, -theta);
		quat_to_mat3(rot, cyl_rot);
	}

	vec3_negative(p);
	//internal_r_mesh_set_sphere(&max_used, vertex_data + offset, index_data + off_i, radius, p, refinement, color);
	internal_r_mesh_set_sphere(&max_used, vertex_data + offset, index_data + off_i, radius, p, refinement);
	max_used = 2*max_used + 1;
	off_i = 2*off_i;
	offset = 2*offset;
	vec3_negative(p);

	const f32 angle_inc = MM_PI_2_F / refinement;
	v[1] = half_length;
	for (u32 i = 0; i < refinement; ++i)
	{
		v[0] = f32_cos(angle_inc * i) * radius;
		v[2] = f32_sin(angle_inc * i) * radius;
		mat3_vec_mul(v_rot, rot, v);

		vec3_normalize(n, p);
		vec3_scale(tmp, n, vec3_dot(v_rot, n));
		vec3_sub(n, v_rot, tmp);
		vec3_mul_constant(n, 1.0f/vec3_length(n));

		memcpy(vertex_data + offset + p_offset, v_rot, sizeof(vec3));
		//memcpy(vertex_data + offset + c_offset, color, sizeof(vec4));
		memcpy(vertex_data + offset + n_offset, n, sizeof(vec3));
		//offset += sizeof(vec3) + sizeof(vec4) + sizeof(vec3);
		offset += sizeof(vec3) + sizeof(vec3);
	}
		
	v[1] = -half_length;
	for (u32 i = 0; i < refinement; ++i)
	{
		v[0] = f32_cos(angle_inc * i) * radius;
		v[2] = f32_sin(angle_inc * i) * radius;
		mat3_vec_mul(v_rot, rot, v);

		vec3_normalize(n, p);
		vec3_scale(tmp, n, vec3_dot(v_rot, n));
		vec3_sub(n, v_rot, tmp);
		vec3_mul_constant(n, 1.0f/vec3_length(n));

		memcpy(vertex_data + offset + p_offset, v_rot, sizeof(vec3));
		//memcpy(vertex_data + offset + c_offset, color, sizeof(vec4));
		memcpy(vertex_data + offset + n_offset, n, sizeof(vec3));
		//offset += sizeof(vec3) + sizeof(vec4) + sizeof(vec3);
		offset += sizeof(vec3) + sizeof(vec3);
	}


	u32 index = max_used+1;
	for (u32 i = 2; i < refinement; ++i)
	{
		index_data[off_i++] = index;
		index_data[off_i++] = index+i;
		index_data[off_i++] = index+i-1;
	}
	
	index += refinement;
	for (u32 i = 2; i < refinement; ++i)
	{
		index_data[off_i++] = index;
		index_data[off_i++] = index+i-1;
		index_data[off_i++] = index+i;
	}


	u32 indices[6];
	for (u32 i = 0; i < refinement; ++i)
	{
		index_data[off_i++] = index + i;
		index_data[off_i++] = index + i - refinement;
		index_data[off_i++] = index + ((i+1) % refinement) - refinement;
		index_data[off_i++] = index + i;
		index_data[off_i++] = index + ((i+1) % refinement) - refinement;
		index_data[off_i++] = index + ((i+1) % refinement);
	}

	max_used += 2 * refinement;

	//mesh->attribute_flags = R_MESH_ATTRIBUTE_POSITION | R_MESH_ATTRIBUTE_COLOR | R_MESH_ATTRIBUTE_NORMAL;
	mesh->attribute_flags = R_MESH_ATTRIBUTE_POSITION | R_MESH_ATTRIBUTE_NORMAL;
	mesh->index_max_used = max_used;
	mesh->index_count = index_count;
	mesh->index_data = index_data;
	mesh->vertex_count = vertex_count;
	mesh->vertex_data = (void *) vertex_data;
}

void r_mesh_set_hull(struct arena *mem, struct r_mesh *mesh, const struct collision_hull *hull/*, const vec4 color*/)
{
	mesh->vertex_data = mem->stack_ptr;
	mesh->vertex_count = 0;

	for (u32 fi = 0; fi < hull->f_count; ++fi)
	{
		vec3 normal;
		struct hull_face *f = hull->f + fi;
		struct hull_half_edge *e0 = hull->e + f->first;
		struct hull_half_edge *e1 = hull->e + f->first + 1;
		struct hull_half_edge *e2 = hull->e + f->first + 2;
		triangle_CCW_normal(normal, 
				hull->v[e0->origin],
				hull->v[e1->origin],
				hull->v[e2->origin]);

		vec3 p0, p1, p2;
		vec3_copy(p0, hull->v[e0->origin]); 
                vec3_copy(p1, hull->v[e1->origin]);
                vec3_copy(p2, hull->v[e2->origin]);

		arena_push_packed_memcpy(mem, p0, sizeof(vec3));
		//arena_push_packed_memcpy(mem, color, sizeof(vec4));
		arena_push_packed_memcpy(mem, normal, sizeof(vec3));
		arena_push_packed_memcpy(mem, p1, sizeof(vec3));
		//arena_push_packed_memcpy(mem, color, sizeof(vec4));
		arena_push_packed_memcpy(mem, normal, sizeof(vec3));
		arena_push_packed_memcpy(mem, p2, sizeof(vec3));
		//arena_push_packed_memcpy(mem, color, sizeof(vec4));
		arena_push_packed_memcpy(mem, normal, sizeof(vec3));
		mesh->vertex_count += 3;

		const u32 tri_count = f->count - 2;
		for (u32 ti = 1; ti < tri_count; ++ti)
		{
			mesh->vertex_count += 1;
			e2 = hull->e + f->first + ti + 2;
                	vec3_copy(p2, hull->v[e2->origin]);

			arena_push_packed_memcpy(mem, p2, sizeof(vec3));
			//arena_push_packed_memcpy(mem, color, sizeof(vec4));
			arena_push_packed_memcpy(mem, normal, sizeof(vec3));
		}
	}

	mesh->index_data = (u32 *) mem->stack_ptr;
	mesh->index_count = 0;
	u32 m_i = 0;
	for (u32 fi = 0; fi < hull->f_count; ++fi)
	{
		u32 indices[3] = { m_i + 0, m_i + 1, m_i + 2 };
		u32 offset = 3;
		arena_push_packed_memcpy(mem, indices, sizeof(indices));
		mesh->index_count += 3;

		struct hull_face *f = hull->f + fi;
		const u32 tri_count = f->count - 2;
		for (u32 ti = 1; ti < tri_count; ++ti)
		{
			indices[0] = m_i;
			indices[1] = m_i + ti + 1;
			indices[2] = m_i + ti + 2;

			arena_push_packed_memcpy(mem, indices, sizeof(indices));
			offset += 1;
			mesh->index_count += 3;
		}

		kas_assert((u64) m_i + offset <= U32_MAX)
		m_i += offset;
	}

	//mesh->attribute_flags = R_MESH_ATTRIBUTE_POSITION | R_MESH_ATTRIBUTE_COLOR | R_MESH_ATTRIBUTE_NORMAL;
	mesh->attribute_flags = R_MESH_ATTRIBUTE_POSITION | R_MESH_ATTRIBUTE_NORMAL;
	mesh->index_max_used = m_i - 1;
}
