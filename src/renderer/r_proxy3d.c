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

void r_proxy3d_buffer_layout_setter(void)
{
	kas_glEnableVertexAttribArray(0);
	kas_glEnableVertexAttribArray(1);
	kas_glEnableVertexAttribArray(2);
	kas_glEnableVertexAttribArray(3);
	kas_glEnableVertexAttribArray(4);

	kas_glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, S_PROXY3D_STRIDE, (void *) (0));
	kas_glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, S_PROXY3D_STRIDE, (void *) (sizeof(vec3)));
	kas_glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, S_PROXY3D_STRIDE, (void *) (sizeof(vec3) + sizeof(vec4)));
	kas_glVertexAttribDivisor(0, 1);
	kas_glVertexAttribDivisor(1, 1);
	kas_glVertexAttribDivisor(2, 1);

	kas_glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, L_PROXY3D_STRIDE, (void *) (0));
	kas_glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, L_PROXY3D_STRIDE, (void *) (sizeof(vec3)));
}

static struct r_proxy3d *internal_r_proxy3d_address(const u32 type_index)
{
	return hierarchy_index_address(g_r_core->proxy3d_hierarchy, type_index);
}

void r_proxy3d_set_linear_speculation(const vec3 position, const quat rotation, const vec3 linear_velocity, const vec3 angular_velocity, const u64 ns_time, const u32 unit)
{
	struct r_proxy3d *proxy = r_proxy3d_address(unit);
	//TODO
	//if (POOL_SLOT_ALLOCATED(proxy))
	//{
		proxy->flags &= ~(PROXY3D_SPECULATE_FLAGS | PROXY3D_MOVING);
		proxy->flags |= PROXY3D_SPECULATE_LINEAR;
		proxy->ns_at_update = ns_time;
		vec3_copy(proxy->position, position);
		quat_copy(proxy->rotation, rotation);
		vec3_copy(proxy->linear.linear_velocity, linear_velocity);
		vec3_copy(proxy->linear.angular_velocity, angular_velocity);
		if (vec3_dot(linear_velocity, linear_velocity) + vec3_dot(angular_velocity, angular_velocity) > 0.0f)
		{
			proxy->flags |= PROXY3D_MOVING;
		}
	//}
}

u32 r_proxy3d_alloc(const struct r_proxy3d_config *config)
{
	struct slot slot = r_unit_alloc(config->mesh);
	struct r_unit *unit = slot.address;
	const u32 unit_index = slot.index;

	slot = hierarchy_index_add(g_r_core->proxy3d_hierarchy, config->parent);
	unit->type = R_UNIT_TYPE_PROXY3D;
	unit->type_index = slot.index;

	struct r_proxy3d *proxy = slot.address;
	proxy->flags = (config->parent != g_r_core->proxy3d_root)
		? PROXY3D_RELATIVE
		: 0;

	r_proxy3d_set_linear_speculation(config->position, config->rotation, config->linear_velocity, config->angular_velocity, config->ns_time, unit_index);

	return unit_index;
}

struct r_proxy3d *r_proxy3d_address(const u32 unit)
{
	struct r_unit *u = r_unit_address(unit);
	kas_assert(u->type == R_UNIT_TYPE_PROXY3D);
	return internal_r_proxy3d_address(u->type_index);
}


/* Calculate the speculative movement of the proxy locally, i.e., the position of the proxy not counting any position type effects */
static void internal_r_proxy3d_local_speculative_orientation(struct r_proxy3d *proxy, const u64 ns_time)
{
	const f32 timestep = (f32) (ns_time - proxy->ns_at_update) / NSEC_PER_SEC;

	switch (proxy->flags & PROXY3D_SPECULATE_FLAGS)
	{
		case PROXY3D_SPECULATE_LINEAR:
		{
			proxy->spec_position[0] = proxy->position[0] + proxy->linear.linear_velocity[0] * timestep;
			proxy->spec_position[1] = proxy->position[1] + proxy->linear.linear_velocity[1] * timestep;
			proxy->spec_position[2] = proxy->position[2] + proxy->linear.linear_velocity[1] * timestep;

			quat a_vel_quat, rot_delta;
			quat_set(a_vel_quat, 
					proxy->linear.angular_velocity[0], 
					proxy->linear.angular_velocity[1], 
					proxy->linear.angular_velocity[2],
				      	0.0f);
			quat_mult(rot_delta, a_vel_quat, proxy->rotation);
			quat_scale(rot_delta, timestep / 2.0f);
			quat_add(proxy->spec_rotation, proxy->rotation, rot_delta);
			quat_normalize(proxy->spec_rotation);	
		} break;
		
		default:
		{
			vec3_copy(proxy->spec_position, proxy->position);	
			quat_copy(proxy->spec_rotation, proxy->rotation);	
		} break;
	}	
}

void r_proxy3d_hierarchy_speculate(struct arena *mem, const u64 ns_time)
{
	struct hierarchy_index_iterator it = hierarchy_index_iterator_init(mem, g_r_core->proxy3d_hierarchy, g_r_core->proxy3d_root);
	// skip root stub 
	hierarchy_index_iterator_next_df(&it);
	while (it.count)
	{
		const u32 index = hierarchy_index_iterator_next_df(&it);
		struct r_proxy3d *proxy = internal_r_proxy3d_address(index);
		if (proxy->flags & PROXY3D_MOVING)
		{
			internal_r_proxy3d_local_speculative_orientation(proxy, ns_time);
		}

		if (proxy->header.parent != g_r_core->proxy3d_root)
		{
			const struct r_proxy3d *parent = r_proxy3d_address(proxy->header.parent);
			if ((proxy->flags & PROXY3D_MOVING) == 0)
			{
				vec3_copy(proxy->spec_position, proxy->position);	
				quat_copy(proxy->spec_rotation, proxy->rotation);	
			}

			vec3_translate(proxy->spec_position, parent->spec_position);
			quat tmp;
			quat_copy(tmp, proxy->spec_rotation);
			quat_mult(proxy->spec_rotation, tmp, parent->spec_rotation);
		}
	}
	hierarchy_index_iterator_release(&it);
}
