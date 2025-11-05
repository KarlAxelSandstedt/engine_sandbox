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

#include "led_local.h"

void cmd_led_node_add(void);
void cmd_led_node_remove(void);
void cmd_led_node_set_position(void);
void cmd_led_node_set_rb_prefab(void);
void cmd_led_node_set_csg_brush(void);
void cmd_led_node_set_proxy3d(void);

void cmd_collision_shape_add(void);
void cmd_collision_shape_remove(void);
void cmd_collision_box_add(void);
void cmd_collision_sphere_add(void);
void cmd_collision_capsule_add(void);

void cmd_rb_prefab_add(void);
void cmd_rb_prefab_remove(void);

void cmd_render_mesh_add(void);
void cmd_render_mesh_remove(void);

u32 cmd_led_node_add_id;
u32 cmd_led_node_remove_id;
u32 cmd_led_node_set_position_id;
u32 cmd_led_node_set_rb_prefab_id;
u32 cmd_led_node_set_csg_brush_id;
u32 cmd_led_node_set_proxy3d_id;

u32 cmd_rb_prefab_add_id;
u32 cmd_rb_prefab_remove_id;

u32 cmd_render_mesh_add_id;
u32 cmd_render_mesh_remove_id;

u32 cmd_collision_shape_add_id;
u32 cmd_collision_shape_remove_id;
u32 cmd_collision_box_add_id;
u32 cmd_collision_sphere_add_id;
u32 cmd_collision_capsule_add_id;

void led_core_init_commands(void)
{
	cmd_led_node_add_id = cmd_function_register(utf8_inline("led_node_add"), 1, &cmd_led_node_add).index;
	cmd_led_node_remove_id = cmd_function_register(utf8_inline("led_node_remove"), 1, &cmd_led_node_remove).index;
	cmd_led_node_set_position_id = cmd_function_register(utf8_inline("led_node_set_position"), 4, &cmd_led_node_set_position).index;
	cmd_led_node_set_rb_prefab_id = cmd_function_register(utf8_inline("led_node_set_rb_prefab"), 2, &cmd_led_node_set_rb_prefab).index;
	cmd_led_node_set_csg_brush_id = cmd_function_register(utf8_inline("led_node_set_csg_brush"), 2, &cmd_led_node_set_csg_brush).index;
	cmd_led_node_set_proxy3d_id = cmd_function_register(utf8_inline("led_node_set_proxy3d"), 2, &cmd_led_node_set_proxy3d).index;

	cmd_rb_prefab_add_id = cmd_function_register(utf8_inline("rb_prefab_add"), 6, &cmd_rb_prefab_add).index;
	cmd_rb_prefab_remove_id = cmd_function_register(utf8_inline("rb_prefab_remove"), 1, &cmd_rb_prefab_remove).index;

	cmd_render_mesh_add_id = cmd_function_register(utf8_inline("render_mesh_add"), 2, &cmd_render_mesh_add).index;
	cmd_render_mesh_remove_id = cmd_function_register(utf8_inline("render_mesh_remove"), 1, &cmd_render_mesh_remove).index;

	cmd_collision_shape_add_id = cmd_function_register(utf8_inline("collision_shape_add"), 1, &cmd_collision_shape_add).index;
	cmd_collision_box_add_id = cmd_function_register(utf8_inline("collision_box_add"), 4, &cmd_collision_box_add).index;
	cmd_collision_sphere_add_id = cmd_function_register(utf8_inline("collision_sphere_add"), 2, &cmd_collision_sphere_add).index;
	cmd_collision_capsule_add_id = cmd_function_register(utf8_inline("collision_capsule_add"), 3, &cmd_collision_capsule_add).index;
	cmd_collision_shape_remove_id = cmd_function_register(utf8_inline("collision_shape_remove"), 1, &cmd_collision_shape_remove).index;
}

void cmd_led_node_add(void)
{
	led_node_add(g_editor, g_queue->cmd_exec->arg[0].utf8);
}

void cmd_led_node_remove(void)
{
	led_node_remove(g_editor, g_queue->cmd_exec->arg[0].utf8);
}

void cmd_led_node_set_position(void)
{
	const vec3 position =
	{
		g_queue->cmd_exec->arg[1].f32,
		g_queue->cmd_exec->arg[2].f32,
		g_queue->cmd_exec->arg[3].f32,
	};
	led_node_set_position(g_editor, g_queue->cmd_exec->arg[0].utf8, position);
}

void cmd_led_node_set_rb_prefab(void)
{
	led_node_set_rb_prefab(g_editor, g_queue->cmd_exec->arg[0].utf8, g_queue->cmd_exec->arg[1].utf8);
}

void cmd_led_node_set_csg_brush(void)
{
	led_node_set_csg_brush(g_editor, g_queue->cmd_exec->arg[0].utf8, g_queue->cmd_exec->arg[1].utf8);
}

void cmd_led_node_set_proxy3d(void)
{
	led_node_set_proxy3d(g_editor, g_queue->cmd_exec->arg[0].utf8, g_queue->cmd_exec->arg[1].utf8);
}

void cmd_collision_shape_add(void)
{
	g_queue->cmd_exec->arg[1].f32 = 0.5f;
	g_queue->cmd_exec->arg[2].f32 = 0.5f;
	g_queue->cmd_exec->arg[3].f32 = 0.5f;
	cmd_collision_box_add();	
}

void cmd_collision_box_add(void)
{
	//TODO Need to come up with a memory allocation strategy for dcels
	struct system_window *sys_win = system_window_address(g_editor->window);
	const vec3 hw =
	{
		g_queue->cmd_exec->arg[1].f32,
		g_queue->cmd_exec->arg[2].f32,
		g_queue->cmd_exec->arg[3].f32,
	};
	struct collision_shape shape =
	{
		.id = g_queue->cmd_exec->arg[0].utf8,
		.type = COLLISION_SHAPE_CONVEX_HULL,
		.hull = collision_box(&sys_win->mem_persistent, hw), 
	};

	led_collision_shape_add(g_editor, &shape);
}

void cmd_collision_sphere_add(void)
{
	struct collision_shape shape =
	{
		.id = g_queue->cmd_exec->arg[0].utf8,
		.type = COLLISION_SHAPE_SPHERE,
		.sphere = { .radius = g_queue->cmd_exec->arg[1].f32 },
	};

	led_collision_shape_add(g_editor, &shape);
}

void cmd_collision_capsule_add(void)
{
	struct collision_shape shape =
	{
		.id = g_queue->cmd_exec->arg[0].utf8,
		.type = COLLISION_SHAPE_CAPSULE,
		.capsule = 
		{ 
			.radius = g_queue->cmd_exec->arg[1].f32,
			.p1 = { 0.0f, g_queue->cmd_exec->arg[2].f32/2.0f, 0.0f },
		},
	};

	led_collision_shape_add(g_editor, &shape);
}

void cmd_collision_shape_remove(void)
{
	led_collision_shape_remove(g_editor, g_queue->cmd_exec->arg[0].utf8);
}

struct slot led_collision_shape_add(struct led *led, const struct collision_shape *shape)
{
	struct slot slot = empty_slot;
	if (!shape->id.len)
	{
		log_string(T_LED, S_WARNING, "Failed to allocate collision shape: shape->id must not be empty");
	} 
	else if (string_database_lookup(&led->cs_db, shape->id).index != STRING_DATABASE_STUB_INDEX) 
	{
		log_string(T_LED, S_WARNING, "Failed to allocate collision shape: shape with given id already exist");
	}
	else
	{ 
		u8 *buf = thread_alloc_256B();
		const utf8 copy = utf8_copy_buffered(buf, 256, shape->id);	
		if (!copy.len)
		{
			log_string(T_LED, S_WARNING, "Failed to allocate collision shape: shape->id size must be <= 256B");
			thread_free_256B(buf);
		}
		else
		{
			slot = string_database_add_and_alias(&led->cs_db, copy);
			struct collision_shape *new_shape = slot.address;
			new_shape->type = shape->type;
			switch (shape->type)
			{
				case COLLISION_SHAPE_SPHERE: { new_shape->sphere = shape->sphere; } break;
				case COLLISION_SHAPE_CAPSULE: { new_shape->capsule = shape->capsule; } break;
				case COLLISION_SHAPE_CONVEX_HULL: { new_shape->hull = shape->hull; } break;
			};
		}
	}

	return slot;
}

void led_collision_shape_remove(struct led *led, const utf8 id)
{
	struct slot slot = led_collision_shape_lookup(led, id);
	struct collision_shape *shape = slot.address;
	if (slot.index != STRING_DATABASE_STUB_INDEX && shape->reference_count == 0)
	{
		void *buf = shape->id.buf;
		string_database_remove(&led->cs_db, id);
		thread_free_256B(buf);
	}
}

struct slot led_collision_shape_lookup(struct led *led, const utf8 id)
{
	return string_database_lookup(&led->cs_db, id);
}

void cmd_render_mesh_add(void)
{
	led_render_mesh_add(g_editor, g_queue->cmd_exec->arg[0].utf8, g_queue->cmd_exec->arg[1].utf8);
}

void cmd_render_mesh_remove(void)
{
	led_render_mesh_remove(g_editor, g_queue->cmd_exec->arg[0].utf8);
}

struct slot led_render_mesh_add(struct led *led, const utf8 id, const utf8 shape)
{
	struct slot slot = empty_slot;
	if (!id.len)
	{
		log_string(T_LED, S_WARNING, "Failed to allocate render mesh: id must not be empty");
	} 
	else if (string_database_lookup(&led->render_mesh_db, id).index != STRING_DATABASE_STUB_INDEX) 
	{
		log_string(T_LED, S_WARNING, "Failed to allocate render mesh: mesh with given id already exist");
	}
	else
	{ 
		u8 *buf = thread_alloc_256B();
		const utf8 copy = utf8_copy_buffered(buf, 256, id);	
		if (!copy.len)
		{
			log_string(T_LED, S_WARNING, "Failed to allocate render mesh: id size must be <= 256B");
			thread_free_256B(buf);
		}
		else
		{
			slot = string_database_add_and_alias(&led->render_mesh_db, copy);
			struct r_mesh *mesh = slot.address;

			struct slot ref = string_database_lookup(&led->cs_db, shape);
			if (ref.index == STRING_DATABASE_STUB_INDEX)
			{
				log_string(T_LED, S_WARNING, "In render_mesh_add: shape not found, stub_shape chosen");
			}

			struct system_window *sys_win = system_window_address(g_editor->window);
			const struct collision_shape *s = ref.address;
			switch (s->type)
			{
				case COLLISION_SHAPE_SPHERE: { r_mesh_set_sphere(&sys_win->mem_persistent, mesh, s->sphere.radius, 12); } break;
				case COLLISION_SHAPE_CAPSULE: { kas_assert(0); } break;
				case COLLISION_SHAPE_CONVEX_HULL: { r_mesh_set_hull(&sys_win->mem_persistent, mesh, &s->hull); } break;
			}
		}
	}

	return slot;
}

void led_render_mesh_remove(struct led *led, const utf8 id)
{
	struct slot slot = string_database_lookup(&led->render_mesh_db, id);
	struct r_mesh *mesh = slot.address;
	if (slot.index != STRING_DATABASE_STUB_INDEX && mesh->reference_count == 0)
	{
		void *buf = mesh->id.buf;
		string_database_remove(&led->render_mesh_db, id);
		thread_free_256B(buf);
	}
}

struct slot led_render_mesh_lookup(struct led *led, const utf8 id)
{
	return string_database_lookup(&led->render_mesh_db, id);
}

void cmd_rb_prefab_add(void)
{
	const utf8 id = g_queue->cmd_exec->arg[0].utf8;
	const utf8 shape = g_queue->cmd_exec->arg[1].utf8;
	const f32 density = g_queue->cmd_exec->arg[2].f32;
	const f32 restitution = g_queue->cmd_exec->arg[3].f32;
	const f32 friction = g_queue->cmd_exec->arg[4].f32;
	const u32 dynamic = (u32) g_queue->cmd_exec->arg[5].u64;

	led_rigid_body_prefab_add(g_editor, id, shape, density, restitution, friction, dynamic);
}

void cmd_rb_prefab_remove(void)
{
	led_rigid_body_prefab_remove(g_editor, g_queue->cmd_exec->arg[0].utf8);
}

struct slot led_rigid_body_prefab_add(struct led *led, const utf8 id, const utf8 shape, const f32 density, const f32 restitution, const f32 friction, const u32 dynamic)
{
	struct slot slot = empty_slot;	
	if (!id.len)
	{
		log_string(T_LED, S_WARNING, "Failed to allocate rb_prefab: prefab->id must not be empty");
	} 
	else if (string_database_lookup(&led->rb_prefab_db, id).index != STRING_DATABASE_STUB_INDEX) 
	{
		log_string(T_LED, S_WARNING, "Failed to allocate rb_prefab: prefab with given id already exist");
	}
	else
	{ 
		u8 *buf = thread_alloc_256B();
		const utf8 copy = utf8_copy_buffered(buf, 256, id);	
		if (!copy.len)
		{
			log_string(T_LED, S_WARNING, "Failed to allocate rb_prefab: prefab->id size must be <= 256B");
			thread_free_256B(buf);
		}
		else
		{
			struct slot ref = string_database_reference(&led->cs_db, shape);
			if (ref.index == STRING_DATABASE_STUB_INDEX)
			{
				log_string(T_LED, S_WARNING, "In rb_prefab: shape not found, stub_shape chosen");
			}
			slot = string_database_add_and_alias(&led->rb_prefab_db, copy);
			struct rigid_body_prefab *prefab = slot.address;

			prefab->shape = ref.index;
			prefab->restitution = restitution;
			prefab->friction = friction;
			prefab->dynamic = dynamic;
			prefab->density = density;
			prefab_statics_setup(prefab, ref.address, density);
		}
	}

	return slot;
}

void led_rigid_body_prefab_remove(struct led *led, const utf8 id)
{
	struct slot slot = led_rigid_body_prefab_lookup(led, id);
	struct rigid_body_prefab *prefab = slot.address;
	if (slot.index != STRING_DATABASE_STUB_INDEX && prefab->reference_count == 0)
	{
		void *buf = prefab->id.buf;
		string_database_dereference(&led->cs_db, prefab->shape);
		string_database_remove(&led->rb_prefab_db, id);
		thread_free_256B(buf);
	}	
}

struct slot led_rigid_body_prefab_lookup(struct led *led, const utf8 id)
{
	return string_database_lookup(&led->rb_prefab_db, id);
}

struct slot led_node_add(struct led *led, const utf8 id)
{
	struct slot slot = empty_slot;
	if (!id.len)
	{
		log_string(T_LED, S_WARNING, "Failed to allocate led_node: id must not be empty");
	} 
	else if (led_node_lookup(led, id).address != STRING_DATABASE_STUB_INDEX) 
	{
		log_string(T_LED, S_WARNING, "Failed to allocate led_node: node with given id already exist");
	}
	else
	{ 
		void *buf = thread_alloc_256B();
		const utf8 copy = utf8_copy_buffered(buf, 256, id);	
		const u32 key = utf8_hash(id);
		if (!copy.len)
		{
			log_string(T_LED, S_WARNING, "Failed to allocate led_node: id size must be <= 256B");
			thread_free_256B(buf);
		} 
		else
		{
			slot = gpool_add(&led->node_pool);
			hash_map_add(led->node_map, key, slot.index);
			dll_append(&led->node_non_marked_list, led->node_pool.buf, slot.index);
			dll_slot_set_not_in_list(&led->node_selected_list, slot.address);

			struct led_node *node = slot.address;
			node->flags = LED_FLAG_NONE;
			node->id = copy;
			node->key = key;
			node->cache = ui_node_cache_null();

			const vec3 axis = { 0.0f, 1.0f, 0.0f };
			vec3_set(node->position, 0.0f, 0.0f, 0.0f);
			axis_angle_to_quaternion(node->rotation, axis, 0.0f);

			node->rb_prefab = STRING_DATABASE_STUB_INDEX;
			node->proxy3d_unit = HI_NULL_INDEX;
			node->csg_brush = STRING_DATABASE_STUB_INDEX;
		}
	}

	return slot;
}

struct slot led_node_lookup(struct led *led, const utf8 id)
{
	const u32 key = utf8_hash(id);
	struct slot slot = empty_slot;
	for (u32 i = hash_map_first(led->node_map, key); i != HASH_NULL; i = hash_map_next(led->node_map, i))
	{
		struct led_node *node = gpool_address(&led->node_pool, i);
		if (utf8_equivalence(id, node->id))
		{
			slot.index = i;
			slot.address = node;
			break;
		}
	}

	return slot;
}

static void led_remove_marked_structs(struct led *led)
{
	struct led_node *node = NULL;
	for (u32 i = led->node_marked_list.first; i != DLL_NULL; i = DLL_NEXT(node))
	{
		node = gpool_address(&led->node_pool, i);
		if (node->flags & LED_CONSTANT)
		{
			node->flags &= ~LED_MARKED_FOR_REMOVAL;
			dll_remove(&led->node_marked_list, led->node_pool.buf, i);
			dll_append(&led->node_non_marked_list, led->node_pool.buf, i);
			continue;
		}

		if (DLL2_IN_LIST(node))
		{
			fprintf(stderr, "Was selected, removing from list - ");
			utf8_debug_print(node->id);
			dll_remove(&led->node_selected_list, led->node_pool.buf, i);
		}

		if (node->proxy3d_unit != HI_NULL_INDEX)
		{
			r_unit_dealloc(&led->frame, node->proxy3d_unit);
		}

		string_database_dereference(&led->rb_prefab_db, node->rb_prefab);
		string_database_dereference(&led->csg.brush_db, node->csg_brush);

		node->rb_prefab = STRING_DATABASE_STUB_INDEX;
		node->csg_brush = STRING_DATABASE_STUB_INDEX;
		node->proxy3d_unit = HI_NULL_INDEX;

		hash_map_remove(led->node_map, node->key, i);
		thread_free_256B(node->id.buf);
		gpool_remove(&led->node_pool, i);
	}

	dll_flush(&led->node_marked_list);
}

void led_node_remove(struct led *led, const utf8 id)
{
	struct slot slot = led_node_lookup(led, id);
	struct led_node *node = slot.address;
	if (node)
	{
		node->flags |= LED_MARKED_FOR_REMOVAL;
		dll_remove(&led->node_non_marked_list, led->node_pool.buf, slot.index);
		dll_append(&led->node_marked_list, led->node_pool.buf, slot.index);
	}
}

void led_node_set_position(struct led *led, const utf8 id, const vec3 position)
{
	struct slot slot = led_node_lookup(led, id);
	struct led_node *node = slot.address;
	if (!node)
	{
		log(T_LED, S_WARNING, "Failed to set position of led node %k, node not found.", &id);
	}
	else if (node->flags & LED_CONSTANT)
	{
		log(T_LED, S_WARNING, "Failed to set position of led node %k, node is constant.", &id);
	}
	else
	{
		vec3_copy(node->position, position);
	}
}

void led_node_set_rb_prefab(struct led *led, const utf8 id, const utf8 prefab)
{
	struct slot slot = led_node_lookup(led, id);
	struct led_node *node = slot.address;
	if (!node)
	{
		log(T_LED, S_WARNING, "Failed to set of led node %k, node not found.", &id);
	}
	else if (node->flags & LED_CONSTANT)
	{
		log(T_LED, S_WARNING, "Failed to set of led node %k, node is constant.", &id);
	}
	else
	{
		slot = string_database_reference(&led->rb_prefab_db, prefab);
		if (slot.index == STRING_DATABASE_STUB_INDEX)
		{
			log(T_LED, S_WARNING, "Failed to set of led node %k, prefab not found.", &id);
		}
		else
		{
			string_database_dereference(&led->rb_prefab_db, node->rb_prefab);
			string_database_dereference(&led->csg.brush_db, node->csg_brush);
			node->csg_brush = STRING_DATABASE_STUB_INDEX;

			node->rb_prefab = slot.index;
			node->flags &= ~(LED_CSG | LED_PHYSICS);
			node->flags |= LED_PHYSICS;
		}
	}
}

void led_node_set_csg_brush(struct led *led, const utf8 id, const utf8 brush)
{
	struct slot slot = led_node_lookup(led, id);
	struct led_node *node = slot.address;
	if (!node)
	{
		log(T_LED, S_WARNING, "Failed to set of led node %k, node not found.", &id);
	}
	else if (node->flags & LED_CONSTANT)
	{
		log(T_LED, S_WARNING, "Failed to set of led node %k, node is constant.", &id);
	}
	else
	{
		slot = string_database_reference(&led->csg.brush_db, brush);
		if (slot.index == STRING_DATABASE_STUB_INDEX)
		{
			log(T_LED, S_WARNING, "Failed to set of led node %k, brush not found.", &id);
		}
		else
		{
			string_database_dereference(&led->rb_prefab_db, node->rb_prefab);
			string_database_dereference(&led->csg.brush_db, node->csg_brush);
			node->rb_prefab = STRING_DATABASE_STUB_INDEX;

			node->csg_brush = slot.index;
			node->flags &= ~(LED_CSG | LED_PHYSICS);
			node->flags |= LED_CSG;
		}
	}
}

void led_node_set_proxy3d(struct led *led, const utf8 id, const utf8 mesh)
{
	struct slot slot = led_node_lookup(led, id);
	struct led_node *node = slot.address;
	if (!node)
	{
		log(T_LED, S_WARNING, "Failed to set of led node %k, node not found.", &id);
	}
	else if (node->flags & LED_CONSTANT)
	{
		log(T_LED, S_WARNING, "Failed to set of led node %k, node is constant.", &id);
	}
	else
	{
		if (node->proxy3d_unit != HI_NULL_INDEX)
		{
			struct arena tmp = arena_alloc_1MB();
			r_unit_dealloc(&tmp, node->proxy3d_unit);
			node->proxy3d_unit = HI_NULL_INDEX;
			arena_free_1MB(&tmp);
		}

		struct r_proxy3d_config config =
		{
			.parent = HI_ROOT_STUB_INDEX,
			.linear_velocity = { 0.0f, 0.0f, 0.0f },
			.angular_velocity = { 0.0f, 0.0f, 0.0f },
			.mesh = mesh,
			.ns_time = led->ns,
		};
		vec3_copy(config.position, node->position);
		quat_copy(config.rotation, node->rotation);
		node->proxy3d_unit = r_proxy3d_alloc(&config);
	}
}

void led_wall_smash_simulation_setup(struct led *led)
{
	struct system_window *sys_win = system_window_address(g_editor->window);

	const u32 tower1_box_count = 40;
	const u32 tower2_box_count = 10;
	const u32 pyramid_layers = 10;
	const u32 bodies = tower1_box_count + tower2_box_count + 3 + pyramid_layers*(pyramid_layers+1) / 2;

	/* Setup rigid bodies */
	const f32 box_friction = 0.8f;
	const f32 ramp_friction = 0.1f;
	const f32 sphere_friction = 0.1f;
	const f32 floor_friction = 0.8f;

	const f32 alpha1 = 0.7f;
	const f32 alpha2 = 0.5f;
	const vec4 tower1_color = { 154.0f/256.0f, 101.0f/256.0f, 182.0f/256.0f,alpha1 };
	const vec4 tower2_color = { 54.0f/256.0f, 183.0f/256.0f, 122.0f/256.0f, alpha2 };
	const vec4 pyramid_color = { 254.0f/256.0f, 181.0f/256.0f, 82.0f/256.0f,alpha2 };
	const vec4 floor_color = { 0.8f, 0.6f, 0.6f,                            alpha2 };
	const vec4 ramp_color = { 165.0f/256.0f, 242.0f/256.0f, 243.0f/256.0f,  alpha2 };
	const vec4 sphere_color = { 0.2f, 0.9f, 0.5f,                             alpha1 };

	const f32 box_side = 1.0f;
	struct AABB box_aabb = { .center = { 0.0f, 0.0f, 0.0f }, .hw = { box_side / 2.0f, box_side / 4.0f, box_side / 2.0f} };

	const f32 ramp_width = 10.0f;
	const f32 ramp_length = 60.0f;
	const f32 ramp_height = 34.0f;
	const u32 v_count = 6;
	vec3 ramp_vertices[6] = 
	{
		{0.0f, 		ramp_height,	-ramp_length},
		{ramp_width, 	ramp_height, 	-ramp_length},
		{0.0f, 		0.0f, 		-ramp_length},
		{ramp_width, 	0.0f, 		-ramp_length},
		{0.0f, 		0.0f, 		0.0f},
		{ramp_width, 	0.0f, 		0.0f},
	};

	sys_win->cmd_queue->regs[0].utf8 = utf8_cstr(sys_win->ui->mem_frame, "c_floor");
	sys_win->cmd_queue->regs[1].f32 = 8.0f * ramp_width;
	sys_win->cmd_queue->regs[2].f32 = 0.5f;
	sys_win->cmd_queue->regs[3].f32 = ramp_length;
	cmd_queue_submit(sys_win->cmd_queue, cmd_collision_box_add_id);

	sys_win->cmd_queue->regs[0].utf8 = utf8_cstr(sys_win->ui->mem_frame, "c_box");
	sys_win->cmd_queue->regs[1].f32 = box_side / 2.0f;
	sys_win->cmd_queue->regs[2].f32 = box_side / 4.0f;
	sys_win->cmd_queue->regs[3].f32 = box_side / 2.0f;
	cmd_queue_submit(sys_win->cmd_queue, cmd_collision_box_add_id);

	sys_win->cmd_queue->regs[0].utf8 = utf8_cstr(sys_win->ui->mem_frame, "c_sphere");
	sys_win->cmd_queue->regs[1].f32 = 2.0f;
	cmd_queue_submit(sys_win->cmd_queue, cmd_collision_sphere_add_id);

	//TODO
	//const u32 ramp_handle = physics_pipeline_cs_alloc(&game->physics);
	//struct collision_shape *ramp = physics_pipeline_cs_lookup(&game->physics, ramp_handle);
	//ramp->type = COLLISION_SHAPE_CONVEX_HULL;
	//ramp->hull = collision_hull_construct(mem_persistent, 
	//	&mem_tmp.arenas[0], 
	//	&mem_tmp.arenas[1], 
	//	&mem_tmp.arenas[2], 
	//	&mem_tmp.arenas[3], 
	//	&mem_tmp.arenas[4], 
	//	ramp_vertices,
	//       	v_count,
	//       	0.001f);

	sys_win->cmd_queue->regs[0].utf8 = utf8_cstr(sys_win->ui->mem_frame, "rb_floor");
	sys_win->cmd_queue->regs[1].utf8 = utf8_cstr(sys_win->ui->mem_frame, "c_floor");
	sys_win->cmd_queue->regs[2].f32 = 1.0f;
	sys_win->cmd_queue->regs[3].f32 = 0.0f;
	sys_win->cmd_queue->regs[4].f32 = 0.0f;
	sys_win->cmd_queue->regs[5].u32 = 0;
	cmd_queue_submit(sys_win->cmd_queue, cmd_rb_prefab_add_id);

	sys_win->cmd_queue->regs[0].utf8 = utf8_cstr(sys_win->ui->mem_frame, "rb_box");
	sys_win->cmd_queue->regs[1].utf8 = utf8_cstr(sys_win->ui->mem_frame, "c_box");
	sys_win->cmd_queue->regs[2].f32 = 1.0f;
	sys_win->cmd_queue->regs[3].f32 = 0.0f;
	sys_win->cmd_queue->regs[4].f32 = box_friction;
	sys_win->cmd_queue->regs[5].u32 = 1;
	cmd_queue_submit(sys_win->cmd_queue, cmd_rb_prefab_add_id);

	sys_win->cmd_queue->regs[0].utf8 = utf8_cstr(sys_win->ui->mem_frame, "rb_sphere");
	sys_win->cmd_queue->regs[1].utf8 = utf8_cstr(sys_win->ui->mem_frame, "c_sphere");
	sys_win->cmd_queue->regs[2].f32 = 100.0f;
	sys_win->cmd_queue->regs[3].f32 = 0.0f;
	sys_win->cmd_queue->regs[4].f32 = sphere_friction;
	sys_win->cmd_queue->regs[5].u32 = 1;
	cmd_queue_submit(sys_win->cmd_queue, cmd_rb_prefab_add_id);

	sys_win->cmd_queue->regs[0].utf8 = utf8_cstr(sys_win->ui->mem_frame, "rb_ramp");
	sys_win->cmd_queue->regs[1].utf8 = utf8_cstr(sys_win->ui->mem_frame, "c_ramp");
	sys_win->cmd_queue->regs[2].f32 = 1.0f;
	sys_win->cmd_queue->regs[3].f32 = 0.0f;
	sys_win->cmd_queue->regs[4].f32 = ramp_friction;
	sys_win->cmd_queue->regs[5].u32 = 0;
	cmd_queue_submit(sys_win->cmd_queue, cmd_rb_prefab_add_id);

	sys_win->cmd_queue->regs[0].utf8 = utf8_cstr(sys_win->ui->mem_frame, "rm_floor");
	sys_win->cmd_queue->regs[1].utf8 = utf8_cstr(sys_win->ui->mem_frame, "c_floor");
	cmd_queue_submit(sys_win->cmd_queue, cmd_render_mesh_add_id);

	sys_win->cmd_queue->regs[0].utf8 = utf8_cstr(sys_win->ui->mem_frame, "rm_ramp");
	sys_win->cmd_queue->regs[1].utf8 = utf8_cstr(sys_win->ui->mem_frame, "c_ramp");
	cmd_queue_submit(sys_win->cmd_queue, cmd_render_mesh_add_id);

	sys_win->cmd_queue->regs[0].utf8 = utf8_cstr(sys_win->ui->mem_frame, "rm_tower1");
	sys_win->cmd_queue->regs[1].utf8 = utf8_cstr(sys_win->ui->mem_frame, "c_box");
	cmd_queue_submit(sys_win->cmd_queue, cmd_render_mesh_add_id);
	
	sys_win->cmd_queue->regs[0].utf8 = utf8_cstr(sys_win->ui->mem_frame, "rm_tower2");
	sys_win->cmd_queue->regs[1].utf8 = utf8_cstr(sys_win->ui->mem_frame, "c_box");
	cmd_queue_submit(sys_win->cmd_queue, cmd_render_mesh_add_id);

	sys_win->cmd_queue->regs[0].utf8 = utf8_cstr(sys_win->ui->mem_frame, "rm_box");
	sys_win->cmd_queue->regs[1].utf8 = utf8_cstr(sys_win->ui->mem_frame, "c_box");
	cmd_queue_submit(sys_win->cmd_queue, cmd_render_mesh_add_id);

	sys_win->cmd_queue->regs[0].utf8 = utf8_cstr(sys_win->ui->mem_frame, "rm_sphere");
	sys_win->cmd_queue->regs[1].utf8 = utf8_cstr(sys_win->ui->mem_frame, "c_sphere");
	cmd_queue_submit(sys_win->cmd_queue, cmd_render_mesh_add_id);

	vec3 sphere_translation = { -0.5, 0.5f + ramp_height, -ramp_length };
	vec3 box_translation =  {-0.5f, 0.0f, -0.5f};
	vec3 ramp_translation = {-ramp_width/2.0f, -ramp_width/2.0f, -ramp_width/2.0f};
	vec3 floor_translation = { 0.0f, -ramp_width/2.0f - 1.0f, ramp_length / 2.0f -ramp_width/2.0f};
	vec3 box_base_translation = { 0.0f, floor_translation[1] + 1.0f, floor_translation[2] / 2.0f};

	//const vec3 y_axis = { 0.0f, 1.0f, 0.0f };
	//axis_angle_to_quaternion(config.rotation, y_axis, 0.0f);
	//vec3_set(config.linear_velocity, 0.0f, 0.0f, 0.0f);
	//vec3_set(config.angular_velocity, 0.0f, 0.0f, 0.0f);

	sys_win->cmd_queue->regs[0].utf8 = utf8_cstr(sys_win->ui->mem_frame, "led_floor");
	cmd_queue_submit(sys_win->cmd_queue, cmd_led_node_add_id);
	sys_win->cmd_queue->regs[0].utf8 = utf8_cstr(sys_win->ui->mem_frame, "led_floor");
	sys_win->cmd_queue->regs[1].f32 = floor_translation[0];
	sys_win->cmd_queue->regs[2].f32 = floor_translation[1];
	sys_win->cmd_queue->regs[3].f32 = floor_translation[2];
	cmd_queue_submit(sys_win->cmd_queue, cmd_led_node_set_position_id);
	sys_win->cmd_queue->regs[0].utf8 = utf8_cstr(sys_win->ui->mem_frame, "led_floor");
	sys_win->cmd_queue->regs[1].utf8 = utf8_cstr(sys_win->ui->mem_frame, "rb_floor");
	cmd_queue_submit(sys_win->cmd_queue, cmd_led_node_set_rb_prefab_id);
	sys_win->cmd_queue->regs[0].utf8 = utf8_cstr(sys_win->ui->mem_frame, "led_floor");
	sys_win->cmd_queue->regs[1].utf8 = utf8_cstr(sys_win->ui->mem_frame, "rm_floor");
	cmd_queue_submit(sys_win->cmd_queue, cmd_led_node_set_proxy3d_id);
//	vec4_copy(config.color, floor_color);
//	e->r_handle = proxy3d_alloc(R_UNIT_PARENT_NONE, R_CMD_SCREEN_LAYER_GAME, R_CMD_TRANSPARENCY_OPAQUE, TEXTURE_NONE, R_UNIT_DRAW_COLOR, &config);

	sys_win->cmd_queue->regs[0].utf8 = utf8_cstr(sys_win->ui->mem_frame, "led_ramp");
	cmd_queue_submit(sys_win->cmd_queue, cmd_led_node_add_id);
	sys_win->cmd_queue->regs[0].utf8 = utf8_cstr(sys_win->ui->mem_frame, "led_ramp");
	sys_win->cmd_queue->regs[1].f32 = ramp_translation[0];
	sys_win->cmd_queue->regs[2].f32 = ramp_translation[1];
	sys_win->cmd_queue->regs[3].f32 = ramp_translation[2];
	cmd_queue_submit(sys_win->cmd_queue, cmd_led_node_set_position_id);
	sys_win->cmd_queue->regs[0].utf8 = utf8_cstr(sys_win->ui->mem_frame, "led_ramp");
	sys_win->cmd_queue->regs[1].utf8 = utf8_cstr(sys_win->ui->mem_frame, "rb_ramp");
	cmd_queue_submit(sys_win->cmd_queue, cmd_led_node_set_rb_prefab_id);
	sys_win->cmd_queue->regs[0].utf8 = utf8_cstr(sys_win->ui->mem_frame, "led_ramp");
	sys_win->cmd_queue->regs[1].utf8 = utf8_cstr(sys_win->ui->mem_frame, "rm_ramp");
	cmd_queue_submit(sys_win->cmd_queue, cmd_led_node_set_proxy3d_id);
//	vec4_copy(config.color, ramp_color);
//	e->r_handle = proxy3d_alloc(R_UNIT_PARENT_NONE, R_CMD_SCREEN_LAYER_GAME, R_CMD_TRANSPARENCY_OPAQUE, TEXTURE_NONE, R_UNIT_DRAW_COLOR, &config);

	sys_win->cmd_queue->regs[0].utf8 = utf8_cstr(sys_win->ui->mem_frame, "led_sphere");
	cmd_queue_submit(sys_win->cmd_queue, cmd_led_node_add_id);
	sys_win->cmd_queue->regs[0].utf8 = utf8_cstr(sys_win->ui->mem_frame, "led_sphere");
	sys_win->cmd_queue->regs[1].f32 = sphere_translation[0];
	sys_win->cmd_queue->regs[2].f32 = sphere_translation[1];
	sys_win->cmd_queue->regs[3].f32 = sphere_translation[2];
	cmd_queue_submit(sys_win->cmd_queue, cmd_led_node_set_position_id);
	sys_win->cmd_queue->regs[0].utf8 = utf8_cstr(sys_win->ui->mem_frame, "led_sphere");
	sys_win->cmd_queue->regs[1].utf8 = utf8_cstr(sys_win->ui->mem_frame, "rb_sphere");
	cmd_queue_submit(sys_win->cmd_queue, cmd_led_node_set_rb_prefab_id);
	sys_win->cmd_queue->regs[0].utf8 = utf8_cstr(sys_win->ui->mem_frame, "led_sphere");
	sys_win->cmd_queue->regs[1].utf8 = utf8_cstr(sys_win->ui->mem_frame, "rm_sphere");
	cmd_queue_submit(sys_win->cmd_queue, cmd_led_node_set_proxy3d_id);
//	vec4_copy(config.color, sphere_color);
//	e->r_handle = proxy3d_alloc(R_UNIT_PARENT_NONE, R_CMD_SCREEN_LAYER_GAME, R_CMD_TRANSPARENCY_ADDITIVE, TEXTURE_NONE, R_UNIT_DRAW_COLOR | R_UNIT_DRAW_TRANSPARENT, &config);
	
	for (u32 i = 0; i < pyramid_layers; ++i)
	{
		const f32 local_y = i * box_side;
		for (u32 j = 0; j < pyramid_layers-i; ++j)
		{
			const f32 local_x = j -(pyramid_layers-i-1) * box_side / 2.0f;
			vec3 translation;
			vec3_copy(translation, box_base_translation);
			translation[0] += local_x;
			translation[1] += local_y;

			utf8 id = utf8_format(sys_win->ui->mem_frame, "pyramid_%u_%u", i, j);
			sys_win->cmd_queue->regs[0].utf8 = id;
			cmd_queue_submit(sys_win->cmd_queue, cmd_led_node_add_id);
			sys_win->cmd_queue->regs[0].utf8 = id;
			sys_win->cmd_queue->regs[1].f32 = translation[0];
			sys_win->cmd_queue->regs[2].f32 = translation[1];
			sys_win->cmd_queue->regs[3].f32 = translation[2];
			cmd_queue_submit(sys_win->cmd_queue, cmd_led_node_set_position_id);
			sys_win->cmd_queue->regs[0].utf8 = id;
			sys_win->cmd_queue->regs[1].utf8 = utf8_cstr(sys_win->ui->mem_frame, "rb_box");
			cmd_queue_submit(sys_win->cmd_queue, cmd_led_node_set_rb_prefab_id);
			sys_win->cmd_queue->regs[0].utf8 = id;
			sys_win->cmd_queue->regs[1].utf8 = utf8_cstr(sys_win->ui->mem_frame, "rm_box");
			cmd_queue_submit(sys_win->cmd_queue, cmd_led_node_set_proxy3d_id);

			//vec4_copy(config.color, pyramid_color);
			//e->r_handle = proxy3d_alloc(R_UNIT_PARENT_NONE, R_CMD_SCREEN_LAYER_GAME, R_CMD_TRANSPARENCY_OPAQUE, TEXTURE_NONE, R_UNIT_DRAW_COLOR, &config);
		}
	}

	for (u32 i = 0; i < tower1_box_count; ++i)
	{
		vec3 translation;
		vec3_copy(translation, box_base_translation);
		translation[2] += 15.0f;
		translation[1] += (f32) i * box_aabb.hw[1] * 2.10f;
		translation[0] += 15.0f;

		utf8 id = utf8_format(sys_win->ui->mem_frame, "tower1_%u", i);
		sys_win->cmd_queue->regs[0].utf8 = id;
		cmd_queue_submit(sys_win->cmd_queue, cmd_led_node_add_id);
		sys_win->cmd_queue->regs[0].utf8 = id;
		sys_win->cmd_queue->regs[1].f32 = translation[0];
		sys_win->cmd_queue->regs[2].f32 = translation[1];
		sys_win->cmd_queue->regs[3].f32 = translation[2];
		cmd_queue_submit(sys_win->cmd_queue, cmd_led_node_set_position_id);
		sys_win->cmd_queue->regs[0].utf8 = id;
		sys_win->cmd_queue->regs[1].utf8 = utf8_cstr(sys_win->ui->mem_frame, "rb_box");
		cmd_queue_submit(sys_win->cmd_queue, cmd_led_node_set_rb_prefab_id);
		sys_win->cmd_queue->regs[0].utf8 = id;
		sys_win->cmd_queue->regs[1].utf8 = utf8_cstr(sys_win->ui->mem_frame, "rm_box");
		cmd_queue_submit(sys_win->cmd_queue, cmd_led_node_set_proxy3d_id);

		//vec4_copy(config.color, tower1_color);
		//e->r_handle = proxy3d_alloc(R_UNIT_PARENT_NONE, R_CMD_SCREEN_LAYER_GAME, R_CMD_TRANSPARENCY_OPAQUE, TEXTURE_NONE, R_UNIT_DRAW_COLOR, &config);
	}

	for (u32 i = 0; i < tower2_box_count; ++i)
	{
		vec3 translation;
		vec3_copy(translation, box_base_translation);
		translation[2] += 15.0f;
		translation[1] += (f32) i * box_aabb.hw[1] * 2.10f;
		translation[0] -= 15.0f;
	
		utf8 id = utf8_format(sys_win->ui->mem_frame, "tower2_%u", i);
		sys_win->cmd_queue->regs[0].utf8 = id;
		cmd_queue_submit(sys_win->cmd_queue, cmd_led_node_add_id);
		sys_win->cmd_queue->regs[0].utf8 = id;
		sys_win->cmd_queue->regs[1].f32 = translation[0];
		sys_win->cmd_queue->regs[2].f32 = translation[1];
		sys_win->cmd_queue->regs[3].f32 = translation[2];
		cmd_queue_submit(sys_win->cmd_queue, cmd_led_node_set_position_id);
		sys_win->cmd_queue->regs[0].utf8 = id;
		sys_win->cmd_queue->regs[1].utf8 = utf8_cstr(sys_win->ui->mem_frame, "rb_box");
		cmd_queue_submit(sys_win->cmd_queue, cmd_led_node_set_rb_prefab_id);
		sys_win->cmd_queue->regs[0].utf8 = id;
		sys_win->cmd_queue->regs[1].utf8 = utf8_cstr(sys_win->ui->mem_frame, "rm_box");
		cmd_queue_submit(sys_win->cmd_queue, cmd_led_node_set_proxy3d_id);
	
		//vec4_copy(config.color, tower2_color);
		//e->r_handle = proxy3d_alloc(R_UNIT_PARENT_NONE, R_CMD_SCREEN_LAYER_GAME, R_CMD_TRANSPARENCY_ADDITIVE, TEXTURE_NONE, R_UNIT_DRAW_COLOR | R_UNIT_DRAW_TRANSPARENT, &config);
	}
}

void led_core(struct led *led)
{
	KAS_TASK(__func__, T_LED);

	//TODO fix 
	static u32 once = 1;
	struct system_window *sys_win = system_window_address(g_editor->window);
	if (once && sys_win)
	{
		once = 0;
		led_wall_smash_simulation_setup(led);
	}
	led_remove_marked_structs(led);

	KAS_END;
}
