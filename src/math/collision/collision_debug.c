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

#include "collision.h"

struct collision_debug state = { 0 };
struct collision_debug *g_collision_debug = &state;

void collision_debug_init(struct arena *mem, const u32 max_bodies, const u32 max_primitives)
{
	if (mem)
	{
		state.segment = arena_push(mem, 2 * sizeof(vec3) * max_primitives);
		state.plane_visuals = arena_push(mem, max_primitives * sizeof(struct plane_visual));
	}
	else
	{
		state.segment = malloc(2 * sizeof(vec3) * max_primitives);
		state.plane_visuals = malloc(max_primitives * sizeof(struct plane_visual));
	}

	static u32 on_first_entry = 1;
	if (on_first_entry)
	{
		on_first_entry = 0;
		state.draw_dynamic_tree = 0;
		state.draw_bounding_box = 0;
		state.draw_segment = 0;
		state.draw_plane = 0;
		state.draw_collision = 0;
		state.draw_contact_manifold = 0;
		state.draw_island = 1;
		state.draw_sleeping = 0;

		state.pending_draw_dynamic_tree = state.draw_dynamic_tree;
		state.pending_draw_bounding_box = state.draw_bounding_box;
		state.pending_draw_segment = state.draw_segment;
		state.pending_draw_plane = state.draw_plane;
		state.pending_draw_collision = state.draw_collision;
		state.pending_draw_contact_manifold = state.draw_contact_manifold;
		state.pending_draw_island = state.draw_island;
		state.pending_draw_sleeping = state.draw_sleeping;

		vec4_set(state.segment_color, 0.9f, 0.9f, 0.2f, 1.0f);
		vec4_set(state.dynamic_tree_color, 1.0f, 0.0f, 0.0f, 1.0f);
		vec4_set(state.bounding_box_color, 1.0f, 0.0f, 1.0f, 1.0f);
		vec4_set(state.collision_color, 1.0f, 0.1f, 0.1f, 0.3f);
		vec4_set(state.contact_manifold_color, 0.68f, 0.85f, 0.90f, 1.0f);

		vec4_set(state.island_static_color, 0.6f, 0.6f, 0.6f, 1.0f);
		vec4_set(state.island_sleeping_color, 113.0f/256.0f, 241.0f/256.0f, 157.0f/256.0f, 0.7f);
		vec4_set(state.island_awake_color, 255.0f/256.0f, 36.0f/256.0f, 48.0f/256.0f, 0.7f);
	}

	state.segment_max_count = max_primitives;
	state.plane_max_count = max_primitives;

	collision_debug_clear();
}

void collision_debug_clear(void)
{
	state.plane_count = 0;	
	state.segment_count = 0;	

	state.draw_island = state.pending_draw_island;
	state.draw_dynamic_tree = state.pending_draw_dynamic_tree;
	state.draw_bounding_box = state.pending_draw_bounding_box;
	state.draw_segment = state.pending_draw_segment;
	state.draw_collision = state.pending_draw_collision;		
	state.draw_contact_manifold = state.pending_draw_contact_manifold;
	state.draw_plane = state.pending_draw_plane;
}

void collision_debug_add_segment(struct segment s)
{
	if (state.draw_segment) 
	{ 
		u32 i = atomic_fetch_add_rlx_32(&state.segment_count, 1);
		if (i < state.segment_max_count)
		{
			vec3_copy(state.segment[i][0], s.p0);
			vec3_copy(state.segment[i][1], s.p1);
			state.segment_count += 1;
		}
		else
		{
			atomic_fetch_sub_rlx_32(&state.segment_count, 1);
			fprintf(stderr, "WARNING: collision_debug out-of-memory in add_segment\n");
		}
	}
}

void collision_debug_add_plane(const struct plane p, const vec3 center, const f32 hw, const vec4 color)
{
	if (state.draw_plane)
	{
		const u32 i = atomic_fetch_add_rlx_32(&state.plane_count, 1);
		if (i < state.plane_max_count)
		{
			state.plane_visuals[i].plane = p;
			state.plane_visuals[i].hw = hw;
			vec4_copy(state.plane_visuals[i].color, color);
			vec3_copy(state.plane_visuals[i].center, center);
			state.plane_count += 1;
		}
		else
		{
			atomic_fetch_sub_rlx_32(&state.plane_count, 1);
			fprintf(stderr, "WARNING: collision_debug out-of-memory in add_plane\n");
		}
	}
}

void *collision_debug_add_segment_callback(void *segment_addr)
{
	struct segment *s = segment_addr;
	collision_debug_add_segment(*s);
	return NULL;
}

void collision_debug_add_AABB_outline(const struct AABB box)
{
	vec3 v0;
	vec3_sub(v0, box.center, box.hw);

	vec3 v[8] =
	{
		{ box.center[0] - box.hw[0], box.center[1] - box.hw[1], box.center[2] - box.hw[2] },
		{ box.center[0] - box.hw[0], box.center[1] - box.hw[1], box.center[2] + box.hw[2] },
		{ box.center[0] - box.hw[0], box.center[1] + box.hw[1], box.center[2] - box.hw[2] },
		{ box.center[0] - box.hw[0], box.center[1] + box.hw[1], box.center[2] + box.hw[2] },
		{ box.center[0] + box.hw[0], box.center[1] - box.hw[1], box.center[2] - box.hw[2] },
		{ box.center[0] + box.hw[0], box.center[1] - box.hw[1], box.center[2] + box.hw[2] },
		{ box.center[0] + box.hw[0], box.center[1] + box.hw[1], box.center[2] - box.hw[2] },
		{ box.center[0] + box.hw[0], box.center[1] + box.hw[1], box.center[2] + box.hw[2] },
	};

	collision_debug_add_segment(segment_construct(v[0], v[1]));
	collision_debug_add_segment(segment_construct(v[0], v[2]));
	collision_debug_add_segment(segment_construct(v[0], v[4]));
	collision_debug_add_segment(segment_construct(v[1], v[3]));
	collision_debug_add_segment(segment_construct(v[1], v[5]));
	collision_debug_add_segment(segment_construct(v[2], v[3]));
	collision_debug_add_segment(segment_construct(v[2], v[6]));
	collision_debug_add_segment(segment_construct(v[4], v[5]));
	collision_debug_add_segment(segment_construct(v[4], v[6]));
	collision_debug_add_segment(segment_construct(v[3], v[7]));
	collision_debug_add_segment(segment_construct(v[5], v[7]));
	collision_debug_add_segment(segment_construct(v[6], v[7]));
}

