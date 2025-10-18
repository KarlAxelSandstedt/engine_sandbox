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
#include "sys_public.h"

struct ui_list ui_list_init(enum axis_2 axis, const f32 axis_pixel_size, const f32 entry_pixel_size)
{
	struct ui_list list = 
	{  
		.axis_pixel_size = axis_pixel_size,
		.entry_pixel_size = entry_pixel_size,
		.axis = axis,
	};

	return list;
}

void ui_list_push(struct ui_list *list, const char *format, ...)
{	
	va_list args;
	va_start(args, format);
	utf8 id = utf8_format_variadic(g_ui->mem_frame, format, args);
	va_end(args);

	struct allocation_slot slot;
	ui_child_layout_axis(list->axis)
	ui_size(list->axis, ui_size_pixel(list->axis_pixel_size, 1.0f))
	ui_recursive_interaction(UI_INTER_DRAG)
	slot = ui_node_alloc(UI_INTER_RECURSIVE_ROOT | UI_INTER_DRAG | UI_DRAW_BACKGROUND | UI_DRAW_BORDER, &id);

	list->cache_count = list->frame_count;
	list->frame_count = 0;
	list->frame_node_address = slot.address;
	list->frame_node = slot.index;

	list->visible.high = f32_min(list->visible.high, list->cache_count*list->entry_pixel_size);
	list->visible.high = f32_max(list->visible.high, list->axis_pixel_size);
	list->visible.low = list->visible.high - list->axis_pixel_size;

	ui_child_layout_axis_push(list->axis);
	ui_intv_viewable_push(list->axis, list->visible);
	ui_node_push(list->frame_node);
}

void ui_list_pop(struct ui_list *list)
{	
	ui_child_layout_axis_pop();
	ui_intv_viewable_pop(list->axis);
	ui_node_pop();

	if (list->frame_node_address->inter_recursive->drag)
	{
		list->visible.low += g_ui->inter.cursor_delta[list->axis];
		list->visible.high += g_ui->inter.cursor_delta[list->axis];
	}
}

struct allocation_slot ui_list_entry_alloc(struct ui_list *list)
{
	const intv intv_entry =
	{
		.low = list->entry_pixel_size * list->frame_count,
		.high = list->entry_pixel_size * (list->frame_count + 1),
	};

	struct allocation_slot entry;
	ui_size(list->axis, ui_size_unit(intv_entry))
	ui_size(1-list->axis, ui_size_perc(1.0f))
	ui_child_layout_axis(1-list->axis)
	entry = ui_node_alloc_f(UI_UNIT_POSITIVE_DOWN | UI_DRAW_BORDER, "###%p_%u", list->frame_node_address, list->frame_count);

	list->frame_count += 1;
	return entry;
}

//U+3bc == 01110 111100
//	=> 1100 1110  1011 1100

u8 buf_ns[] = { 'n', 's', '\0' };
u8 buf_us[] = { 0xce, 0xbc, 's', '\0' };
u8 buf_ms[] = { 'm', 's', '\0' };
u8 buf_s[]  = { 's', '\0' };

const utf8 utf8_ns = { .buf = buf_ns, .len = 2 , .size = 3};
const utf8 utf8_us = { .buf = buf_us, .len = 2 , .size = 4};
const utf8 utf8_ms = { .buf = buf_ms, .len = 2 , .size = 3};
const utf8 utf8_s  = { .buf = buf_s,  .len = 1 , .size = 1};

static void time_unit_config_generate(struct timeline_config *config)
{
	if (config->unit_line_preferred_count == 0 || config->unit_line_preferred_count > (u32) config->width)
	{
		config->unit_line_preferred_count = 10;
	}

	const u64 interval_length = config->ns_interval_end - config->ns_interval_start;
	const u64 preferred_ns_per_unit_line_interval = interval_length / config->unit_line_preferred_count;

	u64 base = 1;
	for (; preferred_ns_per_unit_line_interval / (10 * base); base *= 10);

	/*
	 * Find closest single non-zero digit with trailing 0s closest to the interval size 
	 */
	const u64 low = (preferred_ns_per_unit_line_interval / base) * base;
	const u64 high = low + base;
	const u64 low_diff  = preferred_ns_per_unit_line_interval - low;
	const u64 high_diff = high - preferred_ns_per_unit_line_interval;
	const u64 ns_unit_line_interval = (low_diff <= high_diff) ? low : high;
	
	const u64 ns_unit_line_first = (config->ns_interval_start % ns_unit_line_interval)
		? (1 + config->ns_interval_start / ns_unit_line_interval) * ns_unit_line_interval
		: config->ns_interval_start;

	u64 ns_unit_line_last = (config->ns_interval_end / ns_unit_line_interval) * ns_unit_line_interval;
	ns_unit_line_last = (ns_unit_line_last < ns_unit_line_first) ? ns_unit_line_first : ns_unit_line_last;

	config->unit_line_count = 1 + (ns_unit_line_last - ns_unit_line_first) / ns_unit_line_interval;

	if (ns_unit_line_interval / NSEC_PER_SEC)
	{
		config->unit = utf8_s;
		config->unit_line_first = ns_unit_line_first;
		config->unit_line_interval = ns_unit_line_interval;
		config->unit_to_ns_multiplier = NSEC_PER_SEC;
	}
	else if (ns_unit_line_interval / NSEC_PER_MSEC)
	{
		config->unit = utf8_ms;
		config->unit_line_first = ns_unit_line_first;
		config->unit_line_interval = ns_unit_line_interval;
		config->unit_to_ns_multiplier = NSEC_PER_MSEC;
	}
	else if (ns_unit_line_interval / NSEC_PER_USEC)
	{
		config->unit = utf8_us;
		config->unit_line_first = ns_unit_line_first;
		config->unit_line_interval = ns_unit_line_interval;
		config->unit_to_ns_multiplier = NSEC_PER_USEC;
	}
	else
	{
		config->unit = utf8_ns;
		config->unit_line_first = ns_unit_line_first;
		config->unit_line_interval = ns_unit_line_interval;
		config->unit_to_ns_multiplier = 1;
	}
}

void ui_timeline(struct timeline_config *config)
{
	ui_child_layout_axis(AXIS_2_Y)
	ui_parent(ui_node_alloc_non_hashed(UI_DRAW_BORDER).index)
	ui_background_color(config->background_color)
	ui_sprite_color(config->text_color)
	ui_intv_viewable_x(intv_inline(config->ns_interval_start, config->ns_interval_end))
	{
		ui_height(ui_size_childsum(0.0f))
		config->timeline = ui_node_alloc_f(UI_DRAW_BACKGROUND, "timeline_rows_%p", config).index;
		
		const struct ui_node *timeline_node = hierarchy_index_address(g_ui->node_hierarchy, config->timeline);
		config->width = timeline_node->layout_size[0];
		const f32 half_pixel_count = (2.0f * config->width * (1.0f - config->perc_width_row_title_column));
		config->ns_half_pixel = (f32) (config->ns_interval_end - config->ns_interval_start) / half_pixel_count;

		time_unit_config_generate(config);

		ui_parent(config->timeline)
		ui_child_layout_axis(AXIS_2_X)
		ui_background_color(config->unit_line_color)
		ui_height(ui_size_perc(1.0f))
		ui_width(ui_size_perc(1.0f))
		ui_flags(UI_SKIP_HOVER_SEARCH)
		ui_parent(ui_node_alloc_non_hashed(UI_FLAG_NONE).index)
		{
			ui_pad_perc(config->perc_width_row_title_column);	

			ui_width(ui_size_perc(1.0f - config->perc_width_row_title_column))
			ui_parent(ui_node_alloc_non_hashed(UI_FLAG_NONE).index)
			{
				for (u64 i = 0; i < config->unit_line_count; ++i)
				{
					const u64 ns_line = config->unit_line_first + i*config->unit_line_interval;
					ui_width(ui_size_unit(intv_inline(ns_line - config->unit_line_width * config->ns_half_pixel, ns_line + config->unit_line_width * config->ns_half_pixel)))
					ui_node_alloc_non_hashed(UI_DRAW_BACKGROUND);
				}

				ui_background_color(config->subline_color)
				if (config->draw_sublines)
				{
					for (u64 i = 0; i <= config->unit_line_count; ++i)
					{
						const u64 ns_line = config->unit_line_first +  (i-1)*config->unit_line_interval;
						for (u64 j = 1; j <= config->sublines_per_line; ++j)
						{
							const u64 ns_subline = ns_line + j * config->unit_line_interval / config->sublines_per_line;
							ui_width(ui_size_unit(intv_inline(ns_subline - config->subline_width * config->ns_half_pixel, ns_subline + config->subline_width * config->ns_half_pixel)))
							ui_node_alloc_non_hashed(UI_DRAW_BACKGROUND);
						}
					}
				}
			}
		}

		ui_child_layout_axis(AXIS_2_X)
		ui_height(ui_size_pixel(32.0f, 1.0f))
		ui_parent(ui_node_alloc_non_hashed(UI_FLAG_NONE).index)
		{
			ui_pad_perc(config->perc_width_row_title_column);	

			ui_width(ui_size_perc(1.0f - config->perc_width_row_title_column))
			ui_parent(ui_node_alloc_f(UI_DRAW_BACKGROUND | UI_DRAW_BORDER | UI_DRAW_ROUNDED_CORNERS, "timeline_units_bar_%p", config).index)
			{
				ui_background_color(config->unit_line_color)
				ui_height(ui_size_perc(1.0f))
				for (u64 i = 0; i < config->unit_line_count; ++i)
				{
					const u64 ns_line = config->unit_line_first + i*config->unit_line_interval;
					ui_child_layout_axis(AXIS_2_Y)
					ui_width(ui_size_unit(intv_inline(ns_line - config->ns_half_pixel, ns_line + config->ns_half_pixel)))
					ui_parent(ui_node_alloc_non_hashed(UI_FLAG_NONE).index)
					ui_width(ui_size_perc(1.0f))
					ui_height(ui_size_perc(0.25f))
					ui_node_alloc_non_hashed(UI_DRAW_BACKGROUND);
					
					ui_width(ui_size_unit(intv_inline(
							  ns_line - config->unit_line_interval / 2
							, ns_line + config->unit_line_interval / 2)))
					ui_node_alloc_f(UI_DRAW_TEXT, "%lu%k##%p", ns_line / config->unit_to_ns_multiplier, &config->unit, config);
				}
			}
		}
	}
}

void ui_timeline_row_push(struct timeline_config *config, const u32 row, const char *title_format, ...)
{
	const struct timeline_row_config *row_config = config->row + row;
	config->row_pushed = row;

	ui_flags_push(UI_TEXT_ALLOW_OVERFLOW | UI_UNIT_POSITIVE_DOWN);
	ui_gradient_color_push(BOX_CORNER_BR, config->task_gradient_br);
	ui_gradient_color_push(BOX_CORNER_TR, config->task_gradient_tr);
	ui_gradient_color_push(BOX_CORNER_TL, config->task_gradient_tl);
	ui_gradient_color_push(BOX_CORNER_BL, config->task_gradient_bl);
	ui_intv_viewable_push(AXIS_2_X, intv_inline(config->ns_interval_start, config->ns_interval_end));
	ui_intv_viewable_push(AXIS_2_Y, row_config->depth_visible);
	ui_child_layout_axis_push(AXIS_2_X);
	ui_node_push(config->timeline);
	ui_text_align_x_push(ALIGN_LEFT);

	va_list args;
	va_start(args, title_format);
	utf8 title = utf8_format_variadic(g_ui->mem_frame, title_format, args);
	va_end(args);
	
	ui_width(ui_size_perc(1.0f))
	ui_height(ui_size_pixel(row_config->height, 1.0f))
	ui_node_push(ui_node_alloc_f(UI_DRAW_BORDER | UI_DRAW_ROUNDED_CORNERS, "%k##title", &title).index);

	ui_text_align_y(ALIGN_TOP)
	ui_text_pad_x(8.0f)
	ui_width(ui_size_perc(config->perc_width_row_title_column))
	ui_height(ui_size_perc(1.0f))
	ui_node_alloc_f(UI_DRAW_TEXT, "%k", &title);

	ui_recursive_interaction(UI_INTER_DRAG)
	ui_width(ui_size_perc(1.0f-config->perc_width_row_title_column))
	ui_height(ui_size_perc(1.0f))
	ui_background_color(config->background_color)
	ui_node_push(ui_node_alloc_f(UI_INTER_RECURSIVE_ROOT | UI_DRAW_BORDER | UI_INTER_DRAG, "%k##task_bar", &title).index);
}

void timeline_drag(void)
{
	struct timeline_config *config = g_queue->cmd_exec->arg[0].ptr;
	const i64 drag_delta_x = g_queue->cmd_exec->arg[1].i64;
	const i64 drag_delta_y = g_queue->cmd_exec->arg[2].i64;
	const u64 ctrl_pressed = g_queue->cmd_exec->arg[3].u64;

	i64 offset = -(i64) (drag_delta_x * 2.0f * config->ns_half_pixel);
	if (offset < -(i64) config->ns_interval_start)
	{
		offset = -(i64) config->ns_interval_start;
	}
	config->ns_interval_start = (u64) ((i64) config->ns_interval_start + offset);
	config->ns_interval_end = (u64) ((i64) config->ns_interval_end + offset);
	config->fixed = 1;

	if (ctrl_pressed)
	{
		const i64 ns_interval = (i64) (config->ns_interval_end - config->ns_interval_start);
		const f64 ns_drag_half = (f64) ns_interval / 500;

		/* upward motion => zoom in, downward motion => zoon out */
		const f64 ns_interval_start = config->ns_interval_start - ns_drag_half*drag_delta_y;

		config->ns_interval_start = (0 < ns_interval_start)
			? (u64) ns_interval_start
			: 0;
		config->ns_interval_end = config->ns_interval_start + ns_interval + 2*ns_drag_half*drag_delta_y;
	}
}

void ui_timeline_row_pop(struct timeline_config *config)
{
	struct timeline_row_config *row_config = config->row + config->row_pushed;
	if (ui_node_top()->inter_recursive->drag)
	{
		if (!g_ui->inter.key_pressed[KAS_CTRL])
		{
			const f32 depth_offset = f32_max(-row_config->depth_visible.low, g_ui->inter.cursor_delta[1] / config->task_height);
			row_config->depth_visible.low += depth_offset;
			row_config->depth_visible.high += depth_offset;
		}

		cmd_submit_f(g_ui->mem_frame, "timeline_drag %p %li %li %u", config, (i64) g_ui->inter.cursor_delta[0], (i64) g_ui->inter.cursor_delta[1], g_ui->inter.key_pressed[KAS_CTRL]);
	}
	
	ui_text_align_x_pop();
	ui_flags_pop();
	ui_gradient_color_pop(BOX_CORNER_BR);
	ui_gradient_color_pop(BOX_CORNER_TR);
	ui_gradient_color_pop(BOX_CORNER_TL);
	ui_gradient_color_pop(BOX_CORNER_BL);
	ui_intv_viewable_pop(AXIS_2_X);
	ui_child_layout_axis_pop();
	ui_node_pop();
	ui_node_pop();

	ui_child_layout_axis(AXIS_2_X)
	ui_height(ui_size_pixel(10.0f, 1.0f))
	ui_parent(ui_node_alloc_non_hashed(UI_FLAG_NONE).index)
	ui_height(ui_size_perc(1.0f))
	{
		ui_pad_perc(config->perc_width_row_title_column);	

		const struct ui_node *drag_node;
		ui_background_color(config->draggable_color)
		ui_width(ui_size_perc(1.0f - config->perc_width_row_title_column))
		drag_node = ui_node_alloc_f(UI_DRAW_BACKGROUND | UI_DRAW_BORDER | UI_DRAW_ROUNDED_CORNERS | UI_INTER_DRAG, "drag_area_%u", config->row_pushed).address;
		if (drag_node->inter_local->drag)
		{
			row_config->height -= (g_ui->inter.cursor_delta[1] <= row_config->height)
				? g_ui->inter.cursor_delta[1]
				: row_config->height;
			row_config->depth_visible.high = row_config->depth_visible.low + row_config->height / config->task_height;
			fprintf(stderr, "height: %f\n", row_config->height);
			fprintf(stderr, "depth_visible: %f, %f\n", row_config->depth_visible.low, row_config->depth_visible.high);
		}
	}

	ui_node_pop();
}

struct ui_inter_node *ui_button_f(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	utf8 id = utf8_format_variadic(g_ui->mem_frame, fmt, args);
	va_end(args);

	struct ui_node *button = ui_node_alloc(UI_INTER_LEFT_CLICK | UI_DRAW_BORDER | UI_DRAW_BACKGROUND | UI_DRAW_GRADIENT | UI_DRAW_ROUNDED_CORNERS | UI_DRAW_TEXT, &id).address;
	return button->inter_local;
}

void ui_text_input_mode_enable(void)
{
	const utf8 id = g_queue->cmd_exec->arg[0].utf8;
	struct ui_input_line *line = g_queue->cmd_exec->arg[1].ptr;

	system_window_text_input_mode_enable();

	g_ui->inter.keyboard_text_input = 1;
	g_ui->inter.text_edit.id = id;
	g_ui->inter.text_edit.text = &line->text;
	g_ui->inter.text_edit.cursor = 0;
	g_ui->inter.text_edit.mark = 0;
}

void ui_text_edit_clear(void)
{
	const utf8 id = g_queue->cmd_exec->arg[0].utf8;
	if (utf8_equivalence(g_ui->inter.text_edit.id, id))
	{
		g_ui->inter.text_edit.text->len = 0;
		g_ui->inter.text_edit.cursor = 0;
		g_ui->inter.text_edit.mark = 0;
	}
}

static utf32 text_stub = { 0 };

void ui_text_input_mode_disable(void)
{
	system_window_text_input_mode_disable();

	struct ui_node *node = ui_node_lookup(&g_ui->inter.text_edit.id);
	if (node)
	{
		node->inter_local->active = 0;
	}

	g_ui->inter.keyboard_text_input = 0;
	g_ui->inter.text_edit.id = utf8_empty();
	g_ui->inter.text_edit.text = &text_stub;
	g_ui->inter.text_edit.cursor = 0;
	g_ui->inter.text_edit.mark = 0;
}

void ui_text_op(void)
{
	const enum kas_keycode key = g_queue->cmd_exec->arg[0].u32;
	const u32 mod = g_queue->cmd_exec->arg[1].u32;
	const utf8 replace = g_queue->cmd_exec->arg[2].utf8;

	u32 key_ctrl = mod & KEY_MOD_CTRL;
	u32 key_shift = mod & KEY_MOD_SHIFT;
	utf32 *edit = g_ui->inter.text_edit.text;

	/* start constructing text operation */ 
	struct text_op op =
	{
		.str_copy = utf32_empty(),
		.str_replace = utf32_empty(),
		.cursor_new = g_ui->inter.text_edit.cursor,
		.mark_new = g_ui->inter.text_edit.mark,
	};

	if (g_ui->inter.text_edit.mark <= g_ui->inter.text_edit.cursor)
	{
		op.low = g_ui->inter.text_edit.mark;
		op.high = g_ui->inter.text_edit.cursor;
	}
	else
	{
		op.low = g_ui->inter.text_edit.cursor;
		op.high = g_ui->inter.text_edit.mark;
	}

	/* ordinary text input  */ 
	if (replace.len)
	{
		const u64 len_left = edit->max_len - edit->len;
		op.str_replace = utf32_utf8(g_ui->mem_frame, replace);
		if (len_left < op.str_replace.len)
		{
			op.str_replace.len = len_left;
		}
		
		op.cursor_new = op.low + op.str_replace.len;
		op.mark_new = op.cursor_new;
	}
	else
	{
		switch (key)
		{
			case KAS_LEFT:
			{
				if (op.cursor_new)
				{
					if (key_ctrl)
					{
						for (; op.cursor_new && is_wordbreak(edit->buf[op.cursor_new-1]); op.cursor_new -= 1);
						for (; op.cursor_new && !is_wordbreak(edit->buf[op.cursor_new-1]); op.cursor_new -= 1);
					}
					else
					{
						op.cursor_new -= 1;
					}
				}

				op.low = 0;	
				op.high = 0;	
				if (!key_shift)
				{
					op.mark_new = op.cursor_new;
				}
			} break;

			case KAS_RIGHT:
			{
				if (op.cursor_new < edit->len)
				{
					if (key_ctrl)
					{
						for (; op.cursor_new < edit->len && !is_wordbreak(edit->buf[op.cursor_new]); op.cursor_new += 1);
						for (; op.cursor_new < edit->len && is_wordbreak(edit->buf[op.cursor_new]); op.cursor_new += 1);
					}
					else
					{
						op.cursor_new += 1;
					}
				}

				op.low = 0;
				op.high = 0;
				if (!key_shift)
				{
					op.mark_new = op.cursor_new;
				}

			} break;

			case KAS_BACKSPACE:
			{
				if (op.low == op.high)
				{
					if (key_ctrl)
					{
						/* backtrack any leading wordbreakers, until we reach the end of a word */
						for (; op.low && is_wordbreak(edit->buf[op.low-1]); op.low -= 1);
						for (; op.low && !is_wordbreak(edit->buf[op.low-1]); op.low -= 1);
					}
					else
					{
						if (op.low)
						{
							op.low -= 1;
						}
					}
				}

				op.cursor_new = op.low;
				op.mark_new = op.low;
			} break;

			case KAS_DELETE:
			{
				if (op.low == op.high)
				{
					if (key_ctrl)
					{
						for (; op.high < edit->len && !is_wordbreak(edit->buf[op.high]); op.high += 1);
						for (; op.high < edit->len && is_wordbreak(edit->buf[op.high]); op.high += 1);
					}
					else
					{
						if (op.high < edit->len)
						{
							op.high += 1;
						}
					}
				}

				op.cursor_new = op.low;
				op.mark_new = op.low;
			} break;

			case KAS_HOME:
			{
				op.cursor_new = 0;
				op.low = 0;
				op.high = 0;
				if (!key_shift)
				{
					op.mark_new = 0;
				}
			} break;

			case KAS_END:
			{
				op.cursor_new = edit->len;
				op.low = 0;
				op.high = 0;
				if (!key_shift)
				{
					op.mark_new = edit->len;
				}
			} break;

			case KAS_C:
			{
				op.str_copy = (utf32) { .buf = edit->buf + op.low, .len = op.high - op.low, .max_len = op.high - op.low };
				op.low = 0;
				op.high = 0;
			} break;

			case KAS_X:
			{
				op.str_copy = (utf32) { .buf = edit->buf + op.low, .len = op.high - op.low, .max_len = op.high - op.low };
				op.cursor_new = op.low;
				op.mark_new = op.low;
			} break;

			default:
			{

			} break;
		}
	}

	/* do any potential clipboard setting before we potentially begin editting the string */
	if (op.str_copy.len)
	{
		cstr_set_clipboard((const char *) utf8_utf32_null_terminated(g_ui->mem_frame, op.str_copy).buf);
	}

	/* apply text operation */
	const u32 intv_len = op.high - op.low;
	u64 new_len = edit->len;

	/* downward shift text above selected range to tightly pack with replacement string */
	if (op.str_replace.len < intv_len)
	{
		const u32 shift = edit->len - op.high;
		const u32 diff = intv_len - op.str_replace.len;
		new_len -= diff;
		for (u32 i = 0; i < shift; ++i)
		{
			edit->buf[op.low + op.str_replace.len + i] = edit->buf[op.high+i];
		}
	}
	/* up shift text above selected range to make room for replacement string  */
	else if (op.str_replace.len > intv_len)
	{
		const u32 count = edit->len - op.high;
		const u32 shift = op.str_replace.len - intv_len;
		new_len += shift;
		for (u32 i = 1; i <= count; ++i)
		{
			edit->buf[new_len - i] = edit->buf[new_len - i - shift];
		}
	}

	for (u32 i = 0; i < op.str_replace.len; ++i)
	{
		edit->buf[op.low + i] = op.str_replace.buf[i];
	}

	edit->len = new_len;
	g_ui->inter.text_edit.cursor = op.cursor_new;
	g_ui->inter.text_edit.mark = op.mark_new;
}

struct ui_node *ui_input_line(const utf32 external_text, const utf8 id)
{
	struct ui_node *line;

	ui_external_text(external_text)
	line = ui_node_alloc(UI_INTER_LEFT_CLICK | UI_DRAW_TEXT | UI_TEXT_ALLOW_OVERFLOW | UI_TEXT_EXTERNAL, &id).address;
	if (line->inter_local->active)
	{
		line->background_color[0] += 0.03125f;
		line->background_color[1] += 0.03125f;
		line->background_color[2] += 0.03125f;
		line->border_color[0] += 0.25f;
		line->border_color[1] += 0.25f;
		line->border_color[2] += 0.25f;
	}

	return line;
}

struct ui_node *ui_input_line_f(const utf32 external_text, const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	utf8 id = utf8_format_variadic(g_ui->mem_frame, fmt, args);
	va_end(args);

	return ui_input_line(external_text, id);
}

void ui_cmd_console(struct cmd_console *console, const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	utf8 id = utf8_format_variadic(g_ui->mem_frame, fmt, args);
	va_end(args);

	struct ui_node *line;
	ui_flags(UI_DRAW_BACKGROUND | UI_DRAW_BORDER | UI_DRAW_ROUNDED_CORNERS)
	line = ui_input_line(console->prompt.text, id);

	if (line->inter_local->clicked)
	{
		cmd_submit_f(g_ui->mem_frame, "ui_text_input_mode_enable \"%k\" %p", &line->id, &console->prompt);
	}

	if (line->inter_local->active && line->inter_local->key_clicked[KAS_ENTER])
	{
		cmd_submit_utf8(utf8_utf32(g_ui->mem_frame, console->prompt.text));
		cmd_submit_f(g_ui->mem_frame, "ui_text_edit_clear \"%k\"", &line->id);
	}
}

void ui_popup_build(void)
{
	const u32 parent = g_window;
	struct ui_popup *popup = g_queue->cmd_exec->arg[0].ptr;
	struct ui_visual *visual = g_queue->cmd_exec->arg[1].ptr;

	if (popup->window == HI_NULL_INDEX)
	{
		*popup = ui_popup_null();
		return;
	}

	struct system_window *win = system_window_address(popup->window);
	if (win->tagged_for_destruction || popup->state == UI_POPUP_STATE_COMPLETED)
	{
		win->tagged_for_destruction = 1;
		*popup = ui_popup_null();
		return;
	}

	/* set for the duration of this function, window, ui, cmd globals */
	system_window_set_global(popup->window);
	cmd_queue_execute();

	ui_frame_begin(win->size, visual);
	ui_text_align_x(ALIGN_X_CENTER)
	ui_text_align_y(ALIGN_Y_CENTER)
	ui_parent(ui_node_alloc_f(UI_DRAW_BACKGROUND | UI_DRAW_BORDER, "###popup_%u", popup->window).index)
	{
		if (popup->type == UI_POPUP_UTF8_DISPLAY)
		{
			ui_node_alloc_f(UI_DRAW_TEXT, "%k###popup_display_%u", &popup->display1, popup->window);
		}
		else if (popup->type == UI_POPUP_UTF8_INPUT)
		{
			ui_child_layout_axis(AXIS_2_Y)
			ui_parent(ui_node_alloc_non_hashed(UI_FLAG_NONE).index)
			{
				ui_height(ui_size_pixel(96.0f, 0.0f))
				ui_node_alloc_f(UI_DRAW_TEXT, "%k###popup_display1_%u", &popup->display1, popup->window);

				ui_child_layout_axis(AXIS_2_X)
				ui_height(ui_size_pixel(32.0f, 1.0f))
				ui_parent(ui_node_alloc_non_hashed(UI_FLAG_NONE).index)
				{
					ui_width(ui_size_text(F32_INFINITY, 1.0f))
					ui_node_alloc_f(UI_DRAW_TEXT, "%k###popup_display2_%u", &popup->display2, popup->window);
					struct ui_node *line;
					ui_flags(UI_DRAW_BORDER | UI_DRAW_ROUNDED_CORNERS)
					ui_width(ui_size_perc(1.0f))
					ui_text_align_x(ALIGN_LEFT)
					line = ui_input_line_f(popup->prompt->text, "###popup_input_%u", popup->window);
						
					if (line->inter_local->clicked)
					{
						cmd_submit_f(g_ui->mem_frame, "ui_text_input_mode_enable \"%k\" %p", &line->id, popup->prompt);
					}

					if (line->inter_local->active && line->inter_local->key_clicked[KAS_ENTER] && popup->state != UI_POPUP_STATE_PENDING_VERIFICATION)
					{
						cmd_submit_f(g_ui->mem_frame, "ui_text_input_mode_disable");
						*popup->input = utf8_utf32_buffered(popup->input->buf, popup->input->size, popup->prompt->text);
						popup->state = UI_POPUP_STATE_PENDING_VERIFICATION;
					}

					ui_pad();
				}
			}
		}
		else if (popup->type == UI_POPUP_CHOICE)
		{
			ui_child_layout_axis(AXIS_2_Y)
			ui_parent(ui_node_alloc_non_hashed(UI_FLAG_NONE).index)
			{
				ui_height(ui_size_pixel(96.0f, 0.0f))
				ui_node_alloc_f(UI_DRAW_TEXT, "%k###popup_display1_%u", &popup->display1);

				ui_child_layout_axis(AXIS_2_X)
				ui_height(ui_size_pixel(48.0f, 1.0f))
				ui_parent(ui_node_alloc_non_hashed(UI_FLAG_NONE).index)
				{
					ui_pad_fill();

					ui_width(ui_size_pixel(128.0f, 1.0f))
					if (ui_button_f("%k###popup_display2_%u", &popup->display2, popup->window)->clicked)
					{
						if (popup->state != UI_POPUP_STATE_PENDING_VERIFICATION)
						{
							popup->positive = 1;
							popup->negative = 0;
							popup->state = UI_POPUP_STATE_PENDING_VERIFICATION;
						}
					}

					ui_pad_fill();

					ui_width(ui_size_pixel(128.0f, 1.0f))
					if (ui_button_f("%k###popup_display3_%u", &popup->display3, popup->window)->clicked)
					{
						if (popup->state != UI_POPUP_STATE_PENDING_VERIFICATION)
						{
							popup->positive = 0;
							popup->negative = 1;
							popup->state = UI_POPUP_STATE_PENDING_VERIFICATION;
						}
					}
					
					ui_pad_fill();
				}
			}
		}
	}
	ui_frame_end();

	system_window_set_global(parent);
	g_queue->regs[0].ptr = popup;
	g_queue->regs[1].ptr = visual;
	cmd_submit_next_frame(cmd_ui_popup_build);
}


struct ui_popup ui_popup_null(void)
{
	return (struct ui_popup) { .window = HI_NULL_INDEX, .state = UI_POPUP_STATE_NULL };
}

void ui_popup_try_destroy_and_set_to_null(struct ui_popup *popup)
{
	if (popup->window != HI_NULL_INDEX)
	{
		struct system_window *win = system_window_address(popup->window);
		win->tagged_for_destruction = 1;
	}
	*popup = ui_popup_null();
}

void ui_popup_utf8_display(struct ui_popup *popup, const utf8 display, const char *title, const struct ui_visual *visual)
{
	if (popup->state == UI_POPUP_STATE_NULL)
	{
		popup->window = system_window_alloc(title, vec2u32_inline(0,0), vec2u32_inline(600, 200), g_window);
		if (popup->window != HI_NULL_INDEX)
		{
			struct system_window *win = system_window_address(popup->window);
			popup->display1 = utf8_copy(&win->mem_persistent, display);
			popup->type = UI_POPUP_UTF8_DISPLAY;
			popup->state = UI_POPUP_STATE_RUNNING,

			g_queue->regs[0].ptr = popup;
			g_queue->regs[1].ptr = (void *) visual;
			cmd_submit(cmd_ui_popup_build);
		}
	}
}

void ui_popup_utf8_input(struct ui_popup *popup, utf8 *input, struct ui_input_line *line, const utf8 description, const utf8 prefix, const char *title, const struct ui_visual *visual)
{
	if (popup->state == UI_POPUP_STATE_NULL)
	{
		popup->window = system_window_alloc(title, vec2u32_inline(0,0), vec2u32_inline(600, 200), g_window);
		if (popup->window != HI_NULL_INDEX)
		{
			struct system_window *win = system_window_address(popup->window);
			popup->display1 = utf8_copy(&win->mem_persistent, description);
			popup->display2 = utf8_copy(&win->mem_persistent, prefix);
			popup->type = UI_POPUP_UTF8_INPUT;
			popup->state = UI_POPUP_STATE_RUNNING;
			popup->prompt = line;
			popup->input = input;

			g_queue->regs[0].ptr = popup;
			g_queue->regs[1].ptr = (void *) visual;
			cmd_submit(cmd_ui_popup_build);
		}
	}
}

void ui_popup_choice(struct ui_popup *popup, const utf8 description, const utf8 positive, const utf8 negative, const char *title, const struct ui_visual *visual)
{
	if (popup->state == UI_POPUP_STATE_NULL)
	{
		popup->window = system_window_alloc(title, vec2u32_inline(0,0), vec2u32_inline(600, 200), g_window);
		if (popup->window != HI_NULL_INDEX)
		{
			struct system_window *win = system_window_address(popup->window);

			popup->display1 = utf8_copy(&win->mem_persistent, description);
			popup->display2 = utf8_copy(&win->mem_persistent, positive);
			popup->display3 = utf8_copy(&win->mem_persistent, negative);
			popup->type = UI_POPUP_CHOICE;
			popup->state = UI_POPUP_STATE_RUNNING;
			popup->yes = 0;
			popup->no = 0;

			g_queue->regs[0].ptr = popup;
			g_queue->regs[1].ptr = (void *) visual;
			cmd_submit(cmd_ui_popup_build);
		}
	}
}

struct ui_input_line ui_input_line_empty(void)
{
	return (struct ui_input_line) { .cursor = 0, .mark = 0, .text = utf32_empty() };
}

struct ui_input_line ui_input_line_alloc(struct arena *mem, const u32 max_len)
{
	utf32 text = utf32_alloc(mem, max_len);
	return (text.max_len)
		? (struct ui_input_line) { .cursor = 0, .mark = 0, .text = text }
		: ui_input_line_empty();
}
