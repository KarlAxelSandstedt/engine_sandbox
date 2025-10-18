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

struct r_scene *g_scene = NULL;

struct r_scene *r_scene_alloc(void)
{
	struct r_scene *scene = malloc(sizeof(struct r_scene));
	scene->mem_frame_arr[0] = arena_alloc(1024*1024*1024);
	scene->mem_frame_arr[1] = arena_alloc(1024*1024*1024);
	scene->mem_frame = scene->mem_frame_arr + 0;
	scene->frame = 0;

	scene->unit_to_instance_map = hash_map_alloc(NULL, 4096, 4096, HASH_GROWABLE);
	scene->instance_list = array_list_intrusive_alloc(NULL, 4096, sizeof(struct r_instance), ARRAY_LIST_GROWABLE);

	scene->instance_new_first = U32_MAX;

	scene->cmd_cache = NULL;
	scene->cmd_cache_count = 0;
	scene->cmd_frame = NULL;
	scene->cmd_frame_count = 0;

	return scene;
}

void r_scene_free(struct r_scene *scene)
{
	array_list_intrusive_free(scene->instance_list);
	hash_map_free(scene->unit_to_instance_map);
	arena_free(scene->mem_frame_arr + 0),
	arena_free(scene->mem_frame_arr + 1),
	free(scene);
}

void r_scene_set(struct r_scene *scene)
{
	g_scene = scene;
}

void r_scene_frame_begin(void)
{
	g_scene->frame += 1;
	g_scene->mem_frame = g_scene->mem_frame_arr + (g_scene->frame & 0x1);	
	
	g_scene->instance_new_first = U32_MAX;
	g_scene->cmd_new_count = 0;
	g_scene->cmd_cache = g_scene->cmd_frame;
	g_scene->cmd_cache_count = g_scene->cmd_frame_count;
	g_scene->cmd_frame = NULL;
	g_scene->cmd_frame_count = 0;
	g_scene->frame_bucket_list = NULL;

	arena_flush(g_scene->mem_frame);
}

/* merge unit [left, mid-1], [mid, right-1] => tmp => copy back to unit */
static void internal_r_command_merge(struct r_command *r_cmd, struct r_command *tmp, const u32 left, const u32 mid, const u32 right)
{
	u32 l = left;
	u32 r = mid;
	const u32 count = right - left;

	for (u32 i = left; i < right; ++i)
	{
		if (r < right && (l >= mid || r_cmd[r].key > r_cmd[l].key))
		{
			tmp[i] = r_cmd[r];
			r += 1;
		}
		else
		{
			tmp[i] = r_cmd[l];
			l += 1;
		}
	}

	memcpy(r_cmd + left, tmp + left, count * sizeof(struct r_command));
}

#ifdef KAS_DEBUG

void r_scene_assert_cmd_sorted(void)
{
	u32 sorted = 1;
	for (u32 i = 1; i < g_scene->cmd_frame_count; ++i)
	{
		//fprintf(stderr, "depth: %lu\n", R_CMD_DEPTH_GET(g_scene->cmd_frame[i-1].key));
		if (g_scene->cmd_frame[i-1].key < g_scene->cmd_frame[i].key)
		{
			//fprintf(stderr, "depth: %lu\n", R_CMD_DEPTH_GET(g_scene->cmd_frame[i].key));
			sorted = 0;	
			break;
		}
	}

	kas_assert_string(sorted, "r_scene assertion failed: draw commands not sorted");
}

void r_scene_assert_instance_cmd_bijection(void)
{
	for (u32 i = 0; i < g_scene->cmd_frame_count; ++i)
	{
		struct r_command *cmd = g_scene->cmd_frame + i;
		struct r_instance *instance = array_list_intrusive_address(g_scene->instance_list, cmd->instance);
		kas_assert(instance->header.allocated);
		kas_assert(((u64) instance->cmd - (u64) cmd) == 0);
	}
}

#define R_SCENE_ASSERT_CMD_SORTED		r_scene_assert_cmd_sorted()
#define	R_SCENE_ASSERT_INSTANCE_CMD_BIJECTION	r_scene_assert_instance_cmd_bijection()

#else

#define R_SCENE_ASSERT_CMD_SORTED		
#define	R_SCENE_ASSERT_INSTANCE_CMD_BIJECTION

#endif

static void r_scene_sort_commands_and_prune_instances(void)
{
	KAS_TASK(__func__, T_RENDERER);
	g_scene->cmd_frame = arena_push(g_scene->mem_frame, g_scene->cmd_frame_count * sizeof(struct r_command));
	arena_push_record(g_scene->mem_frame);
	struct r_command *cmd_new = arena_push(g_scene->mem_frame, g_scene->cmd_new_count * sizeof(struct r_command));
	struct r_command *cmd_tmp = arena_push(g_scene->mem_frame, g_scene->cmd_new_count * sizeof(struct r_command));
	struct r_instance *new_instance = array_list_intrusive_address(g_scene->instance_list, g_scene->instance_new_first);
	for (u32 i = 0; i < g_scene->cmd_new_count; ++i)
	{
		cmd_new[i] = *new_instance->cmd;
		new_instance = array_list_intrusive_address(g_scene->instance_list, new_instance->header.next);
	}

	/* Sort newly added commands */
	for (u32 width = 2; width/2 < g_scene->cmd_new_count; width *= 2)
	{
		u32 i = 0;
		for (; i + width <= g_scene->cmd_new_count; i += width)
		{
			//fprintf(stderr, "merging       : [%u, %u] and [%u, %u]\n", i, i + width/2 - 1, i + width/2, i + width - 1);
			internal_r_command_merge(cmd_new, cmd_tmp, i, i + width/2, i + width);
		}

		if (i + width/2 < g_scene->cmd_new_count)
		{
			internal_r_command_merge(cmd_new, cmd_tmp, i, i + width/2, g_scene->cmd_new_count);
			//fprintf(stderr, "merging (last): [%u, %u] and [%u, %u]\n", i, i + width/2 - 1, i + width/2, add_count-1);
		}
	}

	/* (3) sort key_cache with new keys, remove any untouched instances */
	u32 cache_i = 0;
	u32 new_i = 0;
	for (u32 i = 0; i < g_scene->cmd_frame_count; ++i)
	{
		for (; cache_i < g_scene->cmd_cache_count; cache_i++) 
		{
			const u32 index = g_scene->cmd_cache[cache_i].instance;
			struct r_instance *cached_instance = array_list_intrusive_address(g_scene->instance_list, index);
			if (cached_instance->frame_last_touched != g_scene->frame)
			{
				if (cached_instance->type == R_INSTANCE_UNIT)
				{
					hash_map_remove(g_scene->unit_to_instance_map, (u32) cached_instance->unit, index);
				}
				array_list_intrusive_remove(g_scene->instance_list, cached_instance);
				g_scene->cmd_cache[cache_i].allocated = 0;
			}

			if (g_scene->cmd_cache[cache_i].allocated)
			{
				break;
			}
		}

		if (cache_i >= g_scene->cmd_cache_count) 
		{
			g_scene->cmd_frame[i] = cmd_new[new_i++];
		}
		else if (new_i >= g_scene->cmd_new_count || g_scene->cmd_cache[cache_i].key >= cmd_new[new_i].key)
		{
			g_scene->cmd_frame[i] = g_scene->cmd_cache[cache_i++];
		}
		else
		{
			g_scene->cmd_frame[i] = cmd_new[new_i++];
		}

		struct r_instance *instance = array_list_intrusive_address(g_scene->instance_list, g_scene->cmd_frame[i].instance);
		instance->cmd = g_scene->cmd_frame + i;
	}

	/* (4) remove any last untouched instances */
	for (; cache_i < g_scene->cmd_cache_count; cache_i++) 
	{
		const u32 index = g_scene->cmd_cache[cache_i].instance;
		struct r_instance *cached_instance = array_list_intrusive_address(g_scene->instance_list, index);
		if (cached_instance->frame_last_touched != g_scene->frame)
		{
			if (cached_instance->type == R_INSTANCE_UNIT)
			{
				hash_map_remove(g_scene->unit_to_instance_map, cached_instance->unit, index);
			}
			array_list_intrusive_remove(g_scene->instance_list, cached_instance);
			g_scene->cmd_cache[cache_i].allocated = 0;
		}
	}
	arena_pop_record(g_scene->mem_frame);
	R_SCENE_ASSERT_CMD_SORTED;
	R_SCENE_ASSERT_INSTANCE_CMD_BIJECTION;

	KAS_END;
}

void r_buffer_constructor_reset(struct r_buffer_constructor *constructor)
{
	constructor->count = 0;
	constructor->first = NULL;
	constructor->last = NULL;
}

void r_buffer_constructor_buffer_alloc(struct r_buffer_constructor *constructor, const u32 c_new_l)
{
	struct r_buffer *buf = arena_push(g_scene->mem_frame, sizeof(struct r_buffer));
	buf->next = NULL;
	buf->c_l = c_new_l;
	buf->local_size = 0;
	buf->shared_size = 0;
	buf->index_count = 0;
	buf->instance_count = 0;

	if (constructor->count == 0)
	{
		constructor->first = buf;
	}
	else
	{
		constructor->last->next = buf;
		constructor->last->c_h = c_new_l-1;
	}

	constructor->last = buf;
	constructor->count += 1;
}

void r_buffer_constructor_buffer_add_size(struct r_buffer_constructor *constructor, const u64 local_size, const u64 shared_size, const u32 instance_count, const u32 index_count)
{
	kas_assert(constructor->count);
	constructor->last->local_size += local_size;
	constructor->last->shared_size += shared_size;
	constructor->last->instance_count += instance_count;
	constructor->last->index_count += index_count;
}

struct r_buffer **r_buffer_constructor_finish(struct r_buffer_constructor *constructor, const u32 c_h)
{
	struct r_buffer **array = NULL;
	if (constructor->count)
	{
		array = arena_push(g_scene->mem_frame, constructor->count * sizeof(struct r_buffer *));
		u32 i = 0;
		for (struct r_buffer *buf = constructor->first; buf; buf = buf->next)
		{
			array[i++] = buf;
		}
		kas_assert(i == constructor->count);
		array[i-1]->c_h = c_h;
	}

	return array;
}

static struct r_bucket bucket_stub = 
{  
	.c_l = U32_MAX,
	.c_h = U32_MAX,
	.buffer_count = 0,
	.buffer_array = NULL,
};

void r_scene_generate_bucket_list(void)
{
	KAS_TASK(__func__, T_RENDERER);

	struct r_bucket *start = &bucket_stub;
	struct r_bucket *b = start;
	b->next = NULL;
	u32 begin_new_bucket = 1;

	struct r_buffer_constructor buf_constructor = { 0 };

	for (u32 i = 0; i < g_scene->cmd_frame_count; ++i)
	{
		struct r_command *cmd = g_scene->cmd_frame + i;
		struct r_instance *instance = array_list_intrusive_address(g_scene->instance_list, cmd->instance);

		/* TODO: can just compare u64 masked keys here... */
		if (b->transparency != R_CMD_TRANSPARENCY_GET(cmd->key)
				|| b->material != R_CMD_MATERIAL_GET(cmd->key)
				|| b->screen_layer != R_CMD_SCREEN_LAYER_GET(cmd->key)
				|| b->primitive != R_CMD_PRIMITIVE_GET(cmd->key)
				|| b->instanced != R_CMD_INSTANCED_GET(cmd->key))
		{
			begin_new_bucket = 1;
		}

		if (begin_new_bucket)
		{
			b->buffer_count = buf_constructor.count;
			b->buffer_array = r_buffer_constructor_finish(&buf_constructor, i-1);
			r_buffer_constructor_reset(&buf_constructor);
			r_buffer_constructor_buffer_alloc(&buf_constructor, i);

			begin_new_bucket = 0;
			b->c_h = i-1;
			b->next = arena_push(g_scene->mem_frame, sizeof(struct r_bucket));
			kas_assert(b->next);
			b = b->next;
			b->next = NULL;
			b->c_l = i;
			b->screen_layer = R_CMD_SCREEN_LAYER_GET(cmd->key);
			b->transparency = R_CMD_TRANSPARENCY_GET(cmd->key);
			b->material = R_CMD_MATERIAL_GET(cmd->key);
			b->primitive = R_CMD_PRIMITIVE_GET(cmd->key);
			b->instanced = R_CMD_INSTANCED_GET(cmd->key);
		}

		switch (instance->type)
		{ 
			case R_INSTANCE_UI: 
			{
				//r_command_key_print(cmd->key);

				const u32 count = instance->ui_bucket->count;
				//TODO If instanced set index count immediately 
				buf_constructor.last->index_count = 6;
				r_buffer_constructor_buffer_add_size(&buf_constructor,
					       	4*instance->ui_bucket->count*L_UI_STRIDE,
					       	  instance->ui_bucket->count*S_UI_STRIDE, 
						  instance->ui_bucket->count,
						  0);	
			} break;

			case R_INSTANCE_UNIT:
			{
				const struct r_unit *u = r_unit_lookup((u32) instance->unit);
				switch (u->type)
				{
					//case R_UNIT_TYPE_PROXY2D:
					//{
					//	r_buffer_constructor_buffer_add_size(&buf_constructor, 4*sizeof(struct r_proxy2d_v), 6);
					//} break;

					case R_UNIT_TYPE_PROXY3D:
					{
						const struct r_mesh *mesh = r_mesh_address(u->mesh_handle);
						r_buffer_constructor_buffer_add_size(&buf_constructor, 
								mesh->vertex_count * R_PROXY3D_V_PACKED_SIZE,
								0,
								0,
							       	mesh->index_count);
					} break;

					//case R_UNIT_TYPE_STATIC:
					//{
					//	/* skip allocation on first buffer since it is already alloced */
					//	if (buf_constructor.last->c_l != i)
					//	{
					//		r_buffer_constructor_buffer_alloc(g_scene->mem_frame, &buf_constructor, i);
					//	}
					//	struct r_static *r_static = array_list_intrusive_address(g_r_core->static_list, u->type_index);
					//	r_buffer_constructor_buffer_add_size(&buf_constructor, r_static->vertex_size, r_static->index_count);
					//} break;
				}
			} break;

			default:
			{
				kas_assert_string(0, "unexpected r_instance type in generate_bucket\n");
			} break;
		}
	}
	b->buffer_count = buf_constructor.count;
	b->buffer_array = r_buffer_constructor_finish(&buf_constructor, g_scene->cmd_frame_count-1);
	b->c_h = g_scene->cmd_frame_count-1;

	KAS_END;

	g_scene->frame_bucket_list = start->next;
}

static void r_scene_bucket_generate_draw_data(struct r_bucket *b)
{
	KAS_TASK(__func__, T_RENDERER);

	const vec4 zero4 = { 0.0f, 0.0f, 0.0f, 0.0f };
	const vec3 zero3 = { 0.0f, 0.0f, 0.0f };

	const struct r_command *r_cmd = g_scene->cmd_frame + b->c_l;
	const struct r_instance *instance = array_list_intrusive_address(g_scene->instance_list, r_cmd->instance);

	for (u32 bi = 0; bi < b->buffer_count; bi++)
	{	
		struct r_buffer *buf = b->buffer_array[bi];
		buf->shared_data = arena_push(g_scene->mem_frame, buf->shared_size);
		buf->local_data = arena_push(g_scene->mem_frame, buf->local_size);
		buf->index_data = arena_push(g_scene->mem_frame, buf->index_count * sizeof(u32));

		u8 *shared_data = buf->shared_data;
		u8 *local_data = buf->local_data;

		switch (instance->type)
		{
			case R_INSTANCE_UI:
			{
				buf->index_data[0] = 0;
				buf->index_data[1] = 1;
				buf->index_data[2] = 2;
				buf->index_data[3] = 0;
				buf->index_data[4] = 2;
				buf->index_data[5] = 3;

				for (u32 i = buf->c_l; i <= buf->c_h; ++i)
				{
					r_cmd = g_scene->cmd_frame + i;
					instance = array_list_intrusive_address(g_scene->instance_list, r_cmd->instance);

					const struct ui_draw_bucket *ui_b = instance->ui_bucket;
					struct ui_draw_node *draw_node = ui_b->list;
					if (UI_CMD_LAYER_GET(ui_b->cmd) == UI_CMD_LAYER_TEXT)
					{
						for (u32 i = 0; i < ui_b->count; )
						{
							const struct ui_node *n = hierarchy_index_address(g_ui->node_hierarchy, draw_node->index);
							draw_node = draw_node->next;
							const vec4 visible_rect =
							{
								(n->pixel_visible[AXIS_2_X].high + n->pixel_visible[AXIS_2_X].low) / 2.0f,
								(n->pixel_visible[AXIS_2_Y].high + n->pixel_visible[AXIS_2_Y].low) / 2.0f,
								(n->pixel_visible[AXIS_2_X].high - n->pixel_visible[AXIS_2_X].low) / 2.0f,
								(n->pixel_visible[AXIS_2_Y].high - n->pixel_visible[AXIS_2_Y].low) / 2.0f,
							};

							vec2 global_offset;
							switch (n->text_align_x)
							{
								case ALIGN_X_CENTER: 
								{ 
									global_offset[0] = n->pixel_position[0] + (n->pixel_size[0] - n->layout_text->width) / 2.0f; 
								} break;

								case ALIGN_LEFT: 
								{ 
									global_offset[0] = n->pixel_position[0] + n->text_pad[0]; 
								} break;

								case ALIGN_RIGHT: 
								{ 
									global_offset[0] = n->pixel_position[0] + n->pixel_size[0] - n->text_pad[0] - n->layout_text->width; 
								} break;
							}	

							switch (n->text_align_y)
							{
								case ALIGN_Y_CENTER: 
								{ 
									global_offset[1] = n->pixel_position[1] + (n->pixel_size[1] + n->font->linespace*n->layout_text->line_count) / 2.0f; 
								} break;

								case ALIGN_TOP: 
								{ 
									global_offset[1] = n->pixel_position[1] + n->pixel_size[1] - n->text_pad[1]; 
								} break;

								case ALIGN_BOTTOM: 
								{ 
									global_offset[1] = n->pixel_position[1] + n->font->linespace*n->layout_text->line_count + n->text_pad[1]; 
								} break;
							}

							global_offset[0] = f32_round(global_offset[0]);
							global_offset[1] = f32_round(global_offset[1]);

							struct text_line *line = n->layout_text->line;
							for (u32 l = 0; l < n->layout_text->line_count; ++l, line = line->next)
							{
								vec2 global_baseline =
								{
									global_offset[0],
									global_offset[1] - n->font->ascent - l*n->font->linespace,
								};
									
								i += line->glyph_count;
								for (u32 t = 0; t < line->glyph_count; ++t)
								{
									const struct font_glyph *glyph = glyph_lookup(n->font, line->glyph[t].codepoint);
									const vec2 local_offset = 
									{ 
										global_baseline[0] + (f32) glyph->bearing[0] + line->glyph[t].x,
										global_baseline[1] + (f32) glyph->bearing[1],
									};

									const vec4 glyph_rect =
									{
										(2*local_offset[0] + (f32) glyph->size[0]) / 2.0f,
										(2*local_offset[1] - (f32) glyph->size[1]) / 2.0f,
										(f32) glyph->size[0] / 2.0f,
										(f32) glyph->size[1] / 2.0f,
									};	

									const vec4 uv_rect = 
									{
										(glyph->tr[0] + glyph->bl[0]) / 2.0f,
										(glyph->tr[1] + glyph->bl[1]) / 2.0f,
										(glyph->tr[0] - glyph->bl[0]) / 2.0f,
										(glyph->tr[1] - glyph->bl[1]) / 2.0f,
									};

									memcpy(shared_data + S_NODE_RECT_OFFSET, glyph_rect, sizeof(vec4));
									memcpy(shared_data + S_VISIBLE_RECT_OFFSET, visible_rect, sizeof(vec4));
									memcpy(shared_data + S_UV_RECT_OFFSET, uv_rect, sizeof(vec4));
									memcpy(shared_data + S_BACKGROUND_COLOR_OFFSET, zero4, sizeof(vec4));
									memcpy(shared_data + S_BORDER_COLOR_OFFSET, zero4, sizeof(vec4));
									memcpy(shared_data + S_SPRITE_COLOR_OFFSET, n->sprite_color, sizeof(vec4));
									memcpy(shared_data + S_EXTRA_OFFSET, zero3, sizeof(vec3));
									memset(shared_data + S_GRADIENT_COLOR_BR_OFFSET, 0, 4*sizeof(vec4));
									shared_data += S_UI_STRIDE;
								}
							}
						}
					}
					else if (UI_CMD_LAYER_GET(ui_b->cmd) == UI_CMD_LAYER_TEXT_SELECTION)
					{
						for (u32 i = 0; i < ui_b->count; ++i)
						{
							const struct ui_text_selection *sel = g_ui->frame_stack_text_selection.arr + draw_node->index;
							const struct ui_node *n = sel->node;
							draw_node = draw_node->next;

							vec2 global_offset;
							switch (n->text_align_x)
							{
								case ALIGN_X_CENTER: 
								{ 
									global_offset[0] = n->pixel_position[0] + (n->pixel_size[0] - n->layout_text->width) / 2.0f; 
								} break;

								case ALIGN_LEFT: 
								{ 
									global_offset[0] = n->pixel_position[0] + n->text_pad[0]; 
								} break;

								case ALIGN_RIGHT: 
								{ 
									global_offset[0] = n->pixel_position[0] + n->pixel_size[0] - n->text_pad[0] - n->layout_text->width; 
								} break;
							}	

							switch (n->text_align_y)
							{
								case ALIGN_Y_CENTER: 
								{ 
									global_offset[1] = n->pixel_position[1] + (n->pixel_size[1] + n->font->linespace*n->layout_text->line_count) / 2.0f; 
								} break;

								case ALIGN_TOP: 
								{ 
									global_offset[1] = n->pixel_position[1] + n->pixel_size[1] - n->text_pad[1]; 
								} break;

								case ALIGN_BOTTOM: 
								{ 
									global_offset[1] = n->pixel_position[1] + n->font->linespace*n->layout_text->line_count + n->text_pad[1]; 
								} break;
							}

							global_offset[0] = f32_round(global_offset[0]);
							global_offset[1] = f32_round(global_offset[1]);

							struct text_line *line = sel->layout->line;
							kas_assert(sel->layout->line_count == 1);
							kas_assert(sel->high <= line->glyph_count + 1);

							const struct font_glyph *glyph = glyph_lookup(n->font, (u32) ' ');
							f32 height = n->font->linespace;
							f32 width = glyph->advance;
							if (sel->low != sel->high)
							{
								width += line->glyph[sel->high-1].x - line->glyph[sel->low].x;	
							}

							if (0 < sel->low && sel->low <= line->glyph_count)
							{
								const struct font_glyph *end_glyph = glyph_lookup(n->font, line->glyph[sel->low-1].codepoint);
								global_offset[0] += line->glyph[sel->low-1].x + end_glyph->advance;
							}

							const vec4 highlight_rect =
							{
								(2*global_offset[0] + width) / 2.0f,
								(2*global_offset[1] - height) / 2.0f,
								width / 2.0f,
								height / 2.0f,
							};	

							const vec4 visible_rect =
							{
								(n->pixel_visible[AXIS_2_X].high + n->pixel_visible[AXIS_2_X].low) / 2.0f,
								(n->pixel_visible[AXIS_2_Y].high + n->pixel_visible[AXIS_2_Y].low) / 2.0f,
								(n->pixel_visible[AXIS_2_X].high - n->pixel_visible[AXIS_2_X].low) / 2.0f,
								(n->pixel_visible[AXIS_2_Y].high - n->pixel_visible[AXIS_2_Y].low) / 2.0f,
							};
	
							const struct sprite *spr = g_sprite + n->sprite;
							const vec4 uv_rect = 
							{
								(spr->tr[0] + spr->bl[0]) / 2.0f,
								(spr->tr[1] + spr->bl[1]) / 2.0f,
								(spr->tr[0] - spr->bl[0]) / 2.0f,
								(spr->tr[1] - spr->bl[1]) / 2.0f,
							};

							memcpy(shared_data + S_NODE_RECT_OFFSET, highlight_rect, sizeof(vec4));
							memcpy(shared_data + S_VISIBLE_RECT_OFFSET, visible_rect, sizeof(vec4));
							memcpy(shared_data + S_UV_RECT_OFFSET, uv_rect, sizeof(vec4));
							memcpy(shared_data + S_BACKGROUND_COLOR_OFFSET, sel->color, sizeof(vec4));
							memcpy(shared_data + S_BORDER_COLOR_OFFSET, zero4, sizeof(vec4));
							memcpy(shared_data + S_SPRITE_COLOR_OFFSET, zero4, sizeof(vec4));
							memcpy(shared_data + S_EXTRA_OFFSET, zero3, sizeof(vec3));
							memset(shared_data + S_GRADIENT_COLOR_BR_OFFSET, 0, 4*sizeof(vec4));
							shared_data += S_UI_STRIDE;
						}
					}
					else
					{
						for (u32 i = 0; i < ui_b->count; ++i)
						{
							const struct ui_node *n = hierarchy_index_address(g_ui->node_hierarchy, draw_node->index);
							draw_node = draw_node->next;
							const struct sprite *spr = g_sprite + n->sprite;
							const vec4 node_rect =
							{
								n->pixel_position[0] + n->pixel_size[0] / 2.0f,
								n->pixel_position[1] + n->pixel_size[1] / 2.0f,
								n->pixel_size[0] / 2.0f,
								n->pixel_size[1] / 2.0f,
							};

							const vec4 visible_rect =
							{
								(n->pixel_visible[AXIS_2_X].high + n->pixel_visible[AXIS_2_X].low) / 2.0f,
								(n->pixel_visible[AXIS_2_Y].high + n->pixel_visible[AXIS_2_Y].low) / 2.0f,
								(n->pixel_visible[AXIS_2_X].high - n->pixel_visible[AXIS_2_X].low) / 2.0f,
								(n->pixel_visible[AXIS_2_Y].high - n->pixel_visible[AXIS_2_Y].low) / 2.0f,
							};

							const vec4 uv_rect = 
							{
								(spr->tr[0] + spr->bl[0]) / 2.0f,
								(spr->tr[1] + spr->bl[1]) / 2.0f,
								(spr->tr[0] - spr->bl[0]) / 2.0f,
								(spr->tr[1] - spr->bl[1]) / 2.0f,
							};


							const vec3 extra = { n->border_size, n->corner_radius, n->edge_softness };
							memcpy(shared_data + S_NODE_RECT_OFFSET, node_rect, sizeof(vec4));
							memcpy(shared_data + S_VISIBLE_RECT_OFFSET, visible_rect, sizeof(vec4));
							memcpy(shared_data + S_UV_RECT_OFFSET, uv_rect, sizeof(vec4));
							memcpy(shared_data + S_BACKGROUND_COLOR_OFFSET, n->background_color, sizeof(vec4));
							memcpy(shared_data + S_BORDER_COLOR_OFFSET, n->border_color, sizeof(vec4));
							memcpy(shared_data + S_SPRITE_COLOR_OFFSET, n->sprite_color, sizeof(vec4));
							memcpy(shared_data + S_EXTRA_OFFSET, extra, sizeof(vec3));
							memcpy(shared_data + S_GRADIENT_COLOR_BR_OFFSET, n->gradient_color, 4*sizeof(vec4));
							shared_data += S_UI_STRIDE;
						}
					}
				}
			} break;

			case R_INSTANCE_UNIT:
			{
				kas_assert_string(0, "Reimplement");
				struct r_unit *u = r_unit_lookup((u32) instance->unit);
				//switch(u->type)
				//{
				//	case R_UNIT_TYPE_STATIC:
				//	{
				//		struct r_draw_call **draw_ptr = &b->draw_call;
				//		*draw_ptr = NULL;
				//		for (u32 bi = 0; bi < b->buffer_count; bi++)
				//		{	
				//			struct r_buffer *buf = b->buffer_array[bi];
				//			kas_assert(buf->c_l == buf->c_h);
				//			r_cmd = g_scene->cmd_frame + buf->c_l;
				//			u = r_unit_lookup(r_cmd->instance);
				//			const struct r_static *r_static = array_list_intrusive_address(g_r_core->static_list, u->type_index);
				//			local_data = r_static->vertex_data;
				//			buf->index_data = r_static->index_data;

				//			u64 vertex_offset = 0;
				//			u32 index_offset = 0;
				//			for (struct r_static_range *range = r_static->range; range != NULL; range = range->next)
				//			{
				//				if (range->index_count)
				//				{
				//					*draw_ptr = internal_r_draw_call_alloc(vertex_offset, index_offset, bi);
				//					if (*draw_ptr == NULL)
				//					{
				//						kas_assert(0);
				//						KAS_END;
				//						return;
				//					}
				//					(*draw_ptr)->vertex_data_size = range->vertex_size;
				//					(*draw_ptr)->index_count = range->index_count;
				//					draw_ptr = &(*draw_ptr)->next;

				//					vertex_offset += range->vertex_size;
				//					index_offset += range->index_count;
				//					b->draw_call_count += 1;
				//				}
				//			}
				//		}
				//	} break;

				//	case R_UNIT_TYPE_PROXY3D:
				//	{
				//		for (u32 bi = 0; bi < b->buffer_count; bi++)
				//		{	
				//			struct r_buffer *buf = b->buffer_array[bi];
				//			buf->index_data = arena_push(g_scene->mem_frame, buf->index_count * sizeof(i16));
				//			local_data = arena_push(g_scene->mem_frame, local_size);
				//			b->draw_call = internal_r_draw_call_alloc(0, 0, bi);

				//			if (b->draw_call == NULL || local_data == NULL || buf->index_data == NULL || buf->index_count == 0)
				//			{
				//				b->draw_call_count = 0;
				//				KAS_END;
				//				return;
				//			}

				//			u64 v_offset = 0;
				//			u64 i_offset = 0;
				//			i32 index_max = 0;
				//			struct r_draw_call *draw = b->draw_call;
				//			b->draw_call_count = 1;
				//			for (u32 i = buf->c_l; i <= buf->c_h; ++i)
				//			{
				//				r_cmd = g_scene->cmd_frame + i;
				//				u = r_unit_lookup(r_cmd->instance);
				//				kas_assert(u->type == R_UNIT_TYPE_PROXY3D);

				//				const struct r_mesh *mesh = r_mesh_address(u->mesh_handle);
				//				if (I16_MAX < index_max + mesh->index_max_used + 1)
				//				{
				//					draw->next = internal_r_draw_call_alloc(draw->vertex_data_offset + draw->vertex_data_size, draw->index_first + draw->index_count, bi);
				//					if (draw->next == NULL)
				//					{
				//						KAS_END;
				//						return;
				//					}
				//					draw = draw->next;
				//					b->draw_call_count += 1;
				//					index_max = 0;
				//				}

				//				kas_assert(mesh->attribute_flags == (R_MESH_ATTRIBUTE_POSITION | R_MESH_ATTRIBUTE_NORMAL));

				//				const u64 mesh_vertex_size = sizeof(vec3) + sizeof(vec3);
				//				for (i16 k = 0; k < mesh->vertex_count; ++k)
				//				{
				//					kas_assert(v_offset + R_PROXY3D_V_PACKED_SIZE <= local_size);
				//					vec3_copy(((struct r_proxy3d_v *) ((u8 *) local_data + v_offset))->position, (void *) (((u8 *) mesh->vertex_data) + k*mesh_vertex_size + 0));
				//					vec4_copy(((struct r_proxy3d_v *) ((u8 *) local_data + v_offset))->color, u->color);
				//					vec3_copy(((struct r_proxy3d_v *) ((u8 *) local_data + v_offset))->normal, (void *) (((u8 *) mesh->vertex_data) + k*mesh_vertex_size + sizeof(vec3)));
				//					vec3_copy(((struct r_proxy3d_v *) ((u8 *) local_data + v_offset))->translation, g_r_core->frame_proxy3d_position[u->type_index]);
				//					quat_copy(((struct r_proxy3d_v *) ((u8 *) local_data + v_offset))->rotation, g_r_core->frame_proxy3d_rotation[u->type_index]);
				//					v_offset += R_PROXY3D_V_PACKED_SIZE;
				//				}

				//				u32 next = (index_max == 0)
				//					? 0
				//					: index_max+1;

				//				for (u32 k = 0; k < mesh->index_count; ++k)
				//				{
				//					buf->index_data[i_offset + k] = next + mesh->index_data[k]; 
				//				}

				//				draw->vertex_data_size += mesh->vertex_count*R_PROXY3D_V_PACKED_SIZE;
				//				draw->index_count += mesh->index_count;

				//				i_offset += mesh->index_count;
				//				index_max = (index_max == 0)
				//					? mesh->index_max_used
				//					: index_max + mesh->index_max_used + 1;
				//			}
				//			kas_assert(v_offset == local_size); 
				//		}
				//	} break;

				//	default:
				//	{
				//		kas_assert_string(0, "Unimplemented unit type in draw call generation");
				//	} break;
				//}
			} break;

			default:
			{
				kas_assert_string(0, "Unimplemented instance type in draw call generation");
			} break;
		}
	}

	KAS_END;
}

void r_scene_frame_end(void)
{
	KAS_TASK(__func__, T_RENDERER);

	r_scene_sort_commands_and_prune_instances();
	r_scene_generate_bucket_list();
	for (struct r_bucket *b = g_scene->frame_bucket_list; b; b = b->next)
	{
		r_scene_bucket_generate_draw_data(b);
	}
	KAS_END;
}

struct r_instance *r_instance_add(const u32 unit, const u32 generation, const u64 cmd)
{
	struct r_instance *instance = NULL;
	const u64 key = ((u64) generation << 32) | (u64) unit;

	u32 index = hash_map_first(g_scene->unit_to_instance_map, key & U32_MAX);
	for (; index != HASH_NULL; index = hash_map_next(g_scene->unit_to_instance_map, index))
	{
		instance = array_list_intrusive_address(g_scene->instance_list, index);
		if (instance->unit == key)
		{
			break;
		}
	}

	if (instance == NULL)
	{
		index = array_list_intrusive_reserve_index(g_scene->instance_list);
		instance = array_list_intrusive_address(g_scene->instance_list, index);
		instance->header.next = g_scene->instance_new_first;	
		hash_map_add(g_scene->unit_to_instance_map, key, index);

		g_scene->instance_new_first = index;
		g_scene->cmd_new_count += 1;

		instance->cmd = arena_push(g_scene->mem_frame, sizeof(struct r_command));
		instance->cmd->key = cmd;
		instance->cmd->instance = index;
		instance->cmd->allocated = 1;
	}
	else if (instance->cmd->key != cmd)
	{
		instance->header.next = g_scene->instance_new_first;

		g_scene->instance_new_first = index;
		g_scene->cmd_new_count += 1;

		instance->cmd->allocated = 0;
		instance->cmd = arena_push(g_scene->mem_frame, sizeof(struct r_command));
		instance->cmd->key = cmd;
		instance->cmd->instance = index;
		instance->cmd->allocated = 1;
	}

	instance->frame_last_touched = g_scene->frame;
	instance->type = R_INSTANCE_UNIT;
	g_scene->cmd_frame_count += 1;
	
	return instance;
}

struct r_instance *r_instance_add_non_cached(const u64 cmd)
{
	const u32 index = array_list_intrusive_reserve_index(g_scene->instance_list);
	struct r_instance *instance = array_list_intrusive_address(g_scene->instance_list, index);
	instance->header.next = g_scene->instance_new_first;	

	g_scene->instance_new_first = index;
	instance->cmd = arena_push(g_scene->mem_frame, sizeof(struct r_command));
	instance->cmd->key = cmd;
	instance->cmd->instance = index;
	instance->cmd->allocated = 1;
	instance->frame_last_touched = g_scene->frame;
	g_scene->cmd_new_count += 1;
	g_scene->cmd_frame_count += 1;
	
	return instance;
}

u64 r_command_key(const u64 screen, const u64 depth, const u64 transparency, const u64 material, const u64 primitive, const u64 instanced)
{
	kas_assert(screen <= (((u64) 1 << R_CMD_SCREEN_LAYER_BITS) - (u64) 1));
	kas_assert(depth <= (((u64) 1 << R_CMD_DEPTH_BITS) - (u64) 1));
	kas_assert(transparency <= (((u64) 1 << R_CMD_TRANSPARENCY_BITS) - (u64) 1));
	kas_assert(material <= (((u64) 1 << R_CMD_MATERIAL_BITS) - (u64) 1));
	kas_assert(primitive <= (((u64) 1 << R_CMD_PRIMITIVE_BITS) - (u64) 1));
	kas_assert(instanced <= (((u64) 1 << R_CMD_INSTANCED_BITS) - (u64) 1));

	return ((u64) screen << R_CMD_SCREEN_LAYER_LOW_BIT)		
	      	| ((u64) depth << R_CMD_DEPTH_LOW_BIT)			
	       	| ((u64) transparency << R_CMD_TRANSPARENCY_LOW_BIT)	
	       	| ((u64) material << R_CMD_MATERIAL_LOW_BIT)		
		| ((u64) primitive << R_CMD_PRIMITIVE_LOW_BIT)
		| ((u64) instanced << R_CMD_INSTANCED_LOW_BIT);
}

const char *screen_str_table[1 << R_CMD_SCREEN_LAYER_BITS] =
{
	"SCREEN_LAYER_HUD",
	"SCREEN_LAYER_GAME",
};

const char *transparency_str_table[1 << R_CMD_TRANSPARENCY_BITS] =
{
	"TRANSPARENCY_NORMAL",
	"TRANSPARENCY_SUBTRACTIVE",
	"TRANSPARENCY_ADDITIVE",
	"TRANSPARENCY_OPAQUE",
};

const char *primitive_str_table[1 << R_CMD_PRIMITIVE_BITS] =
{
	"PRIMITIVE_TRIANGLE",
	"PRIMITIVE_LINE",
};

const char *instanced_str_table[1 << R_CMD_INSTANCED_BITS] =
{
	"INSTANCED",
	"NON_INSTANCED",
};

void r_command_key_print(const u64 key)
{
	const char *screen_str = screen_str_table[R_CMD_SCREEN_LAYER_GET(key)];
	const char *transparency_str = transparency_str_table[R_CMD_TRANSPARENCY_GET(key)];
	const char *primitive_str = primitive_str_table[R_CMD_PRIMITIVE_GET(key)];
	const char *instanced_str = instanced_str_table[R_CMD_INSTANCED_GET(key)];

	fprintf(stderr, "render command key:\n");
	fprintf(stderr, "\tscreen: %s\n", screen_str);
	fprintf(stderr, "\tdepth: %lu\n", R_CMD_DEPTH_GET(key));
	fprintf(stderr, "\ttransparency: %s\n", transparency_str);
	fprintf(stderr, "\tmaterial: %lu\n", R_CMD_MATERIAL_GET(key));
	fprintf(stderr, "\tprimitive: %s\n", primitive_str);
	fprintf(stderr, "\tinstanced: %s\n", instanced_str);
}
