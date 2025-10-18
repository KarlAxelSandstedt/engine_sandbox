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

#include "r_local.h"

u32 aabb_index_data[] = { 0,1,0,2,0,3,1,4,1,5,2,4,2,6,3,5,3,6,6,7,5,7,4,7 };
#define AABB_INDEX_COUNT	(sizeof(aabb_index_data) / sizeof(aabb_index_data[0]))

void AABB_push_lines_buffered(void *vertex_buf, u32 *index_data, const u32 next_index, const struct AABB *box, const vec4 color)
{
	kas_assert((u64) next_index + 7 <= U32_MAX);
	vec3 end;
	vec3_sub(end, box->center, box->hw);

	/* 0, 1, 2, 3 */
	f32 *v = vertex_buf;
	vec3_copy(v, end);
	v += 3;
	vec4_copy(v, color);
	v += 4;

	end[0] += 2.0f*box->hw[0];
	vec3_copy(v, end);
	v += 3;
	vec4_copy(v, color);
	v += 4;

	end[0] -= 2.0f*box->hw[0];
	end[1] += 2.0f*box->hw[1];
	vec3_copy(v, end);
	v += 3;
	vec4_copy(v, color);
	v += 4;

	end[1] -= 2.0f*box->hw[1];
	end[2] += 2.0f*box->hw[2];
	vec3_copy(v, end);
	v += 3;
	vec4_copy(v, color);
	v += 4;

	/* 4, 5 */
	end[0] += 2.0f*box->hw[0];
	end[1] += 2.0f*box->hw[1];
	end[2] -= 2.0f*box->hw[2];
	vec3_copy(v, end);
	v += 3;
	vec4_copy(v, color);
	v += 4;

	end[1] -= 2.0f*box->hw[1];
	end[2] += 2.0f*box->hw[2];
	vec3_copy(v, end);
	v += 3;
	vec4_copy(v, color);
	v += 4;

	/* 6 */
	end[0] -= 2.0f*box->hw[0];
	end[1] += 2.0f*box->hw[1];
	vec3_copy(v, end);
	v += 3;
	vec4_copy(v, color);
	v += 4;

	/* 7 */
	end[0] += 2.0f*box->hw[0];
	vec3_copy(v, end);
	v += 3;
	vec4_copy(v, color);

	for (u32 i = 0; i < AABB_INDEX_COUNT; ++i)
	{
		index_data[i] = aabb_index_data[i] + next_index;
	}
}

#ifdef KAS_PHYSICS_DEBUG 

void r_physics_debug_init(void)
{
	g_r_core->physics_debug.unit_dynamic_tree = R_UNIT_NULL;
	g_r_core->physics_debug.unit_bounding_box = R_UNIT_NULL;
	g_r_core->physics_debug.unit_segment = R_UNIT_NULL;
	g_r_core->physics_debug.unit_plane = R_UNIT_NULL;
	g_r_core->physics_debug.unit_collision = R_UNIT_NULL;
	g_r_core->physics_debug.unit_contact_manifold_1 = R_UNIT_NULL;
	g_r_core->physics_debug.unit_contact_manifold_2 = R_UNIT_NULL;
	g_r_core->physics_debug.unit_sleeping = R_UNIT_NULL;
	g_r_core->physics_debug.unit_island = R_UNIT_NULL;
}

void r_physics_debug_flush(void)
{
	g_r_core->physics_debug.unit_dynamic_tree = R_UNIT_NULL;
	g_r_core->physics_debug.unit_bounding_box = R_UNIT_NULL;
	g_r_core->physics_debug.unit_segment = R_UNIT_NULL;
	g_r_core->physics_debug.unit_plane = R_UNIT_NULL;
	g_r_core->physics_debug.unit_collision = R_UNIT_NULL;
	g_r_core->physics_debug.unit_contact_manifold_1 = R_UNIT_NULL;
	g_r_core->physics_debug.unit_contact_manifold_2 = R_UNIT_NULL;
	g_r_core->physics_debug.unit_sleeping = R_UNIT_NULL;
	g_r_core->physics_debug.unit_island = R_UNIT_NULL;
}

static void internal_r_core_dbvt_push_lines(struct physics_pipeline *pipeline, const vec4 color)
{
	struct arena *mem = &pipeline->frame;
	struct dbvt *tree = &pipeline->dynamic_tree;
	struct r_static *r_static = r_static_lookup(g_r_core->physics_debug.unit_dynamic_tree);
	r_static->vertex_size = 8 * tree->node_count * (sizeof(vec3) + sizeof(vec4));
	r_static->vertex_data = arena_push(mem, r_static->vertex_size);
	r_static->index_count = tree->node_count * AABB_INDEX_COUNT;
	r_static->index_data = arena_push(mem, sizeof(u32)*r_static->index_count);
	r_static->range = r_static_range_init(mem, 0, 0);

	if (!r_static->vertex_data || !r_static->index_data || !r_static->range)
	{
		r_static->vertex_size = 0;
		r_static->vertex_data = NULL;
		r_static->index_count = 0;
		r_static->index_data = NULL;
		r_static->range = NULL;
		return;
	}

	struct r_static_range *range = r_static->range;
	f32 *vertex_data = r_static->vertex_data;
	u32 *index_data = r_static->index_data;
	u32 next_index = 0;

	i32 i = tree->root;
	i32 q = DBVT_NO_NODE;

	while (i != DBVT_NO_NODE)
	{
		if ((u64) next_index + 7 > U32_MAX)
		{
			next_index = 0;
			range->next = r_static_range_init(mem
				, (u64) vertex_data - (u64) r_static->vertex_data
				, ((u64) index_data - (u64) r_static->index_data) / sizeof(u32));
			range = range->next;
			if (!range)
			{
				return;
			}
		}

		AABB_push_lines_buffered(vertex_data, index_data, next_index, &tree->nodes[i].box, color);
		vertex_data += 8*(3 + 4);
		index_data += AABB_INDEX_COUNT;
		range->vertex_size += 8*(sizeof(vec3) + sizeof(vec4));
		range->index_count += AABB_INDEX_COUNT;
		next_index += 8;

		if (tree->nodes[i].left != DBVT_NO_NODE)
		{
			tree->cost_index[++q] = tree->nodes[i].right;
			kas_assert(q < COST_QUEUE_MAX);
			i = tree->nodes[i].left;
		}
		else if (q != DBVT_NO_NODE)
		{
			i = tree->cost_index[q--];
		}
		else
		{
			i = DBVT_NO_NODE;
		}
	}
}

static void internal_r_core_physics_pipeline_push_bounding_boxes(struct physics_pipeline *pipeline, const vec4 color)
{
	struct arena *mem = &pipeline->frame;
	struct r_static *r_static = r_static_lookup(g_r_core->physics_debug.unit_bounding_box);
	r_static->vertex_size = 8 * pipeline->body_list->count * (sizeof(vec3) + sizeof(vec4));
	r_static->vertex_data = arena_push(mem, r_static->vertex_size);
	r_static->index_count = pipeline->body_list->count * AABB_INDEX_COUNT;
	r_static->index_data = arena_push(mem, sizeof(u32)*r_static->index_count);
	r_static->range = r_static_range_init(mem, 0, 0);

	if (!r_static->vertex_data || !r_static->index_data || !r_static->range)
	{
		r_static->vertex_size = 0;
		r_static->vertex_data = NULL;
		r_static->index_count = 0;
		r_static->index_data = NULL;
		r_static->range = NULL;
		return;
	}

	struct r_static_range *range = r_static->range;
	f32 *vertex_data = r_static->vertex_data;
	u32 *index_data = r_static->index_data;
	u32 next_index = 0;

	for (u32 i = 0; i < pipeline->body_list->max_count; ++i)
	{
		const struct rigid_body *b = physics_pipeline_rigid_body_lookup(pipeline, i);
		const u32 flags = RB_ACTIVE;
		if (b->header.allocated && (b->flags & flags) == flags)
		{
			if ((u64) next_index + 7 > U32_MAX)
			{
				next_index = 0;
				range->next = r_static_range_init(mem
					, (u64) vertex_data - (u64) r_static->vertex_data
					, ((u64) index_data - (u64) r_static->index_data) / sizeof(u32));
				range = range->next;
				if (!range)
				{
					return;
				}
			}

			struct AABB world_box = b->local_box;
			vec3_translate(world_box.center, b->position);
			AABB_push_lines_buffered(vertex_data, index_data, next_index, &world_box, color);
			vertex_data += 8*(3 + 4);
			index_data += AABB_INDEX_COUNT;
			range->vertex_size += 8*(sizeof(vec3) + sizeof(vec4));
			range->index_count += AABB_INDEX_COUNT;
			next_index += 8;
		}
	}
}

static void internal_r_core_physics_pipeline_push_segments(struct physics_pipeline *pipeline, const vec4 color)
{
	struct arena *mem = &pipeline->frame;
	struct r_static *r_static = r_static_lookup(g_r_core->physics_debug.unit_segment);
	r_static->vertex_size = 2 * g_collision_debug->segment_count * (sizeof(vec3) + sizeof(vec4));
	r_static->vertex_data = arena_push(mem, r_static->vertex_size);
	r_static->index_count = 2 * g_collision_debug->segment_count;
	r_static->index_data = arena_push(mem, sizeof(u32)*r_static->index_count);
	r_static->range = r_static_range_init(mem, 0, 0);

	if (!r_static->vertex_data || !r_static->index_data || !r_static->range)
	{
		r_static->vertex_size = 0;
		r_static->vertex_data = NULL;
		r_static->index_count = 0;
		r_static->index_data = NULL;
		r_static->range = NULL;
		return;
	}

	struct r_static_range *range = r_static->range;
	f32 *vertex_data = r_static->vertex_data;
	u32 *index_data = r_static->index_data;
	u32 next_index = 0;

	for (u32 i = 0; i < g_collision_debug->segment_count; ++i)
	{
		if ((u64) next_index + 1 > U32_MAX)
		{
			next_index = 0;
			range->next = r_static_range_init(mem
				, (u64) vertex_data - (u64) r_static->vertex_data
				, ((u64) index_data - (u64) r_static->index_data) / sizeof(u32));
			range = range->next;
			if (!range)
			{
				return;
			}
		}

		vec3_copy(vertex_data, g_collision_debug->segment[i][0]);
		vertex_data += 3;
		vec4_copy(vertex_data, color);
		vertex_data += 4;

		vec3_copy(vertex_data, g_collision_debug->segment[i][1]);
		vertex_data += 3;
		vec4_copy(vertex_data, color);
		vertex_data += 4;

		index_data[0] = next_index;
		index_data[1] = next_index+1;
		index_data += 2;

		next_index += 2;
	}
}

static void internal_r_core_physics_pipeline_push_planes(struct physics_pipeline *pipeline)
{
	struct arena *mem = &pipeline->frame;
	struct r_static *r_static = r_static_lookup(g_r_core->physics_debug.unit_plane);
	r_static->vertex_size = 4 * g_collision_debug->plane_count * (sizeof(vec3) + sizeof(vec4));
	r_static->vertex_data = arena_push(mem, r_static->vertex_size);
	r_static->index_count = 6 * g_collision_debug->plane_count;
	r_static->index_data = arena_push(mem, sizeof(u32)*r_static->index_count);
	r_static->range = r_static_range_init(mem, 0, 0);

	if (!r_static->vertex_data || !r_static->index_data || !r_static->range)
	{
		r_static->vertex_size = 0;
		r_static->vertex_data = NULL;
		r_static->index_count = 0;
		r_static->index_data = NULL;
		r_static->range = NULL;
		return;
	}

	struct r_static_range *range = r_static->range;
	f32 *vertex_data = r_static->vertex_data;
	u32 *index_data = r_static->index_data;
	u32 next_index = 0;

	for (u32 i = 0; i < g_collision_debug->plane_count; ++i)
	{
		if ((u64) next_index + 3 > U32_MAX)
		{
			next_index = 0;
			range->next = r_static_range_init(mem
				, (u64) vertex_data - (u64) r_static->vertex_data
				, ((u64) index_data - (u64) r_static->index_data) / sizeof(u32));
			range = range->next;
			if (!range)
			{
				return;
			}
		}

		const struct plane_visual *vp = g_collision_debug->plane_visuals + i;
		const struct plane *plane = &vp->plane;

		vec3 cross, o1, o2 = VEC3_ZERO;

		u32 min_index = 0;
		if (plane->normal[0]*plane->normal[0] > plane->normal[1]*plane->normal[1]) { min_index = 1; };
		if (plane->normal[min_index]*plane->normal[min_index] > plane->normal[2]*plane->normal[2]) { min_index = 2; };
		o2[min_index] = 1.0f;
		vec3_cross(o1, plane->normal, o2);
		vec3_cross(o2, plane->normal, o1);

		vec3 v[4];
		vec3_copy(v[0], vp->center);
		vec3_copy(v[1], vp->center);
		vec3_copy(v[2], vp->center);
		vec3_copy(v[3], vp->center);

		vec3_cross(cross, o1, o2);
		if (vec3_dot(cross, plane->normal) > 0.0f)
		{
			vec3_translate_scaled(v[0], o1,  1.0f);
			vec3_translate_scaled(v[1], o2,  1.0f);
			vec3_translate_scaled(v[2], o1, -1.0f);
			vec3_translate_scaled(v[3], o2, -1.0f);
		}
		else
		{
			vec3_translate_scaled(v[0], o2, -1.0f);
			vec3_translate_scaled(v[1], o1, -1.0f);
			vec3_translate_scaled(v[2], o2,  1.0f);
			vec3_translate_scaled(v[3], o1,  1.0f);
		}


		vec3_copy(vertex_data, v[0]);
		vertex_data += 3;
		vec4_copy(vertex_data, vp->color);
		vertex_data += 4;

		vec3_copy(vertex_data, v[1]);
		vertex_data += 3;
		vec4_copy(vertex_data, vp->color);
		vertex_data += 4;

		vec3_copy(vertex_data, v[2]);
		vertex_data += 3;
		vec4_copy(vertex_data, vp->color);
		vertex_data += 4;

		vec3_copy(vertex_data, v[3]);
		vertex_data += 3;
		vec4_copy(vertex_data, vp->color);
		vertex_data += 4;

		index_data[0] = next_index;
		index_data[1] = next_index+1;
		index_data[2] = next_index+2;
		index_data[3] = next_index+0;
		index_data[4] = next_index+2;
		index_data[5] = next_index+3;
		index_data += 6;

		next_index += 4;
	}
}

static void internal_r_core_physics_pipeline_push_contact_manifold_segments(struct physics_pipeline *pipeline)
{
	struct arena *mem = &pipeline->frame;
	struct contact_manifold *cm = pipeline->c_state.cm;
	const u32 cm_count = pipeline->c_state.cm_count;
	struct r_static *r_static = r_static_lookup(g_r_core->physics_debug.unit_contact_manifold_1);
	r_static->vertex_size = 2 * cm_count * (sizeof(vec3) + sizeof(vec4));
	r_static->vertex_data = arena_push(mem, r_static->vertex_size);
	r_static->index_count = 2 * cm_count;
	r_static->index_data = arena_push(mem, sizeof(u32)*r_static->index_count);
	r_static->range = r_static_range_init(mem, 0, 0);

	if (!r_static->vertex_data || !r_static->index_data || !r_static->range)
	{
		r_static->vertex_size = 0;
		r_static->vertex_data = NULL;
		r_static->index_count = 0;
		r_static->index_data = NULL;
		r_static->range = NULL;
		return;
	}

	struct r_static_range *range = r_static->range;
	f32 *vertex_data = r_static->vertex_data;
	u32 *index_data = r_static->index_data;
	u32 next_index = 0;

	u32 cm_segments = 0;
	for (u32 i = 0; i < cm_count; ++i)
	{
		vec3 n0, n1;
		switch (cm[i].v_count)
		{
			case 1:
			{
				vec3_copy(n0, cm[i].v[0]);
				vec3_add(n1, n0, cm[i].n);
			} break;

			case 2:
			{
				vec3_interpolate(n0, cm[i].v[0], cm[i].v[1], 0.5f);
				vec3_add(n1, n0, cm[i].n);
			} break;

			case 3:
			{
				vec3_scale(n0, cm[i].v[0], 1.0f/3.0f);
				vec3_translate_scaled(n0, cm[i].v[1], 1.0f/3.0f);
				vec3_translate_scaled(n0, cm[i].v[2], 1.0f/3.0f);
				vec3_add(n1, n0, cm[i].n);
			} break;

			case 4:
			{
				vec3_scale(n0, cm[i].v[0], 1.0f/4.0f);
				vec3_translate_scaled(n0, cm[i].v[1], 1.0f/4.0f);
				vec3_translate_scaled(n0, cm[i].v[2], 1.0f/4.0f);
				vec3_translate_scaled(n0, cm[i].v[3], 1.0f/4.0f);
				vec3_add(n1, n0, cm[i].n);
			} break;

			default:
			{
				continue;
			} break;
		}

		if ((u64) next_index + 1 > U32_MAX)
		{
			next_index = 0;
			range->next = r_static_range_init(mem
				, (u64) vertex_data - (u64) r_static->vertex_data
				, ((u64) index_data - (u64) r_static->index_data) / sizeof(u32));
			range = range->next;
			if (!range)
			{
				return;
			}
		}

		vec3_copy(vertex_data, n0);
		vertex_data += 3;
		vec4_copy(vertex_data, g_collision_debug->contact_manifold_color);
		vertex_data += 4;

		vec3_copy(vertex_data, n1);
		vertex_data += 3;
		vec4_copy(vertex_data, g_collision_debug->contact_manifold_color);
		vertex_data += 4;

		index_data[0] = next_index;
		index_data[1] = next_index+1;
		index_data += 2;

		next_index += 2;
		cm_segments += 1;
	}

	r_static->vertex_size = cm_segments * 2 * (sizeof(vec3) + sizeof(vec4));
	r_static->index_count = cm_segments * 2;
}

static void internal_r_core_physics_pipeline_push_contact_manifold_triangles(struct physics_pipeline *pipeline)
{
	struct arena *mem = &pipeline->frame;
	struct contact_manifold *cm = pipeline->c_state.cm;
	const u32 cm_count = pipeline->c_state.cm_count;
	struct r_static *r_static = r_static_lookup(g_r_core->physics_debug.unit_contact_manifold_2);
	r_static->vertex_size = 4 * cm_count * (sizeof(vec3) + sizeof(vec4));
	r_static->vertex_data = arena_push(mem, r_static->vertex_size);
	r_static->index_count = 6 * cm_count;
	r_static->index_data = arena_push(mem, sizeof(u32)*r_static->index_count);
	r_static->range = r_static_range_init(mem, 0, 0);

	if (!r_static->vertex_data || !r_static->index_data || !r_static->range)
	{
		r_static->vertex_size = 0;
		r_static->vertex_data = NULL;
		r_static->index_count = 0;
		r_static->index_data = NULL;
		r_static->range = NULL;
		return;
	}

	struct r_static_range *range = r_static->range;
	f32 *vertex_data = r_static->vertex_data;
	u32 *index_data = r_static->index_data;
	u32 next_index = 0;

	vec3 v[4];
	u32 cm_triangles = 0;
	u32 cm_planes = 0;
	for (u32 i = 0; i < cm_count; ++i)
	{
		switch (cm[i].v_count)
		{
			case 3:
			{
				vec3_copy(v[0], cm[i].v[0]);
				vec3_copy(v[1], cm[i].v[1]);
				vec3_copy(v[2], cm[i].v[2]);
				vec3_translate_scaled(v[0], cm[i].n, 0.005f);
				vec3_translate_scaled(v[1], cm[i].n, 0.005f);
				vec3_translate_scaled(v[2], cm[i].n, 0.005f);

				if ((u64) next_index + 2 > U32_MAX)
				{
					next_index = 0;
					range->next = r_static_range_init(mem
						, (u64) vertex_data - (u64) r_static->vertex_data
						, ((u64) index_data - (u64) r_static->index_data) / sizeof(u32));
					range = range->next;
					if (!range)
					{
						return;
					}
				}
				vec3_copy(vertex_data, v[0]);
				vertex_data += 3;
				vec4_copy(vertex_data, g_collision_debug->contact_manifold_color);
				vertex_data += 4;

				vec3_copy(vertex_data, v[1]);
				vertex_data += 3;
				vec4_copy(vertex_data, g_collision_debug->contact_manifold_color);
				vertex_data += 4;

				vec3_copy(vertex_data, v[2]);
				vertex_data += 3;
				vec4_copy(vertex_data, g_collision_debug->contact_manifold_color);
				vertex_data += 4;

				index_data[0] = next_index;
				index_data[1] = next_index+1;
				index_data[2] = next_index+2;
				index_data += 3;

				next_index += 3;
				cm_triangles += 1;

			} break;

			case 4:
			{
				vec3_copy(v[0], cm[i].v[0]);
				vec3_copy(v[1], cm[i].v[1]);
				vec3_copy(v[2], cm[i].v[2]);
				vec3_copy(v[3], cm[i].v[3]);
				vec3_translate_scaled(v[0], cm[i].n, 0.005f);
				vec3_translate_scaled(v[1], cm[i].n, 0.005f);
				vec3_translate_scaled(v[2], cm[i].n, 0.005f);
				vec3_translate_scaled(v[3], cm[i].n, 0.005f);

				if ((u64) next_index + 3 > U32_MAX)
				{
					next_index = 0;
					range->next = r_static_range_init(mem
						, (u64) vertex_data - (u64) r_static->vertex_data
						, ((u64) index_data - (u64) r_static->index_data) / sizeof(u32));
					range = range->next;
					if (!range)
					{
						return;
					}
				}
				vec3_copy(vertex_data, v[0]);
				vertex_data += 3;
				vec4_copy(vertex_data, g_collision_debug->contact_manifold_color);
				vertex_data += 4;

				vec3_copy(vertex_data, v[1]);
				vertex_data += 3;
				vec4_copy(vertex_data, g_collision_debug->contact_manifold_color);
				vertex_data += 4;

				vec3_copy(vertex_data, v[2]);
				vertex_data += 3;
				vec4_copy(vertex_data, g_collision_debug->contact_manifold_color);
				vertex_data += 4;

				vec3_copy(vertex_data, v[3]);
				vertex_data += 3;
				vec4_copy(vertex_data, g_collision_debug->contact_manifold_color);
				vertex_data += 4;

				index_data[0] = next_index;
				index_data[1] = next_index+1;
				index_data[2] = next_index+2;
				index_data[3] = next_index+0;
				index_data[4] = next_index+2;
				index_data[5] = next_index+3;
				index_data += 6;

				next_index += 4;
				cm_planes += 1;
			} break;

			default:
			{
				continue;
			} break;
		}
	}

	r_static->vertex_size = (3*cm_triangles + 4*cm_planes) * (sizeof(vec3) + sizeof(vec4));
	r_static->index_count = 3*cm_triangles + 6*cm_planes;
}


void r_physics_debug_frame_init(struct physics_pipeline *pipeline)
{
	if (g_collision_debug->draw_dynamic_tree)
	{
		if (g_r_core->physics_debug.unit_dynamic_tree == R_UNIT_NULL)
		{
			g_r_core->physics_debug.unit_dynamic_tree = r_static_alloc(R_CMD_SCREEN_LAYER_GAME, R_CMD_TRANSPARENCY_OPAQUE, 0, r_material_construct(PROGRAM_COLOR, TEXTURE_NONE), R_CMD_PRIMITIVE_LINE);
		}

		internal_r_core_dbvt_push_lines(pipeline, g_collision_debug->dynamic_tree_color);
	}	

	if (g_collision_debug->draw_contact_manifold)
	{
		if (g_r_core->physics_debug.unit_contact_manifold_1 == R_UNIT_NULL)
		{
			g_r_core->physics_debug.unit_contact_manifold_1 = r_static_alloc(R_CMD_SCREEN_LAYER_GAME, R_CMD_TRANSPARENCY_OPAQUE, 0, r_material_construct(PROGRAM_COLOR, TEXTURE_NONE), R_CMD_PRIMITIVE_LINE);
			g_r_core->physics_debug.unit_contact_manifold_2 = r_static_alloc(R_CMD_SCREEN_LAYER_GAME, R_CMD_TRANSPARENCY_OPAQUE, 0, r_material_construct(PROGRAM_COLOR, TEXTURE_NONE), R_CMD_PRIMITIVE_TRIANGLE);
		}

		internal_r_core_physics_pipeline_push_contact_manifold_segments(pipeline);
		internal_r_core_physics_pipeline_push_contact_manifold_triangles(pipeline);
	}

	if (g_collision_debug->draw_bounding_box)
	{
		if (g_r_core->physics_debug.unit_bounding_box == R_UNIT_NULL)
		{
			g_r_core->physics_debug.unit_bounding_box = r_static_alloc(R_CMD_SCREEN_LAYER_GAME, R_CMD_TRANSPARENCY_OPAQUE, 0, r_material_construct(PROGRAM_COLOR, TEXTURE_NONE), R_CMD_PRIMITIVE_LINE);
		}
		internal_r_core_physics_pipeline_push_bounding_boxes(pipeline, g_collision_debug->bounding_box_color);
	}

	if (g_collision_debug->draw_segment)
	{
		if (g_r_core->physics_debug.unit_segment == R_UNIT_NULL)
		{
			g_r_core->physics_debug.unit_segment = r_static_alloc(R_CMD_SCREEN_LAYER_GAME, R_CMD_TRANSPARENCY_OPAQUE, 0, r_material_construct(PROGRAM_COLOR, TEXTURE_NONE), R_CMD_PRIMITIVE_LINE);
		}
		internal_r_core_physics_pipeline_push_segments(pipeline, g_collision_debug->segment_color);
	}

	if (g_collision_debug->draw_plane)
	{
		if (g_r_core->physics_debug.unit_plane == R_UNIT_NULL)
		{
			g_r_core->physics_debug.unit_plane = r_static_alloc(R_CMD_SCREEN_LAYER_GAME, R_CMD_TRANSPARENCY_OPAQUE, 0, r_material_construct(PROGRAM_COLOR, TEXTURE_NONE), R_CMD_PRIMITIVE_TRIANGLE);
		}
		internal_r_core_physics_pipeline_push_planes(pipeline);
	}
}

void r_physics_debug_frame_clear(void)
{
	if (!g_collision_debug->draw_dynamic_tree)
	{
		if (g_r_core->physics_debug.unit_dynamic_tree != R_UNIT_NULL)
		{
			r_static_dealloc(g_r_core->physics_debug.unit_dynamic_tree);
			g_r_core->physics_debug.unit_dynamic_tree = R_UNIT_NULL;
		}
	}
	
	if (!g_collision_debug->draw_bounding_box)
	{
		if (g_r_core->physics_debug.unit_bounding_box != R_UNIT_NULL)
		{
			r_unit_dealloc(&g_r_core->frame, g_r_core->physics_debug.unit_bounding_box);
			g_r_core->physics_debug.unit_bounding_box = R_UNIT_NULL;
		}
	}

	if (!g_collision_debug->draw_segment)
	{
		if (g_r_core->physics_debug.unit_segment != R_UNIT_NULL)
		{
			r_static_dealloc(g_r_core->physics_debug.unit_segment);
			g_r_core->physics_debug.unit_segment = R_UNIT_NULL;
		}
	}

	if (!g_collision_debug->draw_contact_manifold)
	{
		if (g_r_core->physics_debug.unit_contact_manifold_1 != R_UNIT_NULL)
		{
			r_unit_dealloc(&g_r_core->frame, g_r_core->physics_debug.unit_contact_manifold_1);
			r_unit_dealloc(&g_r_core->frame, g_r_core->physics_debug.unit_contact_manifold_2);
			g_r_core->physics_debug.unit_contact_manifold_1 = R_UNIT_NULL;
			g_r_core->physics_debug.unit_contact_manifold_2 = R_UNIT_NULL;
		}
	}

	if (!g_collision_debug->draw_plane)
	{
		if (g_r_core->physics_debug.unit_plane != R_UNIT_NULL)
		{
			r_static_dealloc(g_r_core->physics_debug.unit_plane);
			g_r_core->physics_debug.unit_plane = R_UNIT_NULL;
		}
	}
}

#endif
