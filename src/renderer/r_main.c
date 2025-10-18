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
#include "r_public.h"
#include "asset_public.h"
#include "transform.h"

//static void internal_proxy3d_update_r_commands(void)
//{
//	KAS_TASK(__func__, T_RENDERER);
//
//	const u32 depth_exponent = 1 + f32_exponent_bits(g_r_core->cam.fz_far);
//	kas_assert(depth_exponent >= 23);
//
//	r_proxy3d_hierarchy_speculate(&g_r_core->frame_proxy3d_position, &g_r_core->frame_proxy3d_rotation, &g_r_core->frame, g_r_core->ns_elapsed);
//	
//	struct hierarchy_index_iterator it = hierarchy_index_iterator_init(&g_r_core->frame, g_r_core->unit_hierarchy, 0);
//	// skip root stub 
//	hierarchy_index_iterator_next_df(&it);
//	while (it.count)
//	{
//		const u32 index = hierarchy_index_iterator_next_df(&it);
//		struct r_unit *unit = r_unit_lookup(index);
//		if (unit->type == R_UNIT_TYPE_PROXY3D && (unit->draw_flags & R_UNIT_DRAW_TRANSPARENT))
//		{
//			const f32 dist = vec3_distance(g_r_core->frame_proxy3d_position[unit->type_index], g_r_core->cam.position);
//			const u32 unit_exponent = f32_exponent_bits(dist);
//			const u64 depth = (unit_exponent <= depth_exponent && unit_exponent > (depth_exponent - 23))
//				? (0x00800000 | f32_mantissa_bits(dist)) >> (depth_exponent - unit_exponent + 1)
//				: 0;
//			r_command_update_depth(index, depth);
//		}
//	}
//	hierarchy_index_iterator_release(&it);
//
//	KAS_END;
//}

//static void internal_push_uniforms(const u32 window)
//{
//	vec2u32 win_size;
//	system_window_size(win_size, window);
//
//	mat4 perspective, view;
//	struct r_camera *cam = &g_r_core->cam;
//	cam->aspect_ratio =  (f32) win_size[0] / win_size[1];
//	perspective_matrix(perspective, cam->aspect_ratio, cam->fov_x, cam->fz_near, cam->fz_far);
//	view_matrix(view, cam->position, cam->left, cam->up, cam->forward);
//
//	kas_glUseProgram(g_r_core->program[PROGRAM_PROXY3D].gl_program);
//	GLint aspect_ratio_addr, view_addr, perspective_addr, light_position_addr;
//	aspect_ratio_addr = glGetUniformLocation(g_r_core->program[PROGRAM_PROXY3D].gl_program, "aspect_ratio");
//	view_addr = glGetUniformLocation(g_r_core->program[PROGRAM_PROXY3D].gl_program, "view");
//	perspective_addr = glGetUniformLocation(g_r_core->program[PROGRAM_PROXY3D].gl_program, "perspective");
//	light_position_addr = glGetUniformLocation(g_r_core->program[PROGRAM_PROXY3D].gl_program, "light_position");
//	glUniform1f(aspect_ratio_addr, (f32) cam->aspect_ratio);
//	glUniform3f(light_position_addr, cam->position[0], cam->position[1], cam->position[2]);
//	glUniformMatrix4fv(perspective_addr, 1, GL_FALSE, (f32 *) perspective);
//	glUniformMatrix4fv(view_addr, 1, GL_FALSE, (f32 *) view);
//
//	kas_glUseProgram(g_r_core->program[PROGRAM_COLOR].gl_program);
//	aspect_ratio_addr = glGetUniformLocation(g_r_core->program[PROGRAM_COLOR].gl_program, "aspect_ratio");
//	view_addr = glGetUniformLocation(g_r_core->program[PROGRAM_COLOR].gl_program, "view");
//	perspective_addr = glGetUniformLocation(g_r_core->program[PROGRAM_COLOR].gl_program, "perspective");
//	glUniform1f(aspect_ratio_addr, (f32) cam->aspect_ratio);
//	glUniform3f(light_position_addr, cam->position[0], cam->position[1], cam->position[2]);
//	glUniformMatrix4fv(perspective_addr, 1, GL_FALSE, (f32 *) perspective);
//	glUniformMatrix4fv(view_addr, 1, GL_FALSE, (f32 *) view);
//}

static void internal_r_ui_uniforms(const u32 window)
{
	vec2u32 resolution;
	system_window_size(resolution, window);

	kas_glUseProgram(g_r_core->program[PROGRAM_UI].gl_program);
	GLint resolution_addr = kas_glGetUniformLocation(g_r_core->program[PROGRAM_UI].gl_program, "resolution");
	kas_glUniform2f(resolution_addr, (f32) resolution[0], (f32) resolution[1]);
}

static void r_scene_render(const u32 window)
{
	KAS_TASK(__func__, T_RENDERER);

	struct system_window *sys_win = system_window_address(window);
	kas_glViewport(0, 0, (i32) sys_win->size[0], (i32) sys_win->size[1]); 

	kas_glClearColor(0.08f, 0.08f, 0.08f, 1.0f);
	kas_glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	for (struct r_bucket *b = sys_win->r_scene->frame_bucket_list; b; b = b->next)
	{
		KAS_TASK("render bucket", T_RENDERER);
		switch (b->screen_layer)
		{
			case R_CMD_SCREEN_LAYER_GAME:
			{
				kas_glEnableDepthTesting();
			} break;

			case R_CMD_SCREEN_LAYER_HUD:
			{
				kas_glDisableDepthTesting();
			} break;

			default:
			{
				kas_assert_string(0, "unimplemented");
			} break;
		}

		switch (b->transparency)
		{
			case R_CMD_TRANSPARENCY_OPAQUE:
			{
				kas_glDisableBlending();
			} break;

			case R_CMD_TRANSPARENCY_ADDITIVE:
			{
				kas_glEnableBlending();
				kas_glBlendEquation(GL_FUNC_ADD);
			} break;

			case R_CMD_TRANSPARENCY_SUBTRACTIVE:
			{
				kas_glEnableBlending();
				kas_glBlendEquation(GL_FUNC_SUBTRACT);
			} break;

			default: 
			{ 
				 kas_assert_string(0, "unexpected transparency setting"); 
			} break;
		}
		
		const u32 program = MATERIAL_PROGRAM_GET(b->material);
		kas_glUseProgram(g_r_core->program[program].gl_program);

		const u32 texture = MATERIAL_TEXTURE_GET(b->material);	
		switch (program)
		{
			case PROGRAM_UI:
			{
				u32 tx_index = 0;
				kas_glActiveTexture(GL_TEXTURE0 + tx_index);
				//TODO setup compile time arrays as with g_r_core->program[program].gl_program (?????)
				kas_glBindTexture(GL_TEXTURE_2D, g_r_core->texture[texture].handle);
				const i32 texture_addr = kas_glGetUniformLocation(g_r_core->program[program].gl_program, "texture");
				kas_glUniform1i(texture_addr, tx_index);
			} break;

			default:
			{
				kas_assert_string(0, "unimplemented");
			} break;
		}

		GLenum mode;
		switch (b->primitive)
		{
			case R_CMD_PRIMITIVE_LINE: { mode = GL_LINES; } break;
			case R_CMD_PRIMITIVE_TRIANGLE: { mode = GL_TRIANGLES; } break;
			default: { kas_assert_string(0, "Unexpected draw primitive"); } break;
		}

		u32 vao;
		kas_glGenVertexArrays(1, &vao);
		kas_glBindVertexArray(vao);
		for (u32 i = 0; i < b->buffer_count; ++i)
		{	
			struct r_buffer *buf = b->buffer_array[i];
			kas_glGenBuffers(1, &buf->ebo);
			kas_glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buf->ebo);
			kas_glBufferData(GL_ELEMENT_ARRAY_BUFFER, buf->index_count * sizeof(u32), buf->index_data, GL_STATIC_DRAW);
			kas_glGenBuffers(1, &buf->local_vbo);
			kas_glBindBuffer(GL_ARRAY_BUFFER, buf->local_vbo);
			kas_glBufferData(GL_ARRAY_BUFFER, buf->local_size, buf->local_data, GL_STATIC_DRAW);
			g_r_core->program[program].buffer_local_layout_setter();

			if (!b->instanced)
			{
				//fprintf(stderr, "\t\tDrawing Regular: buf[%u], vbuf[%lu], index_count: %u\n", 
				//		i,
				//		buf->local_size,
				//		buf->index_count);

				kas_glDrawElements(mode, buf->index_count, GL_UNSIGNED_INT, 0);
			}
			else
			{
				//fprintf(stderr, "\t\tDrawing Instanced: buf[%u], sbuf[%lu], vbuf[%lu], index_count: %u, instace_count: %u\n", 
				//		i,
				//		buf->shared_size,
				//		buf->local_size,
				//		buf->index_count,
				//		buf->instance_count);

				kas_glGenBuffers(1, &buf->shared_vbo);
				kas_glBindBuffer(GL_ARRAY_BUFFER, buf->shared_vbo);
				kas_glBufferData(GL_ARRAY_BUFFER, buf->shared_size, buf->shared_data, GL_STATIC_DRAW);
				g_r_core->program[program].buffer_shared_layout_setter();

				kas_glDrawElementsInstanced(mode, buf->index_count, GL_UNSIGNED_INT, 0, buf->instance_count);
				kas_glDeleteBuffers(1, &buf->shared_vbo);
			}	

			kas_glDeleteBuffers(1, &buf->local_vbo);
			kas_glDeleteBuffers(1, &buf->ebo);
		}

		kas_glDeleteVertexArrays(1, &vao);
		KAS_END;
	}

	system_window_swap_gl_buffers(window);
	GL_STATE_ASSERT;
	KAS_END;
}

//void r_main(struct system_window *sys_win, const struct game *game)
//{
//	KAS_TASK(__func__, T_RENDERER);
//
//	const u64 ns_tick = game->ns_process - g_r_core->ns_elapsed;
//	g_r_core->ns_elapsed = game->ns_process;
//	arena_flush(&g_r_core->frame);
//
//	/* CAMERA UPDATE  */
//	vec3 cam_local_velocity = { 0.0f, 0.0f, 0.0f };
//	if (bit_vec_get_bit(&g_input_state->key_pressed, KAS_A) || bit_vec_get_bit(&g_input_state->key_pressed, KAS_a))
//	{
//				cam_local_velocity[0] += 9.0f; 
//	}
//
//	if (bit_vec_get_bit(&g_input_state->key_pressed, KAS_D) || bit_vec_get_bit(&g_input_state->key_pressed, KAS_d))
//	{
//				cam_local_velocity[0] += -9.0f; 
//	}
//
//	if (bit_vec_get_bit(&g_input_state->key_pressed, KAS_W) || bit_vec_get_bit(&g_input_state->key_pressed, KAS_w))
//	{
//				cam_local_velocity[2] += 9.0f; 
//	}
//
//	if (bit_vec_get_bit(&g_input_state->key_pressed, KAS_S) || bit_vec_get_bit(&g_input_state->key_pressed, KAS_s))
//	{
//				cam_local_velocity[2] += -9.0f; 
//	}
//
//	const f32 delta = (f32) ns_tick / NSEC_PER_SEC;
//	g_r_core->cam.position[0] += (f32) delta * (cam_local_velocity[0] * g_r_core->cam.left[0] +  cam_local_velocity[2] * g_r_core->cam.forward[0]);
//	g_r_core->cam.position[1] += (f32) delta * (cam_local_velocity[1] + cam_local_velocity[2] * g_r_core->cam.forward[1]);
//	g_r_core->cam.position[2] += (f32) delta * (cam_local_velocity[0] * g_r_core->cam.left[2] +  cam_local_velocity[2] * g_r_core->cam.forward[2]);
//
//
//	if (g_r_core->ns_tick)
//	{
//		const u64 frames_elapsed_since_last_draw = (g_r_core->ns_elapsed - (g_r_core->frames_elapsed * g_r_core->ns_tick)) / g_r_core->ns_tick;
//		if (frames_elapsed_since_last_draw)
//		{
//			g_r_core->frames_elapsed += frames_elapsed_since_last_draw;
//
//			//fprintf(stderr, "game.ns_elapsed:     %lu\n", game->ns_elapsed);
//			//fprintf(stderr, "physics.ns_elapsed:  %lu\n", game->physics.ns_elapsed);
//			//fprintf(stderr, "renderer.ns_elapsed: %lu\n", g_r_core->ns_elapsed);
//			kas_assert(g_r_core->ns_elapsed >= game->physics.ns_elapsed);
//			
//			//TODO MUST DO THIS INSIDE DRAW LOOP, must bind programs before uploading uniforms!
//			internal_proxy3d_update_r_commands();
//			//r_widget_init_r_units();
//			
//			r_core_cmd_sort(&g_r_core->frame);
//			g_r_core->frame_bucket_list = r_core_generate_bucket_list(&g_r_core->frame);
//			for (struct r_bucket *b = g_r_core->frame_bucket_list; b; b = b->next)
//			{
//				r_core_bucket_generate_draw_calls(&g_r_core->frame, b);
//			}
//
//			internal_push_uniforms(sys_win);
//	
//			r_draw(sys_win);
//			//r_widget_remove_r_units();
//			g_r_core->frame_bucket_list = NULL;
//			g_r_core->frame_proxy3d_position = NULL;	
//			g_r_core->frame_proxy3d_rotation = NULL;	
//		}
//	}
//	else
//	{
//		kas_assert(0);
//		g_r_core->frames_elapsed += 1;
//	}
//
//	KAS_END;
//}

void r_led_main(const struct led *led)
{
	KAS_TASK(__func__, T_RENDERER);

	const u64 ns_tick = led->ns - g_r_core->ns_elapsed;
	g_r_core->ns_elapsed = led->ns;
	arena_flush(&g_r_core->frame);
	const f32 delta = (f32) ns_tick / NSEC_PER_SEC;
	
	if (g_r_core->ns_tick)
	{
		const u64 frames_elapsed_since_last_draw = (g_r_core->ns_elapsed - (g_r_core->frames_elapsed * g_r_core->ns_tick)) / g_r_core->ns_tick;
		if (frames_elapsed_since_last_draw)
		{
			g_r_core->frames_elapsed += frames_elapsed_since_last_draw;

			//TODO update proxy3ds
			//TODO MUST DO THIS INSIDE DRAW LOOP, must bind programs before uploading uniforms!
			//TODO r_widget_init_r_units();

			struct system_window *win = NULL;
			struct arena tmp = arena_alloc_1MB();
			struct hierarchy_index_iterator	it = hierarchy_index_iterator_init(&tmp, g_window_hierarchy, g_process_root_window);
			while (it.count)
			{
				const u32 window = hierarchy_index_iterator_next_df(&it);
				win = system_window_address(window);
				if (!win->tagged_for_destruction)
				{
					system_window_set_current_gl_context(window);
					system_window_set_global(window);

					r_scene_set(win->r_scene);
					r_scene_frame_begin();
					{
						r_ui_draw(win->ui);
					}
					r_scene_frame_end();
					internal_r_ui_uniforms(window);
					r_scene_render(window);
				}
			}
			hierarchy_index_iterator_release(&it);
			arena_free_1MB(&tmp);

			/* NOTE: main context must be set in the case of creating new contexts sharing state. */
			system_window_set_current_gl_context(g_process_root_window);
		}
	}
	else
	{
		kas_assert(0);
		g_r_core->frames_elapsed += 1;
	}

	KAS_END;
}
