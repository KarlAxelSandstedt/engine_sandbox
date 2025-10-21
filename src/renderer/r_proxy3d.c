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

static struct r_proxy3d *internal_r_proxy3d_lookup(const u32 type_index)
{
	return hierarchy_index_address(g_r_core->proxy3d_hierarchy, type_index);
}

u32 r_proxy3d_alloc(const u32 parent, const u64 screen, const u64 transparency, const u32 texture, const u32 draw_flags, const struct r_proxy3d_draw_config *config)
{
	/* NOTE: depth calculated on updates to PROXY3D if transparency is set. */
	const u64 depth = 0;
	const u64 material = r_material_construct(PROGRAM_PROXY3D, texture);
	const u32 index = r_unit_alloc(parent
			, R_UNIT_TYPE_PROXY3D
			, screen
			, transparency
			, depth
			, material
			, R_CMD_PRIMITIVE_TRIANGLE);
	struct r_unit *unit = hierarchy_index_address(g_r_core->unit_hierarchy, index);
	struct r_proxy3d *proxy = hierarchy_index_address(g_r_core->proxy3d_hierarchy, unit->type_index);
	proxy->ns_at_update = 0;

	vec3_copy(proxy->position, config->position);
	quat_copy(proxy->rotation, config->rotation);
	vec3_copy(proxy->linear.linear_velocity, config->linear_velocity);
	vec3_copy(proxy->linear.angular_velocity, config->angular_velocity);

	unit->draw_flags = draw_flags;
	unit->mesh_handle = string_database_reference(&g_r_core->mesh_database, *config->mesh_id_alias).index;
	if (unit->mesh_handle == STRING_DATABASE_STUB_INDEX)
	{
		log(T_RENDERER, S_WARNING, "failed to locate mesh %k in database", config->mesh_id_alias);
	}

	//TODO Hardcoded for now 
	proxy->spec_mov = config->spec_mov;
	proxy->pos_type = config->pos_type;

	const vec4 no_color = VEC4_ZERO; 
	if (draw_flags & R_UNIT_DRAW_COLOR)
	{
		unit->color_blending = 1.0f;
		vec4_copy(unit->color, config->color);
	}
	else
	{
		unit->color_blending = 0.0f;
		vec4_copy(unit->color, no_color);
	}
	
	unit->material = material;
	if (draw_flags & R_UNIT_DRAW_TEXTURE)
	{
		if (draw_flags & R_UNIT_DRAW_COLOR)
		{
			unit->color_blending = config->blend;
		}
	}


	unit->transparency = config->transparency;

	return index;
}

struct r_proxy3d *r_proxy3d_lookup(const u32 unit)
{
	struct r_unit *u = r_unit_lookup(unit);
	return (u->type == R_UNIT_TYPE_PROXY3D)
		? internal_r_proxy3d_lookup(u->type_index)
		: NULL;
}


/* Calculate the speculative movement of the proxy locally, i.e., the position of the proxy not counting any position type effects */
static void internal_r_proxy3d_local_speculative_orientation(vec3 position, quat rotation, const struct r_proxy3d *proxy, const u64 ns_time)
{
	const f32 timestep = (f32) (ns_time - proxy->ns_at_update) / NSEC_PER_SEC;
	//fprintf(stderr, "%f\n", timestep);
	//breakpoint(timestep >= 1.0f);

	switch (proxy->spec_mov)
	{
		case PROXY3D_SPEC_LINEAR:
		{
			position[0] = proxy->position[0] + proxy->linear.linear_velocity[0] * timestep;
			position[1] = proxy->position[1] + proxy->linear.linear_velocity[1] * timestep;
			position[2] = proxy->position[2] + proxy->linear.linear_velocity[1] * timestep;

			quat a_vel_quat, rot_delta;
			quat_set(a_vel_quat, 
					proxy->linear.angular_velocity[0], 
					proxy->linear.angular_velocity[1], 
					proxy->linear.angular_velocity[2],
				      	0.0f);
			quat_mult(rot_delta, a_vel_quat, proxy->rotation);
			quat_scale(rot_delta, timestep / 2.0f);
			quat_add(rotation, proxy->rotation, rot_delta);
			quat_normalize(rotation);
		} break;
		
		default:
		{
			kas_assert_string(0, "IMPLEMENT");
		} break;
	}
}

void r_proxy3d_hierarchy_speculate(vec3ptr *position, quatptr *rotation, struct arena *mem, const u64 ns_time)
{
	vec3ptr spec_pos = arena_push(mem, sizeof(vec3) * g_r_core->proxy3d_hierarchy->list->max_count);	
	quatptr spec_rot = arena_push(mem, sizeof(vec4) * g_r_core->proxy3d_hierarchy->list->max_count);
	*position = spec_pos;
	*rotation = spec_rot;

	const vec3 axis = { 0.0f, 1.0f, 0.0f };
	const f32 angle = 0.0f;
	unit_axis_angle_to_quaternion(spec_rot[HI_ROOT_STUB_INDEX], axis, angle);
	vec3_set(spec_pos[HI_ROOT_STUB_INDEX], 0.0f, 0.0f, 0.0f);

	struct hierarchy_index_iterator it = hierarchy_index_iterator_init(mem, g_r_core->proxy3d_hierarchy, HI_ROOT_STUB_INDEX);
	// skip root stub 
	hierarchy_index_iterator_next_df(&it);
	while (it.count)
	{
		const u32 index = hierarchy_index_iterator_next_df(&it);
		const struct r_proxy3d *proxy = internal_r_proxy3d_lookup(index);
		internal_r_proxy3d_local_speculative_orientation(spec_pos[index], spec_rot[index], proxy, ns_time);
		vec3_translate(spec_pos[index], spec_pos[proxy->header.parent]);
		quat tmp;
		quat_copy(tmp, spec_rot[index]);
		quat_mult(spec_rot[index], tmp, spec_rot[proxy->header.parent]);
	}
	hierarchy_index_iterator_release(&it);
}


