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

void cmd_collision_shape_add(void);
void cmd_collision_shape_remove(void);
void cmd_collision_box_add(void);
void cmd_collision_sphere_add(void);
void cmd_collision_capsule_add(void);

u32 cmd_led_node_add_id;
u32 cmd_led_node_remove_id;

u32 cmd_collision_shape_add_id;
u32 cmd_collision_shape_remove_id;
u32 cmd_collision_box_add_id;
u32 cmd_collision_sphere_add_id;
u32 cmd_collision_capsule_add_id;

void led_core_init_commands(void)
{
	cmd_led_node_add_id = cmd_function_register(utf8_inline("led_node_add"), 1, &cmd_led_node_add).index;
	cmd_led_node_remove_id = cmd_function_register(utf8_inline("led_node_remove"), 1, &cmd_led_node_remove).index;
	cmd_collision_shape_add_id = cmd_function_register(utf8_inline("cmd_collision_shape_add"), 1, &cmd_collision_shape_add).index;
	cmd_collision_box_add_id = cmd_function_register(utf8_inline("cmd_collision_box_add"), 4, &cmd_collision_box_add).index;
	cmd_collision_sphere_add_id = cmd_function_register(utf8_inline("cmd_collision_sphere_add"), 2, &cmd_collision_sphere_add).index;
	cmd_collision_capsule_add_id = cmd_function_register(utf8_inline("cmd_collision_capsule_add"), 3, &cmd_collision_capsule_add).index;
	cmd_collision_shape_remove_id = cmd_function_register(utf8_inline("cmd_collision_shape_remove"), 1, &cmd_collision_shape_remove).index;
}

void cmd_led_node_add(void)
{
	led_node_add(g_editor, g_queue->cmd_exec->arg[0].utf8);
}

void cmd_led_node_remove(void)
{
	led_node_remove(g_editor, g_queue->cmd_exec->arg[0].utf8);
}

void cmd_led_physics_prefab_add(void)
{
	led_node_add(g_editor, g_queue->cmd_exec->arg[0].utf8);
}

void cmd_led_physics_prefab_remove(void)
{
	led_node_remove(g_editor, g_queue->cmd_exec->arg[0].utf8);
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
		g_queue->cmd_exec->arg[1].f32 = 0.5f,
		g_queue->cmd_exec->arg[2].f32 = 0.5f,
		g_queue->cmd_exec->arg[3].f32 = 0.5f,
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
		.type = COLLISION_SHAPE_CONVEX_HULL,
		.sphere = { .radius = g_queue->cmd_exec->arg[0].f32 },
	};

	led_collision_shape_add(g_editor, &shape);
}

void cmd_collision_capsule_add(void)
{
	struct collision_shape shape =
	{
		.id = g_queue->cmd_exec->arg[0].utf8,
		.type = COLLISION_SHAPE_CONVEX_HULL,
		.capsule = 
		{ 
			.radius = g_queue->cmd_exec->arg[0].f32,
			.p1 = { 0.0f, g_queue->cmd_exec->arg[1].f32/2.0f, 0.0f },
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
		log_string(T_LED, S_WARNING, "Failed to allocate collision_shape: shape->id must not be empty");
	} 
	else if (shape->id.size > 256)
	{
		log_string(T_LED, S_WARNING, "Failed to allocate collision_shape: shape->id size must be <= 256B");
	} 
	else if (string_database_lookup(&led->collision_shape_db, shape->id).address == NULL) 
	{ 
		u8 *buf = thread_alloc_256B();
		const utf8 copy = utf8_copy_buffered(buf, 256, shape->id);	
		string_database_add(NULL, &led->collision_shape_db, copy);

		struct collision_shape *new_shape = slot.address;
		new_shape->type = shape->type;
		switch (shape->type)
		{
			case COLLISION_SHAPE_SPHERE: { new_shape->sphere = shape->sphere; } break;
			case COLLISION_SHAPE_CAPSULE: { new_shape->capsule = shape->capsule; } break;
			case COLLISION_SHAPE_CONVEX_HULL: { new_shape->hull = shape->hull; } break;
		};
	}

	return slot;
}

void led_collision_shape_remove(struct led *led, const utf8 id)
{
	struct slot slot = led_collision_shape_lookup(led, id);
	struct collision_shape *shape = slot.address;
	if (shape->reference_count == 0)
	{
		string_database_remove(&led->collision_shape_db, id);
	}
}

struct slot led_collision_shape_lookup(struct led *led, const utf8 id)
{
	return string_database_lookup(&led->collision_shape_db, id);
}

struct slot led_node_add(struct led *led, const utf8 id)
{
	struct slot slot = empty_slot;
	if (!id.len)
	{
		log_string(T_LED, S_WARNING, "Failed to allocate led_node: id must not be empty");
	} 
	else if (id.size > 256)
	{
		log_string(T_LED, S_WARNING, "Failed to allocate led_node: id size must be <= 256B");
	} 
	else if (led_node_lookup(led, id).address == NULL) 
	{ 
		void *buf = thread_alloc_256B();
		const utf8 copy = utf8_copy_buffered(buf, 256, id);	
		const u32 key = utf8_hash(id);

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

		node->physics_handle = POOL_NULL;
		node->render_handle = POOL_NULL;
		node->csg_handle = POOL_NULL;
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
	if ((node) && (POOL_SLOT_ALLOCATED(node)))
	{
		node->flags |= LED_MARKED_FOR_REMOVAL;
		dll_remove(&led->node_non_marked_list, led->node_pool.buf, slot.index);
		dll_append(&led->node_marked_list, led->node_pool.buf, slot.index);
	}
}

void led_wall_smash_simulation_setup(struct led *led)
{
	struct system_window *sys_win = system_window_lookup(g_editor->window).address;

	struct arena tmp1 = arena_alloc_1MB();

	struct r_proxy3d_draw_config config =
	{
		.blend = 0.5f, 
		.transparency = 0.5f,
		.spec_mov = PROXY3D_SPEC_LINEAR,
		.pos_type = PROXY3D_POSITION_RELATIVE,
	};

	const u32 tower1_box_count = 40;
	const u32 tower2_box_count = 10;
	const u32 pyramid_layers = 10;
	const u32 bodies = tower1_box_count + tower2_box_count + 3 + pyramid_layers*(pyramid_layers+1) / 2;

	/* Setup rigid bodies */
	const f32 box_friction = 0.8f;
	const f32 ramp_friction = 0.1f;
	const f32 ball_friction = 0.1f;
	const f32 floor_friction = 0.8f;

	const f32 alpha1 = 0.7f;
	const f32 alpha2 = 0.5f;
	const vec4 tower1_color = { 154.0f/256.0f, 101.0f/256.0f, 182.0f/256.0f,alpha1 };
	const vec4 tower2_color = { 54.0f/256.0f, 183.0f/256.0f, 122.0f/256.0f, alpha2 };
	const vec4 pyramid_color = { 254.0f/256.0f, 181.0f/256.0f, 82.0f/256.0f,alpha2 };
	const vec4 floor_color = { 0.8f, 0.6f, 0.6f,                            alpha2 };
	const vec4 ramp_color = { 165.0f/256.0f, 242.0f/256.0f, 243.0f/256.0f,  alpha2 };
	const vec4 ball_color = { 0.2f, 0.9f, 0.5f,                             alpha1 };

	const f32 box_side = 1.0f;

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

	sys_win->cmd_queue->cmd_exec->arg[0].utf8 = utf8_inline("floor");
	sys_win->cmd_queue->cmd_exec->arg[1].f32 = 8.0f * ramp_width;
	sys_win->cmd_queue->cmd_exec->arg[2].f32 = 0.5f;
	sys_win->cmd_queue->cmd_exec->arg[3].f32 = ramp_length;
	cmd_queue_submit(sys_win->cmd_queue, cmd_collision_box_add_id);

	sys_win->cmd_queue->cmd_exec->arg[0].utf8 = utf8_inline("box");
	sys_win->cmd_queue->cmd_exec->arg[1].f32 = box_side / 2.0f;
	sys_win->cmd_queue->cmd_exec->arg[2].f32 = box_side / 4.0f;
	sys_win->cmd_queue->cmd_exec->arg[3].f32 = box_side / 2.0f;
	cmd_queue_submit(sys_win->cmd_queue, cmd_collision_box_add_id);

	sys_win->cmd_queue->cmd_exec->arg[0].utf8 = utf8_inline("ball");
	sys_win->cmd_queue->cmd_exec->arg[1].f32 = 2.0f;
	cmd_queue_submit(sys_win->cmd_queue, cmd_collision_sphere_add_id);

	//TODO
	//const u32 ramp_handle = physics_pipeline_collision_shape_alloc(&game->physics);
	//struct collision_shape *ramp = physics_pipeline_collision_shape_lookup(&game->physics, ramp_handle);
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

//	const kas_string floor_id = KAS_COMPILE_TIME_STRING("floor");
//	const u32 r_floor_handle = r_mesh_alloc(mem_persistent, &floor_id);
//	struct r_mesh *mesh_floor = r_mesh_address(r_floor_handle);
//	r_mesh_set_hull(mem_persistent, mesh_floor, &floor->hull);
//
//	const kas_string ramp_id = KAS_COMPILE_TIME_STRING("ramp");
//	const u32 r_ramp_handle = r_mesh_alloc(mem_persistent, &ramp_id);
//	struct r_mesh *mesh_ramp = r_mesh_address(r_ramp_handle);
//	r_mesh_set_hull(mem_persistent, mesh_ramp, &ramp->hull);
//	
//	const kas_string tower1_id = KAS_COMPILE_TIME_STRING("tower1");
//	const u32 r_tower1_handle = r_mesh_alloc(mem_persistent, &tower1_id);
//	struct r_mesh *mesh_tower1 = r_mesh_address(r_tower1_handle);
//	r_mesh_set_hull(mem_persistent, mesh_tower1, &box->hull);
//
//	const kas_string tower2_id = KAS_COMPILE_TIME_STRING("tower2");
//	const u32 r_tower2_handle = r_mesh_alloc(mem_persistent, &tower2_id);
//	struct r_mesh *mesh_tower2 = r_mesh_address(r_tower2_handle);
//	r_mesh_set_hull(mem_persistent, mesh_tower2, &box->hull);
//
//	const kas_string pyramid_id = KAS_COMPILE_TIME_STRING("pyramid");
//	const u32 r_pyramid_handle = r_mesh_alloc(mem_persistent, &pyramid_id);
//	struct r_mesh *mesh_pyramid = r_mesh_address(r_pyramid_handle);
//	r_mesh_set_hull(mem_persistent, mesh_pyramid, &box->hull);
//
//	const kas_string ball_id = KAS_COMPILE_TIME_STRING("ball");
//	const u32 r_ball_handle = r_mesh_alloc(mem_persistent, &ball_id);
//	struct r_mesh *mesh_ball = r_mesh_address(r_ball_handle);
//	r_mesh_set_sphere(mem_persistent, mesh_ball, ball->sphere.radius, 12);
//
//	vec3 ball_translation = { -0.5, 0.5f + ramp_height, -ramp_length };
//	vec3 box_translation =  {-0.5f, 0.0f, -0.5f};
//	vec3 ramp_translation = {-ramp_width/2.0f, -ramp_width/2.0f, -ramp_width/2.0f};
//	vec3 floor_translation = { 0.0f, -ramp_width/2.0f - 1.0f, ramp_length / 2.0f -ramp_width/2.0f};
//	vec3 box_base_translation = { 0.0f, floor_translation[1] + 1.0f, floor_translation[2] / 2.0f};
//
//	const vec3 y_axis = { 0.0f, 1.0f, 0.0f };
//	axis_angle_to_quaternion(config.rotation, y_axis, 0.0f);
//	vec3_set(config.linear_velocity, 0.0f, 0.0f, 0.0f);
//	vec3_set(config.angular_velocity, 0.0f, 0.0f, 0.0f);
//
//	config.mesh_id_alias = &floor_id;
//	vec4_copy(config.color, floor_color);
//	vec3_copy(config.position, floor_translation);
//	index = array_list_intrusive_reserve_index(game->entity_list);
//	e = array_list_intrusive_address(game->entity_list, index);
//	e->r_handle = r_proxy3d_alloc(R_UNIT_PARENT_NONE, R_CMD_SCREEN_LAYER_GAME, R_CMD_TRANSPARENCY_OPAQUE, TEXTURE_NONE, R_UNIT_DRAW_COLOR, &config);
//	e->physics_handle = physics_pipeline_rigid_body_add(&game->physics, floor_handle, floor_translation, 1.0f, 0, 0.0f, floor_friction);
//	hash_map_add(game->physics_handle_to_entity_map, e->physics_handle, index);
//#ifdef	KAS_PHYSICS_DEBUG
//	struct rigid_body *body = array_list_intrusive_address(game->physics.body_list, e->physics_handle);
//	vec4_copy(body->color, floor_color);
//#endif
//
//	config.mesh_id_alias = &ramp_id;
//	vec4_copy(config.color, ramp_color);
//	vec3_copy(config.position, ramp_translation);
//	index = array_list_intrusive_reserve_index(game->entity_list);
//	e = array_list_intrusive_address(game->entity_list, index);
//	e->r_handle = r_proxy3d_alloc(R_UNIT_PARENT_NONE, R_CMD_SCREEN_LAYER_GAME, R_CMD_TRANSPARENCY_OPAQUE, TEXTURE_NONE, R_UNIT_DRAW_COLOR, &config);
//	e->physics_handle = physics_pipeline_rigid_body_add(&game->physics, ramp_handle, ramp_translation, 1.0f, 0, 0.0f, ramp_friction);
//	hash_map_add(game->physics_handle_to_entity_map, e->physics_handle, index);
//#ifdef	KAS_PHYSICS_DEBUG
//	body = array_list_intrusive_address(game->physics.body_list, e->physics_handle);
//	vec4_copy(body->color, ramp_color);
//#endif
//	config.mesh_id_alias = &ball_id;
//	vec4_copy(config.color, ball_color);
//	vec3_copy(config.position, ball_translation);
//	index = array_list_intrusive_reserve_index(game->entity_list);
//	e = array_list_intrusive_address(game->entity_list, index);
//	e->r_handle = r_proxy3d_alloc(R_UNIT_PARENT_NONE, R_CMD_SCREEN_LAYER_GAME, R_CMD_TRANSPARENCY_ADDITIVE, TEXTURE_NONE, R_UNIT_DRAW_COLOR | R_UNIT_DRAW_TRANSPARENT, &config);
//	e->physics_handle = physics_pipeline_rigid_body_add(&game->physics, ball_handle, ball_translation, 100.0f, 1, 0.0f, ball_friction);
//	hash_map_add(game->physics_handle_to_entity_map, e->physics_handle, index);
//#ifdef	KAS_PHYSICS_DEBUG
//	body = array_list_intrusive_address(game->physics.body_list, e->physics_handle);
//	vec4_copy(body->color, ball_color);
//#endif
//	
//	for (u32 i = 0; i < pyramid_layers; ++i)
//	{
//		const f32 local_y = i * box_side;
//		for (u32 j = 0; j < pyramid_layers-i; ++j)
//		{
//			const f32 local_x = j -(pyramid_layers-i-1) * box_side / 2.0f;
//			vec3 translation;
//			vec3_copy(translation, box_base_translation);
//			translation[0] += local_x;
//			translation[1] += local_y;
//			vec3_copy(config.position, translation);
//
//			config.mesh_id_alias = &pyramid_id;
//			vec4_copy(config.color, pyramid_color);
//			index = array_list_intrusive_reserve_index(game->entity_list);
//			e = array_list_intrusive_address(game->entity_list, index);
//			e->r_handle = r_proxy3d_alloc(R_UNIT_PARENT_NONE, R_CMD_SCREEN_LAYER_GAME, R_CMD_TRANSPARENCY_OPAQUE, TEXTURE_NONE, R_UNIT_DRAW_COLOR, &config);
//			e->physics_handle = physics_pipeline_rigid_body_add(&game->physics, box_handle, translation, 1.0f, 1, 0.0f, box_friction);
//			hash_map_add(game->physics_handle_to_entity_map, e->physics_handle, index);
//#ifdef	KAS_PHYSICS_DEBUG
//			body = array_list_intrusive_address(game->physics.body_list, e->physics_handle);
//			vec4_copy(body->color, pyramid_color);
//#endif
//		}
//	}
//
//	for (u32 i = 0; i < tower1_box_count; ++i)
//	{
//		vec3 translation;
//		vec3_copy(translation, box_base_translation);
//		translation[2] += 15.0f;
//		translation[1] += (f32) i * box_aabb.hw[1] * 2.10f;
//		translation[0] += 15.0f;
//
//		config.mesh_id_alias = &tower1_id;
//		vec4_copy(config.color, tower1_color);
//		vec3_copy(config.position, translation);
//		index = array_list_intrusive_reserve_index(game->entity_list);
//		e = array_list_intrusive_address(game->entity_list, index);
//		e->r_handle = r_proxy3d_alloc(R_UNIT_PARENT_NONE, R_CMD_SCREEN_LAYER_GAME, R_CMD_TRANSPARENCY_OPAQUE, TEXTURE_NONE, R_UNIT_DRAW_COLOR, &config);
//		e->physics_handle = physics_pipeline_rigid_body_add(&game->physics, box_handle, translation, 1.0f, 1, 0.0f, box_friction);
//		hash_map_add(game->physics_handle_to_entity_map, e->physics_handle, index);
//#ifdef	KAS_PHYSICS_DEBUG
//			body = array_list_intrusive_address(game->physics.body_list, e->physics_handle);
//			vec4_copy(body->color, tower1_color);
//#endif
//	}
//
//	for (u32 i = 0; i < tower2_box_count; ++i)
//	{
//		vec3 translation;
//		vec3_copy(translation, box_base_translation);
//		translation[2] += 15.0f;
//		translation[1] += (f32) i * box_aabb.hw[1] * 2.10f;
//		translation[0] -= 15.0f;
//		
//		config.mesh_id_alias = &tower2_id;
//		vec4_copy(config.color, tower2_color);
//		index = array_list_intrusive_reserve_index(game->entity_list);
//		vec3_copy(config.position, translation);
//		e = array_list_intrusive_address(game->entity_list, index);
//		e->r_handle = r_proxy3d_alloc(R_UNIT_PARENT_NONE, R_CMD_SCREEN_LAYER_GAME, R_CMD_TRANSPARENCY_ADDITIVE, TEXTURE_NONE, R_UNIT_DRAW_COLOR | R_UNIT_DRAW_TRANSPARENT, &config);
//		e->physics_handle = physics_pipeline_rigid_body_add(&game->physics, box_handle, translation, 1.0f, 1, 0.0f, box_friction);
//		hash_map_add(game->physics_handle_to_entity_map, e->physics_handle, index);
//#ifdef	KAS_PHYSICS_DEBUG
//			body = array_list_intrusive_address(game->physics.body_list, e->physics_handle);
//			vec4_copy(body->color, tower2_color);
//#endif
//	}

	arena_free_1MB(&tmp1);
}
void led_core(struct led *led)
{
	KAS_TASK(__func__, T_LED);

	//TODO fix 
	static u32 once = 1;
	struct system_window *sys_win = system_window_lookup(g_editor->window).address;
	if (once && sys_win)
	{
		once = 0;
		led_wall_smash_simulation_setup(led);
	}
	led_remove_marked_structs(led);

	KAS_END;
}


