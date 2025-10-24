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

#include "ui_local.h"
#include "hash_map.h"
#include "kas_profiler.h"

#define INITIAL_UNIT_COUNT	1024
#define INITIAL_HASH_COUNT	1024

//#define UI_DEBUG_FLAGS	UI_DRAW_BORDER
#define UI_DEBUG_FLAGS		UI_FLAG_NONE 

DEFINE_STACK(ui_size);
DEFINE_STACK(ui_text_selection);
DEFINE_STACK(utf32);

static void ui_command_static_assert(void)
{
	kas_static_assert(UI_CMD_LAYER_BITS
			+ UI_CMD_DEPTH_BITS
			+ UI_CMD_TEXTURE_BITS
			== 32, "ui_cmd definitions should span whole 32 bits");

	//TODO Show no overlap between masks
	kas_static_assert((UI_CMD_LAYER_MASK & UI_CMD_DEPTH_MASK) == 0, "UI_CMD_*_MASK values should not overlap");
	kas_static_assert((UI_CMD_LAYER_MASK & UI_CMD_TEXTURE_MASK) == 0, "UI_CMD_*_MASK values should not overlap");
	kas_static_assert((UI_CMD_DEPTH_MASK & UI_CMD_TEXTURE_MASK) == 0, "UI_CMD_*_MASK values should not overlap");

	kas_static_assert(UI_CMD_LAYER_MASK
			+ UI_CMD_DEPTH_MASK
			+ UI_CMD_TEXTURE_MASK
			== U32_MAX, "sum of ui_cmd masks should be U32");

	kas_static_assert(TEXTURE_COUNT <= (UI_CMD_TEXTURE_MASK >> UI_CMD_TEXTURE_LOW_BIT), "texture mask must be able to contain all texture ids");
}

/* Set to current ui being operated on */
struct ui *g_ui = NULL;
/* ui allocator  */

u32	cmd_ui_text_op;
u32	cmd_ui_popup_build;

void ui_init_global_state(void)
{
	cmd_function_register(utf8_inline("timeline_drag"), 4, &timeline_drag);
	cmd_function_register(utf8_inline("ui_text_input_mode_enable"), 2, &ui_text_input_mode_enable);
	cmd_function_register(utf8_inline("ui_text_edit_clear"), 1, &ui_text_edit_clear);
	cmd_function_register(utf8_inline("ui_text_input_mode_disable"), 0, &ui_text_input_mode_disable);
	cmd_ui_text_op = cmd_function_register(utf8_inline("ui_text_op"), 3, &ui_text_op).index;
	cmd_ui_popup_build = cmd_function_register(utf8_inline("ui_popup_build"), 2, &ui_popup_build).index;
}

void ui_free_global_state(void)
{
}

struct ui_visual ui_visual_init(const vec4 background_color
		, const vec4 border_color
		, const vec4 gradient_color[BOX_CORNER_COUNT]
		, const vec4 sprite_color
		, const f32 pad
		, const f32 edge_softness
		, const f32 corner_radius
		, const f32 border_size
		, const enum font_id font
		, const enum alignment_x text_alignment_x
		, const enum alignment_y text_alignment_y
		, const f32 text_pad_x
		, const f32 text_pad_y)
{
	struct ui_visual visual = { 0 };

	vec4_copy(visual.background_color, background_color);
	vec4_copy(visual.border_color, border_color);
	vec4_copy(visual.gradient_color[0], gradient_color[0]);
	vec4_copy(visual.gradient_color[1], gradient_color[1]);
	vec4_copy(visual.gradient_color[2], gradient_color[2]);
	vec4_copy(visual.gradient_color[3], gradient_color[3]);
	vec4_copy(visual.sprite_color, sprite_color);
	visual.pad = pad;
	visual.edge_softness = edge_softness;
	visual.corner_radius = corner_radius;
	visual.border_size = border_size;
	visual.font = font;
	visual.text_alignment_x = text_alignment_x;
	visual.text_alignment_y = text_alignment_y;
	visual.text_pad_x = text_pad_x;
	visual.text_pad_y = text_pad_y;

	return visual;
}

struct ui_text_selection ui_text_selection_empty(void)
{
	struct ui_text_selection sel = { 0 };
	return sel;
}

static utf32 text_stub = { 0 };

struct text_edit_state text_edit_state_null(void)
{
	struct text_edit_state edit = 
	{  
		edit.id = utf8_empty(),
		edit.text = &text_stub,	
		edit.cursor = 0,
		edit.mark = 0,
	};
	return edit;
}

const u32 key_zero_stub[KAS_KEY_COUNT] =  { 0 };

struct ui *ui_alloc(void)
{
	kas_static_assert(sizeof(struct ui_size) == 16, "Expected size");

	struct ui *ui = malloc(sizeof(struct ui));
	ui->node_hierarchy = hierarchy_index_alloc(NULL, INITIAL_UNIT_COUNT, sizeof(struct ui_node), GROWABLE);
	ui->node_map = hash_map_alloc(NULL, U16_MAX, U16_MAX, GROWABLE);
	ui->bucket_allocator = array_list_intrusive_alloc(NULL, 64, sizeof(struct ui_draw_bucket), GROWABLE);
	ui->bucket_map = hash_map_alloc(NULL, 128, 128, GROWABLE);
	ui->frame = 0;
	ui->root = HI_ROOT_STUB_INDEX;
	ui->node_count_prev_frame = 0;
	ui->node_count_frame = 0;
	ui->mem_frame_arr[0] = arena_alloc(64*1024*1024);
	ui->mem_frame_arr[1] = arena_alloc(64*1024*1024);
	ui->mem_frame = ui->mem_frame_arr + (ui->frame & 0x1);
	ui->inter.text_edit = text_edit_state_null();
	ui->stack_parent = stack_u32_alloc(NULL, 32, GROWABLE);
	ui->stack_sprite = stack_u32_alloc(NULL, 32, GROWABLE);
	ui->stack_font = stack_ptr_alloc(NULL, 8, GROWABLE);
	ui->stack_flags = stack_u64_alloc(NULL, 16, GROWABLE);
	ui->stack_recursive_interaction_flags = stack_u64_alloc(NULL, 16, GROWABLE);
	ui->stack_recursive_interaction = stack_ptr_alloc(NULL, 16, GROWABLE);
	ui->stack_external_text = stack_utf32_alloc(NULL, 8, GROWABLE);
	ui->stack_external_text_layout = stack_ptr_alloc(NULL, 8, GROWABLE);
	ui->stack_floating_node = stack_u32_alloc(NULL, 32, GROWABLE);
	ui->stack_floating_depth = stack_u32_alloc(NULL, 32, GROWABLE);
	ui->stack_floating[AXIS_2_X] = stack_f32_alloc(NULL, 16, GROWABLE);
	ui->stack_floating[AXIS_2_Y] = stack_f32_alloc(NULL, 16, GROWABLE);
	ui->stack_ui_size[AXIS_2_X] = stack_ui_size_alloc(NULL, 16, GROWABLE);
	ui->stack_ui_size[AXIS_2_Y] = stack_ui_size_alloc(NULL, 16, GROWABLE);
	ui->stack_gradient_color[BOX_CORNER_BR] = stack_vec4_alloc(NULL, 16, GROWABLE);
	ui->stack_gradient_color[BOX_CORNER_TR] = stack_vec4_alloc(NULL, 16, GROWABLE);
	ui->stack_gradient_color[BOX_CORNER_TL] = stack_vec4_alloc(NULL, 16, GROWABLE);
	ui->stack_gradient_color[BOX_CORNER_BL] = stack_vec4_alloc(NULL, 16, GROWABLE);
	ui->stack_viewable[AXIS_2_X] = stack_intv_alloc(NULL, 8, GROWABLE);
	ui->stack_viewable[AXIS_2_Y] = stack_intv_alloc(NULL, 8, GROWABLE);
	ui->stack_child_layout_axis = stack_u32_alloc(NULL, 16, GROWABLE);
	ui->stack_background_color = stack_vec4_alloc(NULL, 16, GROWABLE);
	ui->stack_border_color = stack_vec4_alloc(NULL, 16, GROWABLE);
	ui->stack_sprite_color = stack_vec4_alloc(NULL, 16, GROWABLE);
	ui->stack_edge_softness = stack_f32_alloc(NULL, 16, GROWABLE);
	ui->stack_corner_radius = stack_f32_alloc(NULL, 16, GROWABLE);
	ui->stack_border_size = stack_f32_alloc(NULL, 16, GROWABLE);
	ui->stack_text_alignment_x = stack_u32_alloc(NULL, 8, GROWABLE);
	ui->stack_text_alignment_y = stack_u32_alloc(NULL, 8, GROWABLE);
	ui->stack_text_pad[AXIS_2_X] = stack_f32_alloc(NULL, 8, GROWABLE);
	ui->stack_text_pad[AXIS_2_Y] = stack_f32_alloc(NULL, 8, GROWABLE);
	ui->stack_fixed_depth = stack_u32_alloc(NULL, 16, GROWABLE);
	ui->stack_pad = stack_f32_alloc(NULL, 8, GROWABLE);
	ui->frame_stack_text_selection = stack_ui_text_selection_alloc(NULL, 128, GROWABLE);

	ui->inter.node_hovered = utf8_empty();
	ui->inter.inter_stub = malloc(sizeof(struct ui_inter_node));
	memset(ui->inter.inter_stub, 0, sizeof(struct ui_inter_node));
	ui->inter.inter_stub->key_clicked = key_zero_stub;
	ui->inter.inter_stub->key_pressed = key_zero_stub;
	ui->inter.inter_stub->key_released = key_zero_stub;

	/* setup root stub values */
	stack_u32_push(&ui->stack_parent, HI_ROOT_STUB_INDEX);
	struct ui_node *stub = hierarchy_index_address(ui->node_hierarchy, HI_ROOT_STUB_INDEX);
	stub->id = utf8_empty();
	stub->semantic_size[AXIS_2_X] = ui_size_pixel(0.0f, 0.0f);
	stub->semantic_size[AXIS_2_Y] = ui_size_pixel(0.0f, 0.0f);
	stub->child_layout_axis = AXIS_2_X;
	stub->depth = 0;
	stub->flags = UI_FLAG_NONE;
	stub->inter = ui->inter.inter_stub;

	struct ui_node *orphan_root = hierarchy_index_address(ui->node_hierarchy, HI_ORPHAN_STUB_INDEX);
	orphan_root->id = utf8_empty();
	orphan_root->semantic_size[AXIS_2_X] = ui_size_pixel(0.0f, 0.0f);
	orphan_root->semantic_size[AXIS_2_Y] = ui_size_pixel(0.0f, 0.0f);
	orphan_root->child_layout_axis = AXIS_2_X;
	orphan_root->depth = 0;
	orphan_root->flags = UI_FLAG_NONE;
	orphan_root->inter = ui->inter.inter_stub;

	ui->stack_flags.next = 1;
	ui->stack_flags.arr[0] = UI_FLAG_NONE;
	ui->stack_recursive_interaction_flags.next = 1;
	ui->stack_recursive_interaction_flags.arr[0] = UI_FLAG_NONE;

	ui->bucket_first = array_list_intrusive_reserve(ui->bucket_allocator);
	ui->bucket_last = ui->bucket_first;
	ui->bucket_cache = ui->bucket_first;
	ui->bucket_count = 0;
	ui->bucket_first->cmd = 0;
	ui->bucket_first->count = 0;

	return ui;
}

void ui_dealloc(struct ui *ui)
{
	arena_free(ui->mem_frame_arr + 0);
	arena_free(ui->mem_frame_arr + 1);
	free(ui->inter.inter_stub);

	stack_ui_text_selection_free(&ui->frame_stack_text_selection);
	stack_f32_free(&ui->stack_pad);
	stack_u64_free(&ui->stack_flags);
	stack_u64_free(&ui->stack_recursive_interaction_flags);
	stack_ptr_free(&ui->stack_recursive_interaction);
	stack_utf32_free(&ui->stack_external_text);
	stack_ptr_free(&ui->stack_external_text_layout);
	stack_u32_free(&ui->stack_text_alignment_x);
	stack_u32_free(&ui->stack_text_alignment_y);
	stack_f32_free(ui->stack_text_pad + AXIS_2_X);
	stack_f32_free(ui->stack_text_pad + AXIS_2_Y);
	stack_f32_free(&ui->stack_edge_softness);
	stack_f32_free(&ui->stack_corner_radius);
	stack_f32_free(&ui->stack_border_size);
	stack_u32_free(&ui->stack_parent);
	stack_u32_free(&ui->stack_sprite);
	stack_ptr_free(&ui->stack_font);
	stack_f32_free(ui->stack_floating + AXIS_2_X);
	stack_f32_free(ui->stack_floating + AXIS_2_Y);
	stack_ui_size_free(ui->stack_ui_size + AXIS_2_X);
	stack_ui_size_free(ui->stack_ui_size + AXIS_2_Y);
	stack_vec4_free(ui->stack_gradient_color + BOX_CORNER_BR);
	stack_vec4_free(ui->stack_gradient_color + BOX_CORNER_TR);
	stack_vec4_free(ui->stack_gradient_color + BOX_CORNER_TL);
	stack_vec4_free(ui->stack_gradient_color + BOX_CORNER_BL);
	stack_intv_free(ui->stack_viewable + AXIS_2_X);
	stack_intv_free(ui->stack_viewable + AXIS_2_Y);
	stack_u32_free(&ui->stack_child_layout_axis);
	stack_vec4_free(&ui->stack_background_color);
	stack_vec4_free(&ui->stack_border_color);
	stack_vec4_free(&ui->stack_sprite_color);
	stack_u32_free(&ui->stack_floating_node);
	stack_u32_free(&ui->stack_floating_depth);
	stack_u32_free(&ui->stack_fixed_depth);
	hash_map_free(ui->node_map);
	array_list_intrusive_free(ui->bucket_allocator);
	hash_map_free(ui->bucket_map);
	hierarchy_index_free(ui->node_hierarchy);
	free(ui);
	if (g_ui == ui)
	{
		g_ui = NULL;
	}
}

static void ui_draw_bucket_add_node(const u32 cmd, const u32 index)
{
	struct ui_draw_bucket *bucket;
	if (g_ui->bucket_cache->cmd == cmd)
	{
		bucket = g_ui->bucket_cache;
	}
	else
	{
		u32 bi = hash_map_first(g_ui->bucket_map, cmd);
		for (; bi != HASH_NULL; bi = hash_map_next(g_ui->bucket_map, bi))
		{
			bucket = array_list_intrusive_address(g_ui->bucket_allocator, bi);
			if (bucket->cmd == cmd)
			{
				break;
			}
		}

		if (bi == HASH_NULL)
		{
			bi = array_list_intrusive_reserve_index(g_ui->bucket_allocator);
			hash_map_add(g_ui->bucket_map, cmd, bi);
			bucket = array_list_intrusive_address(g_ui->bucket_allocator, bi);
			bucket->cmd = cmd;
			bucket->count = 0;
			bucket->list = NULL;
			g_ui->bucket_last->next = bucket;
			g_ui->bucket_last = bucket;
			g_ui->bucket_count += 1;
		}
	}

	struct ui_draw_node *tmp = bucket->list;
	bucket->list = arena_push(g_ui->mem_frame, sizeof(struct ui_draw_node));
	bucket->list->next = tmp;
	bucket->list->index = index;
	bucket->count += 1;
}

void ui_set(struct ui *ui)
{
	g_ui = ui;
}

static struct slot ui_root_f(const char *format, ...)
{
	va_list args;
	va_start(args, format);
	utf8 id = utf8_format_variadic(g_ui->mem_frame, format, args);
	va_end(args);

	return ui_node_alloc(UI_FLAG_NONE, &id);
}

static void ui_node_remove_hash(const struct hierarchy_index *node_hierarchy, const u32 index, void *data)
{
	const struct ui_node *node = hierarchy_index_address(node_hierarchy, index);
	if ((node->flags & UI_NON_HASHED) == 0)
	{
		//fprintf(stderr, "pruning hashed orphan %s\n",(char*) ((struct ui_node *) node)->id.buf);
		hash_map_remove(g_ui->node_map, node->key, index);
	}
}

static void ui_childsum_layout_size_and_prune_nodes(void)
{
	KAS_TASK(__func__, T_UI);
	arena_push_record(g_ui->mem_frame);

	//stack_u32 stack_free = stack_u32_alloc(g_ui->mem_frame, g_ui->node_count_prev_frame, 0);
	stack_ptr stack_childsum_x = stack_ptr_alloc(g_ui->mem_frame, g_ui->node_count_frame, 0);
	stack_ptr stack_childsum_y = stack_ptr_alloc(g_ui->mem_frame, g_ui->node_count_frame, 0);

	struct hierarchy_index_iterator	it = hierarchy_index_iterator_init(g_ui->mem_frame, g_ui->node_hierarchy, g_ui->root);
	while(it.count)
	{
		//const u32 potential_next = hierarchy_index_iterator_peek(&it);
		//struct ui_node *node = hierarchy_index_address(g_ui->node_hierarchy, potential_next);
		//if (node->last_frame_touched != g_ui->frame)
		//{
		//	//fprintf(stderr, "FRAME UNTOUCHED:%lu\tID:%s\tKEY:%u\tINDEX:%u\n", g_ui->frame, node->id.buf, node->key, potential_next);
		//	stack_u32_push(&stack_free, potential_next);
		//	hierarchy_index_iterator_skip(&it);
		//	continue;
		//}
		//hierarchy_index_iterator_next_df(&it);
		struct ui_node *node = hierarchy_index_address(g_ui->node_hierarchy, hierarchy_index_iterator_next_df(&it));

		if (node->semantic_size[AXIS_2_X].type == UI_SIZE_CHILDSUM)
		{
			stack_ptr_push(&stack_childsum_x, node);
		}

		if (node->semantic_size[AXIS_2_Y].type == UI_SIZE_CHILDSUM)
		{
			stack_ptr_push(&stack_childsum_y, node);
		}
	}

	hierarchy_index_iterator_release(&it);

	while (stack_childsum_y.next)
	{
		struct ui_node *node = stack_ptr_pop(&stack_childsum_y);
		node->layout_size[AXIS_2_Y] = 0.0f;
		struct ui_node *child = NULL;
		for (u32 i = node->header.first; i != HI_NULL_INDEX; i = child->header.next)
		{
			child = hierarchy_index_address(g_ui->node_hierarchy, i);
			node->layout_size[AXIS_2_Y] += child->layout_size[AXIS_2_Y];
		}
	}
	
	while (stack_childsum_x.next)
	{
		struct ui_node *node = stack_ptr_pop(&stack_childsum_x);
		node->layout_size[AXIS_2_X] = 0.0f;
		struct ui_node *child = NULL;
		for (u32 i = node->header.first; i != HI_NULL_INDEX; i = child->header.next)
		{
			child = hierarchy_index_address(g_ui->node_hierarchy, i);
			node->layout_size[AXIS_2_X] += child->layout_size[AXIS_2_X];
		}
	}

	arena_pop_record(g_ui->mem_frame);
	KAS_END;
}

static void ui_node_solve_child_violation(struct ui_node *node, const enum axis_2 axis)
{
	if (!node->header.child_count)
	{
		return; 
	}

	arena_push_record(g_ui->mem_frame);
	struct ui_node **child = arena_push(g_ui->mem_frame, node->header.child_count * sizeof(struct ui_node *));
	f32 *new_size = arena_push(g_ui->mem_frame, node->header.child_count  * sizeof(f32));
	u32 *shrink = arena_push(g_ui->mem_frame, node->header.child_count * sizeof(u32));
	f32 child_size_sum = 0.0f;
	u32 children_to_shrink = node->header.child_count;
	u32 index = node->header.first;

	u32 pad_fill_count = 0;
	u32 *pad_fill_index = arena_push(g_ui->mem_frame, node->header.child_count * sizeof(u32));

	for (u32 i = 0; i < node->header.child_count; ++i)
	{
		child[i] = hierarchy_index_address(g_ui->node_hierarchy, index);

		new_size[i] = child[i]->layout_size[axis];
		//child_size_sum += child[i]->layout_size[axis];
		child_size_sum += (child[i]->flags & (UI_FIXED_X << axis))
			? 0.0f
			: child[i]->layout_size[axis];

		const u32 child_is_pad_fill = !!(child[i]->flags & UI_PAD_FILL);
		if (child_is_pad_fill)
		{
			pad_fill_index[pad_fill_count++] = i;
		}

		if ((child[i]->flags & ((UI_FIXED_X | UI_ALLOW_VIOLATION_X | UI_PERC_POSTPONED_X) << axis)) == 0)
		{
			shrink[i] =  1;
		}
		else
		{
			children_to_shrink -= 1;
			shrink[i] =  0;
		}
		index = child[i]->header.next;
	}

	if (node->child_layout_axis != axis && (node->flags & (UI_ALLOW_VIOLATION_X << axis)) == 0)
	{
		for (u32 i = 0; i < node->header.child_count; ++i)
		{
			const f32 perc = f32_max(child[i]->semantic_size[axis].strictness, f32_min(1.0f, child[i]->layout_size[axis] / node->layout_size[axis]));
			new_size[i] = (shrink[i])
				? child[i]->layout_size[axis] * perc
				: child[i]->layout_size[axis];
		}
	}
	else if (node->child_layout_axis == axis)
	{
		const f32 size_left = node->layout_size[axis] - child_size_sum;
		if (size_left < 0.0f)
		{
 			if ((node->flags & (UI_ALLOW_VIOLATION_X << axis)) == 0)
			{
				f32 child_perc_remain_after_shrink = node->layout_size[axis] / child_size_sum;

				while (1)
				{	
					/* sum of original sizes of children we may shrink again */
					f32 original_shrinkable_size = 0.0f;
					/* sum of new sizes of children we may NOT shrink again */
					f32 new_unshrinkable_size = 0.0f;
					u32 can_shrink_again_count = 0;
					for (u32 i = 0; i < node->header.child_count; ++i)
					{
						if (shrink[i])
						{
							if (child[i]->semantic_size[axis].strictness < child_perc_remain_after_shrink)
							{
								new_size[i] = child[i]->layout_size[axis] * child_perc_remain_after_shrink;
								original_shrinkable_size += child[i]->layout_size[axis];
								can_shrink_again_count += 1;
							}
							else
							{
								new_size[i] = child[i]->layout_size[axis] * child[i]->semantic_size[axis].strictness;
								new_unshrinkable_size += new_size[i];
							}
						}
						else
						{
								new_unshrinkable_size += new_size[i];
						}
					}


					if (can_shrink_again_count == children_to_shrink)
					{
						break;
					}
					else if (!can_shrink_again_count || original_shrinkable_size < (node->layout_size[axis] - new_unshrinkable_size))
					{
						//force_shrink_all = 1;
						break;
					}

					children_to_shrink = can_shrink_again_count;
					child_perc_remain_after_shrink = (node->layout_size[axis] - new_unshrinkable_size) / original_shrinkable_size;
				}
			}
		}
		else
		{
			for (u32 i = 0; i < pad_fill_count; ++i)
			{
				new_size[pad_fill_index[i]] = size_left / pad_fill_count;
			}
		}
	}

	if (axis == AXIS_2_X)
	{
		//TODO clamp positions to pixels, (or something)
		for (u32 i = 0; i < node->header.child_count; ++i)
		{
			if ((child[i]->flags & (UI_TEXT_ALLOW_OVERFLOW | UI_TEXT_ATTACHED)) == UI_TEXT_ATTACHED 
					&& (child[i]->layout_size[axis] != new_size[i]))
			{
				child[i]->flags |= UI_TEXT_LAYOUT_POSTPONED;
			}
			child[i]->layout_size[axis] = new_size[i];
		}
	}
	else
	{
		//TODO clamp positions to pixels, (or something)
		for (u32 i = 0; i < node->header.child_count; ++i)
		{
			child[i]->layout_size[axis] = new_size[i];
		}
	}

	arena_pop_record(g_ui->mem_frame);
}

static void ui_solve_violations(void)
{
	KAS_TASK(__func__, T_UI);
	struct arena tmp = arena_alloc_1MB();
	struct hierarchy_index_iterator	it = hierarchy_index_iterator_init(&tmp, g_ui->node_hierarchy, g_ui->root);
	while(it.count)
	{
		const u32 index = hierarchy_index_iterator_next_df(&it);
		struct ui_node *node = hierarchy_index_address(g_ui->node_hierarchy, index);

		ui_node_solve_child_violation(node, AXIS_2_X);
		ui_node_solve_child_violation(node, AXIS_2_Y);
	}
	hierarchy_index_iterator_release(&it);
	arena_free_1MB(&tmp);
	KAS_END;
}

static void ui_layout_absolute_position(void)
{
	KAS_TASK(__func__, T_UI);

	struct arena tmp = arena_alloc_1MB();
	struct hierarchy_index_iterator	it = hierarchy_index_iterator_init(&tmp, g_ui->node_hierarchy, g_ui->root);

	struct ui_node *node = hierarchy_index_address(g_ui->node_hierarchy, g_ui->root);
	node->pixel_position[0] = node->layout_position[0];
	node->pixel_position[1] = node->layout_position[1];
	node->pixel_size[0] = node->layout_size[0];
	node->pixel_size[1] = node->layout_size[1];
	node->pixel_visible[0] = intv_inline(node->pixel_position[0], node->pixel_position[0] + node->pixel_size[0]);
	node->pixel_visible[1] = intv_inline(node->pixel_position[1], node->pixel_position[1] + node->pixel_size[1]);

	while(it.count)
	{
		const u32 index = hierarchy_index_iterator_next_df(&it);
		node = hierarchy_index_address(g_ui->node_hierarchy, index);

		//fprintf(stderr, "%s\n", (char *) node->id.buf);
		//vec2_print("position", node->pixel_position);
		//vec2_print("size", node->pixel_size);
		//vec2_print("layout_position", node->layout_position);
		//vec2_print("layout_size", node->layout_size);
		//fprintf(stderr, "visible area: [%f, %f] x [%f, %f]\n"
		//		,node->pixel_visible[AXIS_2_X].low
		//		,node->pixel_visible[AXIS_2_X].high
		//		,node->pixel_visible[AXIS_2_Y].low
		//		,node->pixel_visible[AXIS_2_Y].high);
		struct ui_node *child = NULL;
		f32 child_layout_axis_offset = (node->child_layout_axis == AXIS_2_X) 
			? 0.0f
			: node->pixel_size[1];
		const u32 non_layout_axis = 1 - node->child_layout_axis;
		for (u32 next = node->header.first; next != HI_NULL_INDEX; next = child->header.next)
		{
			child = hierarchy_index_address(g_ui->node_hierarchy, next);
			f32 new_child_layout_axis_offset = child_layout_axis_offset;

			if (child->flags & (UI_PERC_POSTPONED_X << node->child_layout_axis))
			{
				child->layout_position[node->child_layout_axis] = 0.0f;
				child->layout_size[node->child_layout_axis] = child->semantic_size[node->child_layout_axis].percentage * node->pixel_size[node->child_layout_axis];
			}
			else
			{
				if ((child->flags & (UI_FLOATING_X << node->child_layout_axis)) == 0)
				{
					new_child_layout_axis_offset = (node->child_layout_axis == AXIS_2_X)
					       ? child_layout_axis_offset + child->layout_size[AXIS_2_X]
					       : child_layout_axis_offset - child->layout_size[AXIS_2_Y];
				}
			}

			if (child->flags & (UI_PERC_POSTPONED_X << non_layout_axis))
			{
				child->layout_position[non_layout_axis] = 0.0f;
				child->layout_size[non_layout_axis] = child->semantic_size[non_layout_axis].percentage * node->pixel_size[non_layout_axis];
			}

			if (node->child_layout_axis == AXIS_2_X)
			{
				child->layout_position[AXIS_2_X] = ((child->flags & (UI_FIXED_X | UI_PERC_POSTPONED_X)) || child->semantic_size[AXIS_2_X].type == UI_SIZE_UNIT)
					? child->layout_position[AXIS_2_X]
					: child_layout_axis_offset;

				child->layout_position[AXIS_2_Y] = (child->flags & UI_FIXED_Y || child->semantic_size[AXIS_2_Y].type == UI_SIZE_UNIT)
				       	? child->layout_position[AXIS_2_Y]
			       		: 0.0f;
			}
			else
			{
				child->layout_position[AXIS_2_Y] = ((child->flags & (UI_FIXED_Y | UI_PERC_POSTPONED_Y)) || child->semantic_size[AXIS_2_Y].type == UI_SIZE_UNIT)
					? child->layout_position[AXIS_2_Y]
					: child_layout_axis_offset - child->layout_size[AXIS_2_Y];

				child->layout_position[AXIS_2_X] = (child->flags & UI_FIXED_X || child->semantic_size[AXIS_2_X].type == UI_SIZE_UNIT)
				       	? child->layout_position[AXIS_2_X]
			       		: 0.0f;
			}

			child_layout_axis_offset = new_child_layout_axis_offset;

			child->pixel_size[0] = child->layout_size[0];
			child->pixel_size[1] = child->layout_size[1];
			child->pixel_position[0] = (child->flags & UI_FIXED_X)
			       	? child->layout_position[0]
		       		: child->layout_position[0] + node->pixel_position[0];
			child->pixel_position[1] = (child->flags & UI_FIXED_Y)
			       	? child->layout_position[1]
		       		: child->layout_position[1] + node->pixel_position[1];

			child->pixel_visible[AXIS_2_X] = (child->flags & UI_FLOATING_X)
				? intv_inline(child->pixel_position[0], child->pixel_position[0] + child->pixel_size[0])
				: intv_inline(f32_max(child->pixel_position[0], node->pixel_visible[0].low),
					      f32_min(child->pixel_position[0] + child->pixel_size[0], node->pixel_visible[AXIS_2_X].high));
			child->pixel_visible[AXIS_2_Y] = (child->flags & UI_FLOATING_Y)
				? intv_inline(child->pixel_position[1], child->pixel_position[1] + child->pixel_size[1])
				: intv_inline(f32_max(child->pixel_position[1], node->pixel_visible[1].low),
					      f32_min(child->pixel_position[1] + child->pixel_size[1], node->pixel_visible[AXIS_2_Y].high));

			if (child->flags & UI_TEXT_LAYOUT_POSTPONED)
			{
				const f32 line_width = (child->flags & UI_TEXT_ALLOW_OVERFLOW)
					? F32_INFINITY
					: f32_max(0.0f, child->pixel_size[0] - 2.0f*child->text_pad[0]);
				child->layout_text = utf32_text_layout(g_ui->mem_frame, &child->text, line_width, TAB_SIZE, child->font);
			}
		}
	}

	hierarchy_index_iterator_release(&it);
	arena_free_1MB(&tmp);
	KAS_END;
}

static void ui_node_set_interactions(struct ui_inter_node **inter, const struct ui_node *node, const u64 local_interaction_flags)
{
	/*
	 * NOTE: By setting node->flags |= 
	 * ((struct ui_inter_node *) stack_ptr_top(&g_ui->stack_recursive_interaction))->flags, we simplify the 
	 * logic by always making sure that the set of recursive interactions of the node is a subset of its local
	 * interactions.	
	 */ 
	const struct ui_inter_node *inter_prev = node->inter; 

	u64 interactions = UI_FLAG_NONE;
	const u32 *key_clicked = key_zero_stub;	
	u32 node_clicked = 0;
	u32 node_dragged = 0;
	u32 node_scrolled = 0;

	if (inter_prev->hovered)
	{
		interactions |=  UI_INTER_HOVER 
			      | (UI_INTER_LEFT_CLICK*g_ui->inter.button_clicked[MOUSE_BUTTON_LEFT])
			      | (UI_INTER_SCROLL*(!!(g_ui->inter.scroll_up_count + g_ui->inter.scroll_down_count)));
		node_clicked = g_ui->inter.button_clicked[MOUSE_BUTTON_LEFT];
		node_dragged = g_ui->inter.button_clicked[MOUSE_BUTTON_LEFT] * g_ui->inter.button_pressed[MOUSE_BUTTON_LEFT];
		node_scrolled = g_ui->inter.scroll_up_count + g_ui->inter.scroll_down_count;
	}

	if (node_dragged || (inter_prev->drag && !g_ui->inter.button_released[MOUSE_BUTTON_LEFT]))
	{
		interactions |= UI_INTER_DRAG;
		node_dragged = 1;
	}

	if (inter_prev->active || (interactions & local_interaction_flags) != UI_FLAG_NONE || (node->flags & UI_INTER_RECURSIVE_ROOT))
	{
		*inter = arena_push(g_ui->mem_frame, sizeof(struct ui_inter_node));
		const u32 node_index = array_list_index(g_ui->node_hierarchy->list, node);
		(*inter)->recursive_flags = stack_u64_top(&g_ui->stack_recursive_interaction_flags);
		(*inter)->local_flags = local_interaction_flags;
		(*inter)->node_owner = node_index;
		(*inter)->clicked = node_clicked;
		(*inter)->drag = node_dragged;
		(*inter)->scrolled = node_scrolled;
		(*inter)->hovered = inter_prev->hovered;
		(*inter)->active = ((local_interaction_flags & UI_INTER_LEFT_CLICK)*node_clicked)
				 | ((local_interaction_flags & UI_INTER_DRAG)*node_dragged)
				 | ((local_interaction_flags & UI_INTER_SCROLL)*node_scrolled);
		
		if ((*inter)->active)
		{
			(*inter)->key_clicked = g_ui->inter.key_clicked;
			(*inter)->key_pressed = g_ui->inter.key_pressed;
			(*inter)->key_released = g_ui->inter.key_released;
		}

		for (u32 i = g_ui->stack_recursive_interaction.next-1; i; --i)
		{
			struct ui_inter_node *inherited = g_ui->stack_recursive_interaction.arr[i];
			kas_assert((inherited->recursive_flags & local_interaction_flags) == inherited->recursive_flags);
			if ((inherited->recursive_flags & interactions) == 0)
			{
				break;
			}

			inherited->clicked |= node_clicked;
			inherited->drag |= node_dragged;
			inherited->scrolled |= node_scrolled;
			inherited->hovered |= inter_prev->hovered;
			inherited->active = ((inherited->local_flags & UI_INTER_LEFT_CLICK)*node_clicked)
					  | ((inherited->local_flags & UI_INTER_DRAG)*node_dragged)
					  | ((inherited->local_flags & UI_INTER_SCROLL)*node_scrolled);
		}
	}

	
}

static void assert_inter_stub(void)
{
	const struct ui_inter_node *stub = g_ui->inter.inter_stub;
	kas_assert(stub->local_flags == 0);	
	kas_assert(stub->node_owner == 0);	
	kas_assert(stub->hovered == 0);
	kas_assert(stub->clicked == 0);
	kas_assert(stub->key_clicked == key_zero_stub);
	kas_assert(stub->key_released == key_zero_stub);
	kas_assert(stub->key_pressed == key_zero_stub);
	kas_assert(stub->drag == 0);
	kas_assert(stub->drag_delta[0] == 0);
	kas_assert(stub->drag_delta[1] == 0);
}

void ui_frame_begin(const vec2u32 window_size, const struct ui_visual *base)
{
	g_ui->frame += 1;
	g_ui->mem_frame = g_ui->mem_frame_arr + (g_ui->frame & 0x1);
	arena_flush(g_ui->mem_frame);
	array_list_intrusive_flush(g_ui->bucket_allocator);
	hash_map_flush(g_ui->bucket_map);
	
	/* setup stub bucket */
	g_ui->bucket_first = array_list_intrusive_reserve(g_ui->bucket_allocator);
	g_ui->bucket_last = g_ui->bucket_first;
	g_ui->bucket_cache = g_ui->bucket_first;
	g_ui->bucket_count = 0;
	g_ui->bucket_first->cmd = 0;
	g_ui->bucket_first->count = 0;

	g_ui->frame_stack_text_selection.next = 0;

	/* assert no stub interactions having been written. */
	assert_inter_stub();
	ui_inter_node_recursive_push(g_ui->inter.inter_stub);

	g_ui->node_count_prev_frame = g_ui->node_count_frame;
	g_ui->node_count_frame = 0;

	g_ui->window_size[0] = window_size[0];
	g_ui->window_size[1] = window_size[1];

	ui_external_text_push((utf32) { .len = 0, .max_len = 0, .buf = NULL });

	ui_flags_push(UI_INTER_HOVER);

	ui_child_layout_axis_push(AXIS_2_X);

	ui_font_push(base->font);

	ui_border_size_push(base->border_size);
	ui_corner_radius_push(base->corner_radius);

	ui_width_push(ui_size_perc(1.0f));
	ui_height_push(ui_size_perc(1.0f));
	ui_padding_push(base->pad);

	ui_text_align_x_push(base->text_alignment_x);
	ui_text_align_y_push(base->text_alignment_y);
	ui_text_pad_push(AXIS_2_X, base->text_pad_x);
	ui_text_pad_push(AXIS_2_Y, base->text_pad_y);

	ui_background_color_push(base->background_color);
	ui_border_color_push(base->border_color);
	ui_gradient_color_push(BOX_CORNER_BR, base->gradient_color[BOX_CORNER_BR]);
	ui_gradient_color_push(BOX_CORNER_TR, base->gradient_color[BOX_CORNER_TR]);
	ui_gradient_color_push(BOX_CORNER_TL, base->gradient_color[BOX_CORNER_TL]);
	ui_gradient_color_push(BOX_CORNER_BL, base->gradient_color[BOX_CORNER_BL]);
	ui_sprite_color_push(base->sprite_color);

	vec4_set(g_ui->text_cursor_color, 0.9f, 0.9f, 0.9f, 0.6f);
	vec4_set(g_ui->text_selection_color, 0.7f, 0.7f, 0.9f, 0.6f);

	ui_floating_x(0.0f)
	ui_floating_y(0.0f)
	ui_width(ui_size_pixel((f32) g_ui->window_size[0], 1.0f))
	ui_height(ui_size_pixel((f32) g_ui->window_size[1], 1.0f))
	g_ui->root = ui_root_f("###root_%p", &g_ui->root).index;
	struct ui_node *root = hierarchy_index_address(g_ui->node_hierarchy, g_ui->root);
	root->pixel_visible[AXIS_2_X] = intv_inline(0.0f, (f32) window_size[0]);
	root->pixel_visible[AXIS_2_Y] = intv_inline(0.0f, (f32) window_size[1]);
	
	ui_node_push(g_ui->root);
}

static void ui_identify_hovered_node(void)
{
	//TODO Consider returning stub constant if if-statements occur; 
	struct ui_node *node = ui_node_lookup(&g_ui->inter.node_hovered);
	if (node)
	{
		node->inter->hovered = 0;
	}

	const f32 x = g_ui->inter.cursor_position[0];
	const f32 y = g_ui->inter.cursor_position[1];
	i32 depth = -1;
	u32 index = HI_NULL_INDEX;
	/* find deepest hashed floating subtree which we are hovering */
	for (u32 i = 0; i < g_ui->stack_floating_node.next; ++i)
	{
		const u32 new_depth = g_ui->stack_floating_depth.arr[i];
		if (depth < (i32) new_depth)
		{
			const u32 new_index = g_ui->stack_floating_node.arr[i];
			node = hierarchy_index_address(g_ui->node_hierarchy, new_index);
			if (node->pixel_visible[0].low <= x && x <= node->pixel_visible[0].high &&  
		    	    node->pixel_visible[1].low <= y && y <= node->pixel_visible[1].high &&
			    (node->flags & (UI_NON_HASHED | UI_SKIP_HOVER_SEARCH)) == 0)
			{
				depth = (i32) new_depth;
				index = new_index;
			}
		}
	}

	if (index == HI_NULL_INDEX)
	{
		g_ui->inter.node_hovered = utf8_empty();
		return;
	}
	
	/* search floating subtree for deepest node we are hovering that is hashed */
	u32 deepest_non_hashed_hover_index = index;
	node = hierarchy_index_address(g_ui->node_hierarchy, index);
	kas_assert((node->flags & (UI_NON_HASHED | UI_SKIP_HOVER_SEARCH)) == 0);
	index = node->header.first;
	while (index != HI_NULL_INDEX)
	{
		node = hierarchy_index_address(g_ui->node_hierarchy, index);
		if (node->pixel_visible[0].low <= x && x <= node->pixel_visible[0].high &&  
	    	    node->pixel_visible[1].low <= y && y <= node->pixel_visible[1].high &&
		    (node->flags & UI_SKIP_HOVER_SEARCH) == 0)
		{
			if ((node->flags & UI_NON_HASHED) == 0)
			{
				deepest_non_hashed_hover_index = index;
			}

			index = node->header.first;
			continue;
		}

		index = node->header.next;
	}

	node = hierarchy_index_address(g_ui->node_hierarchy, deepest_non_hashed_hover_index);
	kas_assert((node->flags & (UI_NON_HASHED | UI_SKIP_HOVER_SEARCH)) == 0);
	if ((node->flags & UI_INTER_HOVER) && node->inter == g_ui->inter.inter_stub)
	{
		node->inter = arena_push(g_ui->mem_frame, sizeof(struct ui_inter_node));
		memset(node->inter, 0, sizeof(struct ui_inter_node));
		node->inter->node_owner = deepest_non_hashed_hover_index;
	}
	node->inter->hovered = 1;
	g_ui->inter.node_hovered = node->id;
}

static struct slot ui_text_selection_alloc(const struct ui_node *node, const vec4 color, const u32 low, const u32 high)
{
	const f32 line_width = (node->flags & UI_TEXT_ALLOW_OVERFLOW)
		? F32_INFINITY
		: f32_max(0.0f, node->pixel_size[0] - 2.0f*node->text_pad[0]);
	const struct ui_text_selection selection = 
	{
		.node = node,
		.layout = utf32_text_layout_include_whitespace(g_ui->mem_frame, &node->text, line_width, TAB_SIZE, node->font),
		.color = { color[0], color[1], color[2], color[3], },
		.low = low,
		.high = high,
	};

	const u32 index = g_ui->frame_stack_text_selection.next;
	stack_ui_text_selection_push(&g_ui->frame_stack_text_selection, selection);

	const u32 draw_key = UI_DRAW_COMMAND(node->depth, UI_CMD_LAYER_TEXT_SELECTION, asset_database_sprite_get_texture_id(node->sprite));
	ui_draw_bucket_add_node(draw_key, index);
	return (struct slot) { .index = index, .address = g_ui->frame_stack_text_selection.arr + index };
}

void ui_frame_end(void)
{	
	ui_node_pop();

	ui_flags_pop();

	ui_child_layout_axis_pop();

	ui_external_text_pop();

	ui_border_size_pop();
	ui_corner_radius_pop();

	ui_font_pop();

	ui_width_pop();
	ui_height_pop();
	ui_padding_pop();

	ui_text_align_x_pop();
	ui_text_align_y_pop();
	ui_text_pad_pop(AXIS_2_X);
	ui_text_pad_pop(AXIS_2_Y);

	ui_background_color_pop();
	ui_border_color_pop();
	ui_gradient_color_pop(BOX_CORNER_BR);
	ui_gradient_color_pop(BOX_CORNER_TR);
	ui_gradient_color_pop(BOX_CORNER_TL);
	ui_gradient_color_pop(BOX_CORNER_BL);
	ui_sprite_color_pop();

	ui_inter_node_recursive_pop();

	ui_childsum_layout_size_and_prune_nodes();
	ui_solve_violations();
	ui_layout_absolute_position();
	ui_identify_hovered_node();

	stack_u32_flush(&g_ui->stack_floating_node);
	stack_u32_flush(&g_ui->stack_floating_depth);

	for (u32 i = 0; i < KAS_KEY_COUNT; ++i)
	{
		g_ui->inter.key_clicked[i] = 0;
		g_ui->inter.key_released[i] = 0;
	}

	for (u32 i = 0; i < MOUSE_BUTTON_COUNT; ++i)
	{
		g_ui->inter.button_double_clicked[i] = 0;
		g_ui->inter.button_clicked[i] = 0;
		g_ui->inter.button_released[i] = 0;
	}
	g_ui->inter.scroll_up_count = 0;
	g_ui->inter.scroll_down_count = 0;

	UNPOISON_ADDRESS(g_ui->inter.cursor_delta, sizeof(vec2));
	g_ui->inter.cursor_delta[0] = 0;
	g_ui->inter.cursor_delta[1] = 0;
	POISON_ADDRESS(g_ui->inter.cursor_delta, sizeof(vec2));

	kas_assert(g_ui->stack_parent.next == 1);

	struct ui_node *text_input = ui_node_lookup(&g_ui->inter.text_edit.id);
	if (text_input)
	{
		/* update id to current frame memory and generate text selection draw commands */ 
		g_ui->inter.text_edit.id = text_input->id;

		/* draw cursor highlight */
		ui_text_selection_alloc(text_input, g_ui->text_cursor_color, g_ui->inter.text_edit.cursor, g_ui->inter.text_edit.cursor+1);

		if (g_ui->inter.text_edit.cursor+1 < g_ui->inter.text_edit.mark)
		{
			ui_text_selection_alloc(text_input, g_ui->text_selection_color, g_ui->inter.text_edit.cursor + 1, g_ui->inter.text_edit.mark);
		}
		else if (g_ui->inter.text_edit.mark < g_ui->inter.text_edit.cursor)
		{
			ui_text_selection_alloc(text_input, g_ui->text_selection_color, g_ui->inter.text_edit.mark, g_ui->inter.text_edit.cursor);
		}

	}
	else if (g_ui->inter.keyboard_text_input)
	{
		cmd_submit_f(g_ui->mem_frame, "ui_text_input_mode_disable");	
	}

	struct hierarchy_index_node *orphan = hierarchy_index_address(g_ui->node_hierarchy, HI_ORPHAN_STUB_INDEX);
	struct hierarchy_index_node *node; 
	for (u32 index = orphan->first; index != HI_NULL_INDEX;)
	{
		struct ui_node *node = hierarchy_index_address(g_ui->node_hierarchy, index);
		const u32 next = node->header.next;
		hierarchy_index_apply_custom_free_and_remove(g_ui->mem_frame, g_ui->node_hierarchy, index, &ui_node_remove_hash, NULL);
		index = next;
	}
	hierarchy_index_adopt_node(g_ui->node_hierarchy, g_ui->root, HI_ORPHAN_STUB_INDEX);
}

/* Calculate sizes known at time of creation, i.e. every size expect CHILDSUM */
static void ui_node_calculate_immediate_layout(struct ui_node *node, const enum axis_2 axis)
{
	switch (node->semantic_size[axis].type)
	{
		case UI_SIZE_PIXEL:
		{
			node->layout_size[axis] = node->semantic_size[axis].pixels;
		} break;

		case UI_SIZE_TEXT:
		{
			const f32 pad = 2.0f*node->text_pad[axis];
			if (node->flags & UI_TEXT_ATTACHED)
			{
				node->layout_size[axis] = (axis == AXIS_2_X)
					? pad + node->layout_text->width
					: pad + node->font->linespace*node->layout_text->line_count;
			}
			else
			{
				node->layout_size[axis] = pad;
			}
		} break;

		case UI_SIZE_PERC_PARENT:
		{
			const struct ui_node *parent = hierarchy_index_address(g_ui->node_hierarchy, node->header.parent);
			if (parent->semantic_size[axis].type == UI_SIZE_CHILDSUM || (parent->flags & (UI_PERC_POSTPONED_X << axis)))
			{
				node->layout_size[axis] = 0.0f;
				node->flags |= UI_PERC_POSTPONED_X << axis;
			}
			else
			{
				node->layout_size[axis] = node->semantic_size[axis].percentage * parent->layout_size[axis];	
			}
		} break;

		case UI_SIZE_UNIT:
		{
			const struct ui_node *parent = hierarchy_index_address(g_ui->node_hierarchy, node->header.parent);
			const intv visible = stack_intv_top(g_ui->stack_viewable + axis);
			const f32 pixels_per_unit = parent->layout_size[axis] / (visible.high - visible.low);

			node->layout_size[axis] = pixels_per_unit*(node->semantic_size[axis].intv.high - node->semantic_size[axis].intv.low);
			node->layout_position[axis] = pixels_per_unit*(node->semantic_size[axis].intv.low - visible.low);

			if ((axis == AXIS_2_Y) && (node->flags & UI_UNIT_POSITIVE_DOWN))
			{
				node->layout_position[axis] = parent->layout_size[axis] - node->layout_size[axis] - node->layout_position[axis];
			}
		} break;

		case UI_SIZE_CHILDSUM:
		{
			node->layout_position[axis] = 0.0f;
			node->layout_size[axis] = 0.0f;
		} break;

		default:
		{

		} break;
	}
}

static u32 internal_ui_pad(const u64 flags, const f32 value, const enum ui_size_type type)
{
	const u32 parent_index = stack_u32_top(&g_ui->stack_parent);

	if (parent_index == HI_ORPHAN_STUB_INDEX)
	{
		return HI_ORPHAN_STUB_INDEX;
	}

	struct slot slot = hierarchy_index_add(g_ui->node_hierarchy, parent_index);
	struct ui_node *node = slot.address;
	g_ui->node_count_frame += 1;

	struct ui_node *parent = hierarchy_index_address(g_ui->node_hierarchy, parent_index);
	const u32 non_layout_axis = 1 - parent->child_layout_axis;

	node->id = utf8_empty();
	node->flags = flags | stack_u64_top(&g_ui->stack_flags) | UI_DEBUG_FLAGS;
	node->last_frame_touched = g_ui->frame;
	node->semantic_size[parent->child_layout_axis] = (type == UI_SIZE_PIXEL)
		? ui_size_pixel(value, 0.0f)
		: ui_size_perc(value);
	node->semantic_size[non_layout_axis] = ui_size_perc(1.0f);
	node->child_layout_axis = stack_u32_top(&g_ui->stack_child_layout_axis);
	node->depth = (g_ui->stack_fixed_depth.next)
		? stack_u32_top(&g_ui->stack_fixed_depth)
		: parent->depth + 1;
	node->inter = g_ui->inter.inter_stub;

	if (node->flags & UI_DRAW_SPRITE)
	{
		node->sprite = stack_u32_top(&g_ui->stack_sprite);
		stack_vec4_top(node->sprite_color, &g_ui->stack_sprite_color);
	}
	else
	{
		node->sprite = SPRITE_NONE;
	}
	
	if (node->flags & UI_DRAW_FLAGS)
	{
		const u32 draw_key = (node->flags & UI_INTER_FLAGS)
			? UI_DRAW_COMMAND(node->depth, UI_CMD_LAYER_INTER, asset_database_sprite_get_texture_id(node->sprite))
			: UI_DRAW_COMMAND(node->depth, UI_CMD_LAYER_VISUAL, asset_database_sprite_get_texture_id(node->sprite));
		ui_draw_bucket_add_node(draw_key, slot.index);
	}

	node->text = utf32_empty();
	node->font = NULL;
	node->layout_text = NULL;

	/* set possible immediate sizes and positions */
	ui_node_calculate_immediate_layout(node, AXIS_2_X);
	ui_node_calculate_immediate_layout(node, AXIS_2_Y);

	(node->flags & UI_DRAW_BACKGROUND)
		? stack_vec4_top(node->background_color, &g_ui->stack_background_color)
		: vec4_set(node->background_color, 0.0f, 0.0f, 0.0f, 0.0f);

	if (node->flags & UI_DRAW_BORDER)
	{
		node->border_size = stack_f32_top(&g_ui->stack_border_size);
		stack_vec4_top(node->border_color, &g_ui->stack_border_color);
	}
	else
	{
		node->border_size = 0.0f;
		vec4_set(node->border_color, 0.0f, 0.0f, 0.0f, 0.0f);
	}

	if (node->flags & UI_DRAW_GRADIENT)
	{
		stack_vec4_top(node->gradient_color[BOX_CORNER_BR], g_ui->stack_gradient_color + BOX_CORNER_BR);
		stack_vec4_top(node->gradient_color[BOX_CORNER_TR], g_ui->stack_gradient_color + BOX_CORNER_TR);
		stack_vec4_top(node->gradient_color[BOX_CORNER_TL], g_ui->stack_gradient_color + BOX_CORNER_TL);
		stack_vec4_top(node->gradient_color[BOX_CORNER_BL], g_ui->stack_gradient_color + BOX_CORNER_BL);
	}
	else
	{
		vec4_set(node->gradient_color[BOX_CORNER_BR], 0.0f, 0.0f, 0.0f, 0.0f); 
                vec4_set(node->gradient_color[BOX_CORNER_TR], 0.0f, 0.0f, 0.0f, 0.0f);
                vec4_set(node->gradient_color[BOX_CORNER_TL], 0.0f, 0.0f, 0.0f, 0.0f);
                vec4_set(node->gradient_color[BOX_CORNER_BL], 0.0f, 0.0f, 0.0f, 0.0f);
	}

	node->edge_softness = (node->flags & UI_DRAW_EDGE_SOFTNESS)
		? stack_f32_top(&g_ui->stack_edge_softness)
		: 0.0f;

	node->corner_radius = (node->flags & UI_DRAW_ROUNDED_CORNERS)
		? stack_f32_top(&g_ui->stack_corner_radius)
		: 0.0f;
	
	return slot.index;
}

u32 ui_pad(void)
{
	return internal_ui_pad(UI_NON_HASHED | UI_PAD, stack_f32_top(&g_ui->stack_pad), UI_SIZE_PIXEL);
}

u32 ui_pad_pixel(const f32 pixel)
{
	return internal_ui_pad(UI_NON_HASHED | UI_PAD, pixel, UI_SIZE_PIXEL);
}

u32 ui_pad_perc(const f32 perc)
{
	return internal_ui_pad(UI_NON_HASHED | UI_PAD, perc, UI_SIZE_PERC_PARENT);
}

u32 ui_pad_fill(void)
{
	return internal_ui_pad(UI_NON_HASHED | UI_PAD | UI_PAD_FILL, 0.0f, UI_SIZE_PIXEL);
}

struct slot ui_node_alloc_non_hashed(const u64 flags)
{
	const utf8 id = utf8_empty();
	return ui_node_alloc(flags | UI_NON_HASHED, &id);
}

struct ui_node *ui_node_address(const u32 node)
{
	return array_list_address(g_ui->node_hierarchy->list, node);
}

struct ui_node *ui_node_lookup(const utf8 *id)
{
	struct ui_node *node;
	const u32 key = utf8_hash(*id);
	u32 index = hash_map_first(g_ui->node_map, key);
	for (; index != HASH_NULL; index = hash_map_next(g_ui->node_map, index))
	{
		node = hierarchy_index_address(g_ui->node_hierarchy, index);
		if (utf8_equivalence(node->id, *id))
		{
			break;
		}
	}

	return ((index != HASH_NULL) && (node->last_frame_touched == g_ui->frame))
		? node
		: NULL;
}

struct slot ui_node_alloc_cached(const u64 flags, const utf8 id, const u32 id_hash, const utf8 text, const u32 index_cached)
{
	const u32 parent_index = stack_u32_top(&g_ui->stack_parent);
	struct ui_node *parent = hierarchy_index_address(g_ui->node_hierarchy, parent_index);

	/* Parent failed to alloc */
	if (parent_index == HI_ORPHAN_STUB_INDEX)
	{
		return (struct slot) { .index = HI_ORPHAN_STUB_INDEX, .address = hierarchy_index_address(g_ui->node_hierarchy, HI_ORPHAN_STUB_INDEX) };
	}

	u64 implied_flags = stack_u64_top(&g_ui->stack_flags);

	/* If not cached, index should be != STUB_INDEX */
	struct ui_node *node = hierarchy_index_address(g_ui->node_hierarchy, index_cached);
	struct ui_size size_x = stack_ui_size_top(g_ui->stack_ui_size + AXIS_2_X);
	struct ui_size size_y = stack_ui_size_top(g_ui->stack_ui_size + AXIS_2_Y);

	/* Cull any unit_sized nodes that are not visible except if they are being interacted with */
	if (size_x.type == UI_SIZE_UNIT)
	{
		kas_assert(g_ui->stack_viewable[AXIS_2_X].next);
		implied_flags |= UI_ALLOW_VIOLATION_X;

		const intv visible = stack_intv_top(g_ui->stack_viewable + AXIS_2_X);
		if ((size_x.intv.high < visible.low || size_x.intv.low > visible.high) && !node->inter->active)
		{
			return (struct slot) { .index = HI_ORPHAN_STUB_INDEX, .address = hierarchy_index_address(g_ui->node_hierarchy, HI_ORPHAN_STUB_INDEX) };
		}
	}

	if (size_y.type == UI_SIZE_UNIT)
	{
		kas_assert(g_ui->stack_viewable[AXIS_2_Y].next);
		implied_flags |= UI_ALLOW_VIOLATION_Y;
		
		const intv visible = stack_intv_top(g_ui->stack_viewable + AXIS_2_Y);
		if ((size_y.intv.high < visible.low || size_y.intv.low > visible.high) && !node->inter->active)
		{
			return (struct slot) { .index = HI_ORPHAN_STUB_INDEX, .address = hierarchy_index_address(g_ui->node_hierarchy, HI_ORPHAN_STUB_INDEX) };
		}
	}

	struct ui_inter_node *inter = g_ui->inter.inter_stub;
 
	const u64 node_flags = flags | implied_flags | UI_DEBUG_FLAGS | ((struct ui_inter_node *) stack_ptr_top(&g_ui->stack_recursive_interaction))->recursive_flags;

	const u32 depth = (g_ui->stack_fixed_depth.next)
		? stack_u32_top(&g_ui->stack_fixed_depth)
		: parent->depth + 1;

	struct slot slot;
	if (index_cached == UI_NON_CACHED_INDEX)
	{
		slot = hierarchy_index_add(g_ui->node_hierarchy, stack_u32_top(&g_ui->stack_parent));
		node = slot.address;
		hash_map_add(g_ui->node_map, id_hash, slot.index);
	}
	else
	{
		slot.address = node;
		slot.index = index_cached;
		hierarchy_index_adopt_node_exclusive(g_ui->node_hierarchy, slot.index, stack_u32_top(&g_ui->stack_parent));
		ui_node_set_interactions(&inter, node, node_flags);
	}
	

	g_ui->node_count_frame += 1;

	node->id = id;
	node->key = id_hash;
	node->flags = node_flags;
	node->last_frame_touched = g_ui->frame;
	node->semantic_size[AXIS_2_X] = size_x;
	node->semantic_size[AXIS_2_Y] = size_y;
	node->child_layout_axis = stack_u32_top(&g_ui->stack_child_layout_axis);
	node->depth = depth;
	node->inter = inter;

	if (node->flags & UI_DRAW_SPRITE)
	{
		node->sprite = stack_u32_top(&g_ui->stack_sprite);
		stack_vec4_top(node->sprite_color, &g_ui->stack_sprite_color);
	}
	else
	{
		node->sprite = SPRITE_NONE;
	}
	
	if (node->flags & UI_DRAW_FLAGS)
	{
		const u32 draw_key = (node->flags & UI_INTER_FLAGS)
			? UI_DRAW_COMMAND(node->depth, UI_CMD_LAYER_INTER, asset_database_sprite_get_texture_id(node->sprite))
			: UI_DRAW_COMMAND(node->depth, UI_CMD_LAYER_VISUAL, asset_database_sprite_get_texture_id(node->sprite));
		ui_draw_bucket_add_node(draw_key, slot.index);
	}

	if (node->flags & UI_DRAW_TEXT)
	{
		const struct asset_font *asset = stack_ptr_top(&g_ui->stack_font);
		stack_vec4_top(node->sprite_color, &g_ui->stack_sprite_color);
		node->flags |= UI_TEXT_ATTACHED;
		node->font = asset->font;
		node->text_align_x = stack_u32_top(&g_ui->stack_text_alignment_x);
		node->text_align_y = stack_u32_top(&g_ui->stack_text_alignment_y);
		node->text_pad[AXIS_2_X] = stack_f32_top(g_ui->stack_text_pad + AXIS_2_X);
		node->text_pad[AXIS_2_Y] = stack_f32_top(g_ui->stack_text_pad + AXIS_2_Y);

		if (node->flags & UI_TEXT_EXTERNAL_LAYOUT)
		{
			node->flags |= UI_TEXT_EXTERNAL | UI_TEXT_ALLOW_OVERFLOW;
			node->text = stack_utf32_top(&g_ui->stack_external_text); 
			node->layout_text = stack_ptr_top(&g_ui->stack_external_text_layout); 
		}
		else
		{
			node->text = (node->flags & UI_TEXT_EXTERNAL)
				? stack_utf32_top(&g_ui->stack_external_text)
				: utf32_utf8(g_ui->mem_frame, text);

			/* TODO wonky, would be nice to have it in immediate calculations, but wonky there as well... */
			if (node->semantic_size[AXIS_2_X].type == UI_SIZE_TEXT)
			{
				node->semantic_size[AXIS_2_X].line_width = (node->flags & UI_TEXT_ALLOW_OVERFLOW)
					? F32_INFINITY
					: node->semantic_size[AXIS_2_X].line_width;
				node->layout_text = utf32_text_layout(g_ui->mem_frame, &node->text, node->semantic_size[AXIS_2_X].line_width, TAB_SIZE, node->font);
			}
			else
			{
				node->flags |= UI_TEXT_LAYOUT_POSTPONED;
			}
		}

		/* visual first (10), inter second(01), text last(00) */
		const u32 draw_key = UI_DRAW_COMMAND(node->depth, UI_CMD_LAYER_TEXT, asset->texture_id);
		ui_draw_bucket_add_node(draw_key, slot.index);
	}
	else
	{
		node->text = utf32_empty();
		vec4_set(node->sprite_color, 0.0f, 0.0f, 0.0f, 0.0f);
		node->font = NULL;
		node->layout_text = NULL;
	}

	/* set possible immediate sizes and positions */
	ui_node_calculate_immediate_layout(node, AXIS_2_X);
	ui_node_calculate_immediate_layout(node, AXIS_2_Y);

	u32 floating = 0;
	if (g_ui->stack_floating[AXIS_2_X].next)
	{
		floating = 1;
		node->layout_position[AXIS_2_X] = stack_f32_top(g_ui->stack_floating + AXIS_2_X);
		node->flags |= UI_FLOATING_X | UI_FIXED_X;
	}	

	if (g_ui->stack_floating[AXIS_2_Y].next)
	{
		floating = 1;
		node->layout_position[AXIS_2_Y] = stack_f32_top(g_ui->stack_floating + AXIS_2_Y);
		node->flags |= UI_FLOATING_Y | UI_FIXED_Y;
	}

	if (floating)
	{
		stack_u32_push(&g_ui->stack_floating_node, slot.index);
		stack_u32_push(&g_ui->stack_floating_depth, node->depth);
	}

	(node->flags & UI_DRAW_BACKGROUND)
		? stack_vec4_top(node->background_color, &g_ui->stack_background_color)
		: vec4_set(node->background_color, 0.0f, 0.0f, 0.0f, 0.0f);

	if (node->flags & UI_DRAW_BORDER)
	{
		node->border_size = stack_f32_top(&g_ui->stack_border_size);
		stack_vec4_top(node->border_color, &g_ui->stack_border_color);
	}
	else
	{
		node->border_size = 0.0f;
		vec4_set(node->border_color, 0.0f, 0.0f, 0.0f, 0.0f);
	}

	if (node->flags & UI_DRAW_GRADIENT)
	{
		stack_vec4_top(node->gradient_color[BOX_CORNER_BR], g_ui->stack_gradient_color + BOX_CORNER_BR);
		stack_vec4_top(node->gradient_color[BOX_CORNER_TR], g_ui->stack_gradient_color + BOX_CORNER_TR);
		stack_vec4_top(node->gradient_color[BOX_CORNER_TL], g_ui->stack_gradient_color + BOX_CORNER_TL);
		stack_vec4_top(node->gradient_color[BOX_CORNER_BL], g_ui->stack_gradient_color + BOX_CORNER_BL);
	}
	else
	{
		vec4_set(node->gradient_color[BOX_CORNER_BR], 0.0f, 0.0f, 0.0f, 0.0f); 
                vec4_set(node->gradient_color[BOX_CORNER_TR], 0.0f, 0.0f, 0.0f, 0.0f);
                vec4_set(node->gradient_color[BOX_CORNER_TL], 0.0f, 0.0f, 0.0f, 0.0f);
                vec4_set(node->gradient_color[BOX_CORNER_BL], 0.0f, 0.0f, 0.0f, 0.0f);
	}

	node->edge_softness = (node->flags & UI_DRAW_EDGE_SOFTNESS)
		? stack_f32_top(&g_ui->stack_edge_softness)
		: 0.0f;

	node->corner_radius = (node->flags & UI_DRAW_ROUNDED_CORNERS)
		? stack_f32_top(&g_ui->stack_corner_radius)
		: 0.0f;
	
	kas_assert(node->semantic_size[AXIS_2_Y].type != UI_SIZE_TEXT || node->semantic_size[AXIS_2_X].type == UI_SIZE_TEXT);

	return slot;
}

struct slot ui_node_alloc(const u64 flags, const utf8 *formatted)
{
	u64 implied_flags = stack_u64_top(&g_ui->stack_flags);
	struct ui_size size_x = stack_ui_size_top(g_ui->stack_ui_size + AXIS_2_X);
	struct ui_size size_y = stack_ui_size_top(g_ui->stack_ui_size + AXIS_2_Y);

	const u32 parent_index = stack_u32_top(&g_ui->stack_parent);
	struct ui_node *parent = hierarchy_index_address(g_ui->node_hierarchy, parent_index);

	if (parent_index == HI_ORPHAN_STUB_INDEX)
	{
		return (struct slot) { .index = HI_ORPHAN_STUB_INDEX, .address = hierarchy_index_address(g_ui->node_hierarchy, HI_ORPHAN_STUB_INDEX) };
	}

	if (size_x.type == UI_SIZE_UNIT)
	{
		kas_assert(g_ui->stack_viewable[AXIS_2_X].next);
		implied_flags |= UI_ALLOW_VIOLATION_X;

		const intv visible = stack_intv_top(g_ui->stack_viewable + AXIS_2_X);
		if (size_x.intv.high < visible.low || size_x.intv.low > visible.high)
		{
			return (struct slot) { .index = HI_ORPHAN_STUB_INDEX, .address = hierarchy_index_address(g_ui->node_hierarchy, HI_ORPHAN_STUB_INDEX) };
		}
	}

	if (size_y.type == UI_SIZE_UNIT)
	{
		kas_assert(g_ui->stack_viewable[AXIS_2_Y].next);
		implied_flags |= UI_ALLOW_VIOLATION_Y;
		
		const intv visible = stack_intv_top(g_ui->stack_viewable + AXIS_2_Y);
		if (size_y.intv.high < visible.low || size_y.intv.low > visible.high)
		{
			return (struct slot) { .index = HI_ORPHAN_STUB_INDEX, .address = hierarchy_index_address(g_ui->node_hierarchy, HI_ORPHAN_STUB_INDEX) };
		}
	}

	u32 hash_count = 0;
	u32 hash_begin_index = 0;
	u32 hash_begin_offset = 0;
	u64 offset = 0;
	u32 text_len = formatted->len;
	for (u32 i = 0; i < formatted->len; ++i)
	{
		const u32 codepoint = utf8_read_codepoint(&offset, formatted, offset);
		if (codepoint == '#')
		{
			hash_count += 1;
			if (hash_count == 3)
			{
				hash_begin_index = i+1;
				hash_begin_offset = (u32) offset;
				text_len = i-2;
				break;
			}
			else if (hash_count == 2 && i+1 == formatted->len)
			{
				text_len = i-2;
			}
		}
		else if (hash_count == 2)
		{
			text_len = i-2;
			break;
		}
		else
		{
			hash_count = 0;
		}
	}

	const utf8 id = (utf8) { .buf = formatted->buf + hash_begin_offset, .len = formatted->len - hash_begin_index, .size = formatted->size - hash_begin_offset };
	struct ui_inter_node *inter = g_ui->inter.inter_stub;
 
	const u64 node_flags = flags | implied_flags | UI_DEBUG_FLAGS | ((struct ui_inter_node *) stack_ptr_top(&g_ui->stack_recursive_interaction))->recursive_flags;
	u32 key = 0;
	struct slot slot;
	u32 index;
	struct ui_node *node;
	if (flags & UI_NON_HASHED)
	{
		slot = hierarchy_index_add(g_ui->node_hierarchy, parent_index);
		parent = hierarchy_index_address(g_ui->node_hierarchy, parent_index);
		node = slot.address;
		index = slot.index;
	}
	else
	{
		key = utf8_hash(id);
		for (index = hash_map_first(g_ui->node_map, key); index != HASH_NULL; index = hash_map_next(g_ui->node_map, index))
		{
			node = hierarchy_index_address(g_ui->node_hierarchy, index);
			if (utf8_equivalence(node->id, id))
			{
				kas_assert(node->last_frame_touched != g_ui->frame);
				break;
			}
		}

		if (index == HASH_NULL)
		{
			slot = hierarchy_index_add(g_ui->node_hierarchy, stack_u32_top(&g_ui->stack_parent));
			node = slot.address;
			index = slot.index;
			hash_map_add(g_ui->node_map, key, index);
			parent = hierarchy_index_address(g_ui->node_hierarchy, parent_index);
		}
		else
		{
			hierarchy_index_adopt_node_exclusive(g_ui->node_hierarchy, index, stack_u32_top(&g_ui->stack_parent));
			ui_node_set_interactions(&inter, node, node_flags);
		}
	}

	g_ui->node_count_frame += 1;

	node->id = id;
	node->key = key;
	node->flags = node_flags;
	node->last_frame_touched = g_ui->frame;
	node->semantic_size[AXIS_2_X] = size_x;
	node->semantic_size[AXIS_2_Y] = size_y;
	node->child_layout_axis = stack_u32_top(&g_ui->stack_child_layout_axis);
	node->depth = (g_ui->stack_fixed_depth.next)
		? stack_u32_top(&g_ui->stack_fixed_depth)
		: parent->depth + 1;
	node->inter = inter;

	if (node->flags & UI_DRAW_SPRITE)
	{
		node->sprite = stack_u32_top(&g_ui->stack_sprite);
		stack_vec4_top(node->sprite_color, &g_ui->stack_sprite_color);
	}
	else
	{
		node->sprite = SPRITE_NONE;
	}
	
	if (node->flags & UI_DRAW_FLAGS)
	{
		const u32 draw_key = (node->flags & UI_INTER_FLAGS)
			? UI_DRAW_COMMAND(node->depth, UI_CMD_LAYER_INTER, asset_database_sprite_get_texture_id(node->sprite))
			: UI_DRAW_COMMAND(node->depth, UI_CMD_LAYER_VISUAL, asset_database_sprite_get_texture_id(node->sprite));
		ui_draw_bucket_add_node(draw_key, index);
	}

	if (node->flags & UI_DRAW_TEXT)
	{
		const struct asset_font *asset = stack_ptr_top(&g_ui->stack_font);
		stack_vec4_top(node->sprite_color, &g_ui->stack_sprite_color);
		node->flags |= UI_TEXT_ATTACHED;
		node->font = asset->font;
		node->text_align_x = stack_u32_top(&g_ui->stack_text_alignment_x);
		node->text_align_y = stack_u32_top(&g_ui->stack_text_alignment_y);
		node->text_pad[AXIS_2_X] = stack_f32_top(g_ui->stack_text_pad + AXIS_2_X);
		node->text_pad[AXIS_2_Y] = stack_f32_top(g_ui->stack_text_pad + AXIS_2_Y);

		if (node->flags & UI_TEXT_EXTERNAL_LAYOUT)
		{
			node->flags |= UI_TEXT_EXTERNAL | UI_TEXT_ALLOW_OVERFLOW;
			node->text = stack_utf32_top(&g_ui->stack_external_text); 
			node->layout_text = stack_ptr_top(&g_ui->stack_external_text_layout); 
		}
		else
		{
			node->text = (node->flags & UI_TEXT_EXTERNAL)
				? stack_utf32_top(&g_ui->stack_external_text)
				: utf32_utf8(g_ui->mem_frame, (utf8) { .buf = formatted->buf, .len = text_len, .size = formatted->size });

			/* TODO wonky, would be nice to have it in immediate calculations, but wonky there as well... */
			if (node->semantic_size[AXIS_2_X].type == UI_SIZE_TEXT)
			{
				node->semantic_size[AXIS_2_X].line_width = (node->flags & UI_TEXT_ALLOW_OVERFLOW)
					? F32_INFINITY
					: node->semantic_size[AXIS_2_X].line_width;
				node->layout_text = utf32_text_layout(g_ui->mem_frame, &node->text, node->semantic_size[AXIS_2_X].line_width, TAB_SIZE, node->font);
			}
			else
			{
				node->flags |= UI_TEXT_LAYOUT_POSTPONED;
			}
		}

		/* visual first (10), inter second(01), text last(00) */
		const u32 draw_key = UI_DRAW_COMMAND(node->depth, UI_CMD_LAYER_TEXT, asset->texture_id);
		ui_draw_bucket_add_node(draw_key, index);
	}
	else
	{
		node->text = utf32_empty();
		vec4_set(node->sprite_color, 0.0f, 0.0f, 0.0f, 0.0f);
		node->font = NULL;
		node->layout_text = NULL;
	}

	/* set possible immediate sizes and positions */
	ui_node_calculate_immediate_layout(node, AXIS_2_X);
	ui_node_calculate_immediate_layout(node, AXIS_2_Y);

	u32 floating = 0;
	if (g_ui->stack_floating[AXIS_2_X].next)
	{
		floating = 1;
		node->layout_position[AXIS_2_X] = stack_f32_top(g_ui->stack_floating + AXIS_2_X);
		node->flags |= UI_FLOATING_X | UI_FIXED_X;
	}	

	if (g_ui->stack_floating[AXIS_2_Y].next)
	{
		floating = 1;
		node->layout_position[AXIS_2_Y] = stack_f32_top(g_ui->stack_floating + AXIS_2_Y);
		node->flags |= UI_FLOATING_Y | UI_FIXED_Y;
	}


	if (floating)
	{
		stack_u32_push(&g_ui->stack_floating_node, index);
		stack_u32_push(&g_ui->stack_floating_depth, node->depth);
	}

	(node->flags & UI_DRAW_BACKGROUND)
		? stack_vec4_top(node->background_color, &g_ui->stack_background_color)
		: vec4_set(node->background_color, 0.0f, 0.0f, 0.0f, 0.0f);

	if (node->flags & UI_DRAW_BORDER)
	{
		node->border_size = stack_f32_top(&g_ui->stack_border_size);
		stack_vec4_top(node->border_color, &g_ui->stack_border_color);
	}
	else
	{
		node->border_size = 0.0f;
		vec4_set(node->border_color, 0.0f, 0.0f, 0.0f, 0.0f);
	}

	if (node->flags & UI_DRAW_GRADIENT)
	{
		stack_vec4_top(node->gradient_color[BOX_CORNER_BR], g_ui->stack_gradient_color + BOX_CORNER_BR);
		stack_vec4_top(node->gradient_color[BOX_CORNER_TR], g_ui->stack_gradient_color + BOX_CORNER_TR);
		stack_vec4_top(node->gradient_color[BOX_CORNER_TL], g_ui->stack_gradient_color + BOX_CORNER_TL);
		stack_vec4_top(node->gradient_color[BOX_CORNER_BL], g_ui->stack_gradient_color + BOX_CORNER_BL);
	}
	else
	{
		vec4_set(node->gradient_color[BOX_CORNER_BR], 0.0f, 0.0f, 0.0f, 0.0f); 
                vec4_set(node->gradient_color[BOX_CORNER_TR], 0.0f, 0.0f, 0.0f, 0.0f);
                vec4_set(node->gradient_color[BOX_CORNER_TL], 0.0f, 0.0f, 0.0f, 0.0f);
                vec4_set(node->gradient_color[BOX_CORNER_BL], 0.0f, 0.0f, 0.0f, 0.0f);
	}

	node->edge_softness = (node->flags & UI_DRAW_EDGE_SOFTNESS)
		? stack_f32_top(&g_ui->stack_edge_softness)
		: 0.0f;

	node->corner_radius = (node->flags & UI_DRAW_ROUNDED_CORNERS)
		? stack_f32_top(&g_ui->stack_corner_radius)
		: 0.0f;
	
	kas_assert(node->semantic_size[AXIS_2_Y].type != UI_SIZE_TEXT || node->semantic_size[AXIS_2_X].type == UI_SIZE_TEXT);

	return (struct slot) { .index = index, .address = node };
}

struct slot ui_node_alloc_f(const u64 flags, const char *format, ...)
{
	va_list args;
	va_start(args, format);
	utf8 id = utf8_format_variadic(g_ui->mem_frame, format, args);
	va_end(args);

	return ui_node_alloc(flags, &id);
}

/********************************************************************************************************/
/*					 Push/Pop global state  					*/
/********************************************************************************************************/

void ui_node_push(const u32 node)
{
	struct ui_node *node_ptr = hierarchy_index_address(g_ui->node_hierarchy, node);
	if (node_ptr->flags & UI_INTER_RECURSIVE_ROOT)
	{
		ui_inter_node_recursive_push(node_ptr->inter);
	}
	stack_u32_push(&g_ui->stack_parent, node);
}

void ui_node_pop(void)
{	
	const u32 node = stack_u32_pop(&g_ui->stack_parent);
	const struct ui_node *node_ptr = hierarchy_index_address(g_ui->node_hierarchy, node);
	if (node_ptr->flags & UI_INTER_RECURSIVE_ROOT)
	{
		ui_inter_node_recursive_pop();
	}
}

struct ui_node *ui_node_top(void)
{
	return array_list_address(g_ui->node_hierarchy->list, stack_u32_top(&g_ui->stack_parent));
}

void ui_size_push(const enum axis_2 axis, const struct ui_size size)
{
	stack_ui_size_push(g_ui->stack_ui_size + axis, size);
}

void ui_size_set(const enum axis_2 axis, const struct ui_size size)
{
	stack_ui_size_set(g_ui->stack_ui_size + axis, size);
}

void ui_size_pop(const enum axis_2 axis)
{
	stack_ui_size_pop(g_ui->stack_ui_size + axis);
}

void ui_width_push(const struct ui_size size)
{
	stack_ui_size_push(g_ui->stack_ui_size + AXIS_2_X, size);
}

void ui_width_set(const struct ui_size size)
{
	stack_ui_size_set(g_ui->stack_ui_size + AXIS_2_X, size);
}

void ui_width_pop(void)
{
	stack_ui_size_pop(g_ui->stack_ui_size + AXIS_2_X);
}

void ui_height_push(const struct ui_size size)
{
	stack_ui_size_push(g_ui->stack_ui_size + AXIS_2_Y, size);
}

void ui_height_set(const struct ui_size size)
{
	stack_ui_size_set(g_ui->stack_ui_size + AXIS_2_Y, size);
}

void ui_height_pop(void)
{
	stack_ui_size_pop(g_ui->stack_ui_size + AXIS_2_Y);
}

void ui_floating_push(const enum axis_2 axis, const f32 pixel)
{
	stack_f32_push(g_ui->stack_floating + axis, pixel);
}

void ui_floating_set(const enum axis_2 axis, const f32 pixel)
{
	stack_f32_set(g_ui->stack_floating + axis, pixel);
}

void ui_floating_pop(const enum axis_2 axis)
{
	stack_f32_pop(g_ui->stack_floating + axis);
}

void ui_child_layout_axis_push(const enum axis_2 axis)
{
	stack_u32_push(&g_ui->stack_child_layout_axis, axis);
}

void ui_child_layout_axis_set(const enum axis_2 axis)
{
	stack_u32_set(&g_ui->stack_child_layout_axis, axis);
}

void ui_child_layout_axis_pop(void)
{
	stack_u32_pop(&g_ui->stack_child_layout_axis);
}

void ui_intv_viewable_push(const enum axis_2 axis, const intv inv)
{
	stack_intv_push(g_ui->stack_viewable + axis, inv);
}

void ui_intv_viewable_set(const enum axis_2 axis, const intv inv)
{
	stack_intv_set(g_ui->stack_viewable + axis, inv);
}

void ui_intv_viewable_pop(const enum axis_2 axis)
{
	stack_intv_pop(g_ui->stack_viewable + axis);
}

void ui_background_color_push(const vec4 color)
{
	stack_vec4_push(&g_ui->stack_background_color, color);
}

void ui_background_color_set(const vec4 color)
{
	stack_vec4_set(&g_ui->stack_background_color, color);
}

void ui_background_color_pop(void)
{
	stack_vec4_pop(&g_ui->stack_background_color);
}

void ui_border_color_push(const vec4 color)
{
	stack_vec4_push(&g_ui->stack_border_color, color);
}

void ui_border_color_set(const vec4 color)
{
	stack_vec4_set(&g_ui->stack_border_color, color);
}

void ui_border_color_pop(void)
{
	stack_vec4_pop(&g_ui->stack_border_color);
}

void ui_sprite_color_push(const vec4 color)
{
	stack_vec4_push(&g_ui->stack_sprite_color, color);
}

void ui_sprite_color_set(const vec4 color)
{
	stack_vec4_set(&g_ui->stack_sprite_color, color);
}

void ui_sprite_color_pop(void)
{
	stack_vec4_pop(&g_ui->stack_sprite_color);
}

void ui_gradient_color_push(const enum box_corner corner, const vec4 color)
{
	stack_vec4_push(g_ui->stack_gradient_color + corner, color);
}

void ui_gradient_color_set(const enum box_corner corner, const vec4 color)
{
	stack_vec4_set(g_ui->stack_gradient_color + corner, color);
}

void ui_gradient_color_pop(const enum box_corner corner)
{
	stack_vec4_pop(g_ui->stack_gradient_color + corner);
}

void ui_font_push(const enum font_id font)
{
	struct asset_font *asset = asset_database_request_font(g_ui->mem_frame, font);
	stack_ptr_push(&g_ui->stack_font, asset);
}

void ui_font_set(const enum font_id font)
{
	struct asset_font *asset = asset_database_request_font(g_ui->mem_frame, font);
	stack_ptr_set(&g_ui->stack_font, asset);
}

void ui_font_pop(void)
{
	stack_ptr_pop(&g_ui->stack_font);
}

void ui_sprite_push(const enum sprite_id sprite)
{
	stack_u32_push(&g_ui->stack_sprite, sprite);
}

void ui_sprite_set(const enum sprite_id sprite)
{
	stack_u32_set(&g_ui->stack_sprite, sprite);
}

void ui_sprite_pop(void)
{
	stack_u32_pop(&g_ui->stack_sprite);
}

void ui_edge_softness_push(const f32 softness)
{
	stack_f32_push(&g_ui->stack_edge_softness, softness);
}

void ui_edge_softness_set(const f32 softness)
{
	stack_f32_set(&g_ui->stack_edge_softness, softness);
}

void ui_edge_softness_pop(void)
{
	stack_f32_pop(&g_ui->stack_edge_softness);
}

void ui_corner_radius_push(const f32 radius)
{
	stack_f32_push(&g_ui->stack_corner_radius, radius);
}

void ui_corner_radius_set(const f32 radius)
{
	stack_f32_set(&g_ui->stack_corner_radius, radius);
}

void ui_corner_radius_pop(void)
{
	stack_f32_pop(&g_ui->stack_corner_radius);
}

void ui_border_size_push(const f32 pixels)
{
	stack_f32_push(&g_ui->stack_border_size, pixels);
}

void ui_border_size_set(const f32 pixels)
{
	stack_f32_set(&g_ui->stack_border_size, pixels);
}

void ui_border_size_pop(void)
{
	stack_f32_pop(&g_ui->stack_border_size);
}

void ui_text_align_x_push(const enum alignment_x align)
{
	stack_u32_push(&g_ui->stack_text_alignment_x, align);
}

void ui_text_align_x_set(const enum alignment_x align)
{
	stack_u32_set(&g_ui->stack_text_alignment_x, align);
}

void ui_text_align_x_pop(void)
{
	stack_u32_pop(&g_ui->stack_text_alignment_x);
}

void ui_text_align_y_push(const enum alignment_y align)
{
	stack_u32_push(&g_ui->stack_text_alignment_y, align);
}

void ui_text_align_y_set(const enum alignment_y align)
{
	stack_u32_set(&g_ui->stack_text_alignment_y, align);
}

void ui_text_align_y_pop(void)
{
	stack_u32_pop(&g_ui->stack_text_alignment_y);
}

void ui_text_pad_push(const enum axis_2 axis, const enum alignment_x align)
{
	stack_f32_push(g_ui->stack_text_pad + axis, align);
}

void ui_text_pad_set(const enum axis_2 axis, const enum alignment_x align)
{
	stack_f32_set(g_ui->stack_text_pad + axis, align);
}

void ui_text_pad_pop(const enum axis_2 axis)
{
	stack_f32_pop(g_ui->stack_text_pad + axis);
}

void ui_flags_push(const u64 flags)
{
	const u64 inherited_flags = stack_u64_top(&g_ui->stack_flags);
	stack_u64_push(&g_ui->stack_flags, inherited_flags | flags);
}

void ui_flags_set(const u64 flags)
{
	const u64 inherited_flags = stack_u64_top(&g_ui->stack_flags);
	stack_u64_set(&g_ui->stack_flags, inherited_flags | flags);
}

void ui_flags_pop(void)
{
	stack_u64_pop(&g_ui->stack_flags);
}

void ui_recursive_interaction_push(const u64 flags)
{
	const u64 inherited_flags = stack_u64_top(&g_ui->stack_recursive_interaction_flags);
	stack_u64_push(&g_ui->stack_recursive_interaction_flags, flags | inherited_flags);
}

void ui_recursive_interaction_pop(void)
{
	stack_u64_pop(&g_ui->stack_recursive_interaction_flags);
}

void ui_padding_push(const f32 pad)
{
	stack_f32_push(&g_ui->stack_pad, pad);
}

void ui_padding_set(const f32 pad)
{
	stack_f32_set(&g_ui->stack_pad, pad);
}

void ui_padding_pop(void)
{
	stack_f32_pop(&g_ui->stack_pad);
}

void ui_inter_node_recursive_push(struct ui_inter_node *node)
{
	stack_ptr_push(&g_ui->stack_recursive_interaction, node);
}

void ui_inter_node_recursive_pop(void)
{
	stack_ptr_pop(&g_ui->stack_recursive_interaction);
}

void ui_fixed_depth_push(const u32 depth)
{
	stack_u32_push(&g_ui->stack_fixed_depth, depth);
}

void ui_fixed_depth_set(const u32 depth)
{
	stack_u32_set(&g_ui->stack_fixed_depth, depth);
}

void ui_fixed_depth_pop(void)
{
	stack_u32_pop(&g_ui->stack_fixed_depth);
}

void ui_external_text_push(const utf32 text)
{
	stack_utf32_push(&g_ui->stack_external_text, text);
}

void ui_external_text_set(const utf32 text)
{
	stack_utf32_set(&g_ui->stack_external_text, text);
}

void ui_external_text_pop(void)
{
	stack_utf32_pop(&g_ui->stack_external_text);
}

void ui_external_text_layout_push(struct text_layout *layout, const utf32 text)
{
	stack_ptr_push(&g_ui->stack_external_text_layout, layout);
	stack_utf32_push(&g_ui->stack_external_text, text);
}

void ui_external_text_layout_set(struct text_layout *layout, const utf32 text)
{
	stack_ptr_set(&g_ui->stack_external_text_layout, layout);
	stack_utf32_set(&g_ui->stack_external_text, text);
}

void ui_external_text_layout_pop(void)
{
	stack_ptr_pop(&g_ui->stack_external_text_layout);
}
