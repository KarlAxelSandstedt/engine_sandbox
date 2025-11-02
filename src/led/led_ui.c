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

static void led_project_menu_ui(struct led *led, const struct ui_visual *visual)
{
	struct led_project_menu *menu = &led->project_menu;

	system_window_set_global(menu->window);
	cmd_queue_execute();

	struct system_window *win = system_window_address(menu->window);
	ui_frame_begin(win->size, visual);

	ui_text_align_x(ALIGN_LEFT)
	ui_child_layout_axis(AXIS_2_Y)
	ui_parent(ui_node_alloc_f(UI_DRAW_BACKGROUND | UI_DRAW_BORDER, "###window_%u", menu->window).index)
	ui_flags(UI_DRAW_ROUNDED_CORNERS | UI_TEXT_ALLOW_OVERFLOW)
	ui_child_layout_axis(AXIS_2_X)
	ui_height(ui_size_pixel(32.0f, 1.0f))
	{
		ui_pad();

		ui_parent(ui_node_alloc_f(UI_FLAG_NONE, "###cur_fld_row").index)
		ui_width(ui_size_pixel(20.0f, 1.0f))
		{
			ui_sprite(SPRITE_LED_FOLDER)
			ui_node_alloc_f(UI_DRAW_SPRITE, "###cur_fld_spr");

			ui_pad();

			ui_width(ui_size_text(F32_INFINITY, 0.0f))
			ui_node_alloc_f(UI_DRAW_TEXT | UI_DRAW_BORDER, "%k###cur_fld_path", &led->root_folder.path);

			ui_pad_fill();
			ui_pad();

			ui_width(ui_size_text(F32_INFINITY, 1.0f))
			if (ui_button_f(UI_DRAW_TEXT | UI_DRAW_BORDER | UI_DRAW_BACKGROUND | UI_DRAW_ROUNDED_CORNERS, "Refresh###ref") & UI_INTER_LEFT_CLICK)
			{
				menu->projects_folder_refresh = 1;
			}

			ui_pad();
		}

		ui_pad();

		const u32 file_count = menu->dir_nav.files.next;
		ui_height(ui_size_pixel(20.0f, 1.0f))
		ui_list(&menu->dir_list, "###p", &menu->dir_list)
		for (u32 f = 0; f < file_count; ++f)
		{
			const struct file *file = vector_address(&menu->dir_nav.files, f);
			const enum sprite_id spr = (file->type == FILE_DIRECTORY)
				? SPRITE_LED_FOLDER
				: SPRITE_LED_FILE;

			struct slot entry = ui_list_entry_alloc_f(&menu->dir_list, "###%p_%u", &menu->dir_list, f);
			if (entry.address)
			ui_parent(entry.index)
			{
				ui_pad();

				ui_sprite(spr)
				ui_width(ui_size_pixel(20.0f, 1.0f))
				ui_node_alloc_non_hashed(UI_DRAW_BORDER | UI_DRAW_SPRITE);

				ui_pad();

				ui_width(ui_size_text(F32_INFINITY, 1.0f))
				ui_node_alloc_f(UI_DRAW_TEXT, "%k##%u", &file->path, f);
			}
		}
		ui_pad();

		ui_parent(ui_node_alloc_f(UI_FLAG_NONE, "search_bar_row").index)
		{
			ui_width(ui_size_text(F32_INFINITY, 1.0f))
			ui_node_alloc_f(UI_DRAW_TEXT, "search:##bar");
			
			const utf8 tmp = utf8_inline("Text Window (TODO)");
			ui_width(ui_size_text(F32_INFINITY, 0.0f))
			ui_node_alloc_f(UI_DRAW_TEXT | UI_DRAW_BORDER, "%k##_bar", &tmp);
		}
		
		ui_pad();

		ui_parent(ui_node_alloc_f(UI_FLAG_NONE, "buttons_row").index)
		ui_width(ui_size_text(F32_INFINITY, 0.0f))
		{
			ui_pad();

			if ((ui_button_f(UI_DRAW_TEXT | UI_DRAW_BORDER | UI_DRAW_BACKGROUND | UI_DRAW_ROUNDED_CORNERS, "New Project") & UI_INTER_LEFT_CLICK) && menu->popup_new_project.state == UI_POPUP_STATE_NULL)
			{
				ui_popup_utf8_input(&menu->popup_new_project, &menu->utf8_new_project, &menu->input_line_new_project, utf8_inline("Please enter the new project's name"), utf8_inline("New Project:"), "New Project", visual);
			} 
			else if (menu->popup_new_project.state == UI_POPUP_STATE_PENDING_VERIFICATION)
			{
				ui_popup_try_destroy_and_set_to_null(&menu->popup_new_project_extra);
				menu->popup_new_project.state = UI_POPUP_STATE_COMPLETED;

				const char *error_string = NULL;

				if (menu->utf8_new_project.len == 0 || !led_filename_valid(menu->utf8_new_project))
				{
					error_string = "Invalid project name!";
				}
				else
				{
					struct system_window *project_window = system_window_address(led->window);
					enum fs_error err;

					const char *cstr_project_name = cstr_utf8(g_ui->mem_frame, menu->utf8_new_project);
					err = directory_try_create(&project_window->mem_persistent, &led->project.folder, cstr_project_name, &led->root_folder);
					if (err != FS_SUCCESS)
					{
						switch (err)
						{
							case FS_ALREADY_EXISTS: { error_string = "Project already exists!"; } break;
							default: { error_string = "Unexpected error in creating project folder!"; } break;
						}
					}
					else
					{
						err = file_try_create(&project_window->mem_persistent, &led->project.file, cstr_project_name, &led->project.folder, !FILE_TRUNCATE);
						if (err != FS_SUCCESS)
						{
							switch (err)
							{
								default: { error_string = "Unexpected error in creating main project file!"; } break;
							}
						}
					}
				}
				
				if (error_string)
				{
					ui_popup_utf8_display(&menu->popup_new_project_extra, utf8_cstr(g_ui->mem_frame, error_string), "Error Message", visual);
					menu->popup_new_project.state = UI_POPUP_STATE_RUNNING;
				}
			}

			ui_pad();

			if (ui_button_f(UI_DRAW_TEXT | UI_DRAW_BORDER | UI_DRAW_BACKGROUND | UI_DRAW_ROUNDED_CORNERS, "Load") & UI_INTER_LEFT_CLICK)
			{
				fprintf(stderr, "Load!\n");
			}

			ui_pad();

			if (ui_button_f(UI_DRAW_TEXT | UI_DRAW_BORDER | UI_DRAW_BACKGROUND | UI_DRAW_ROUNDED_CORNERS, "Delete") & UI_INTER_LEFT_CLICK)
			{
				fprintf(stderr, "Delete!\n");
			}
		}
	}

	ui_frame_end();
}

static void led_profiler_ui(struct led *led, const struct ui_visual *visual)
{
	if (g_profiler->level < PROFILE_LEVEL_TASK)
	{
		return;
	}
	struct led_profiler *prof = &led->profiler;
	system_window_set_global(prof->window);
	cmd_queue_execute();

	if (g_profiler->frame_counter == 0)
	{
		kaspf_reader_stream(prof->timeline_config.ns_interval_size);
		return;
	}

	struct system_window *win = system_window_address(prof->window);
	ui_frame_begin(win->size, visual);

	if (prof->timeline_config.fixed)
	{
		kaspf_reader_fixed(prof->timeline_config.ns_interval_start, prof->timeline_config.ns_interval_end);
	}
	else
	{
		kaspf_reader_stream(prof->timeline_config.ns_interval_size);
		prof->timeline_config.ns_interval_start = g_kaspf_reader->ns_start;
		prof->timeline_config.ns_interval_end = g_kaspf_reader->ns_end;
		prof->timeline_config.ns_interval_size = g_kaspf_reader->ns_end - g_kaspf_reader->ns_start;
	}

	ui_child_layout_axis(AXIS_2_Y)
	ui_width(ui_size_perc(1.0f))
	ui_height(ui_size_perc(1.0f))
	ui_flags(UI_TEXT_ALLOW_OVERFLOW)
	ui_parent(ui_node_alloc_f(UI_DRAW_BACKGROUND | UI_DRAW_BORDER, "###window_%u", prof->window).index)
	{
		ui_height(ui_size_perc(0.1f))
		ui_width(ui_size_perc(1.0f))
		ui_font(FONT_DEFAULT_MEDIUM)
		ui_node_alloc_f(UI_DRAW_TEXT, "PROFILER", win->ui);

		ui_height(ui_size_pixel(600.0f, 1.0f))
		ui_width(ui_size_perc(0.9f))
		ui_timeline(&prof->timeline_config);
		for (u32 wi = 0; wi < g_task_ctx->worker_count; ++wi)
		{
			const char *title_format = (wi)
				? "Worker_%u"
				: "Master###%u";
			ui_timeline_row(&prof->timeline_config, wi, title_format, wi)
			{
				struct hw_frame_header *fh = g_kaspf_reader->low;
				for (u64 fi = g_kaspf_reader->frame_low; fi <= g_kaspf_reader->frame_high; ++fi) 
				{
					struct hw_profile_header *hw_h = fh->hw_profile_h + wi;
					for (u64 pi = 0; pi < hw_h->profile_count; ++pi)
					{
						struct hw_profile *p  = hw_h->profiles + pi;
						const vec4 prof_text = { 0.0f, 0.0f, 0.0f, 0.8f };
						const vec4 border_blend_color = { 0.0f, 0.0f, 0.0f, 1.0f };
						vec4 border_color;

						struct kaspf_task_info *info = g_kaspf_reader->task_info + p->task_id;
						vec4_interpolate(border_color, border_blend_color, prof->system_colors[info->system], 0.525f);
						ui_background_color(prof->system_colors[info->system])
						ui_border_color(border_color)
						ui_width(ui_size_unit(intv_inline(p->ns_start, p->ns_end)))
						ui_height(ui_size_unit(intv_inline(p->depth, p->depth+1)))
						ui_external_text_layout(info->layout, info->id)
						p->cache = ui_node_alloc_cached(UI_DRAW_BACKGROUND | UI_DRAW_BORDER | UI_DRAW_GRADIENT | UI_DRAW_TEXT | UI_TEXT_EXTERNAL_LAYOUT | UI_DRAW_TEXT_FADE, p->id, utf8_empty(), p->cache);
					}

					fh = fh->next;
				}
			}
		}
	}

	ui_frame_end();
}

static void led_ui_test(struct led *led, const struct ui_visual *visual)
{
	system_window_set_global(led->window);
	cmd_queue_execute();

	struct system_window *win = system_window_address(led->window);
	ui_frame_begin(win->size, visual);

	ui_text_align_x(ALIGN_LEFT)
	ui_child_layout_axis(AXIS_2_Y)
	ui_parent(ui_node_alloc_f(UI_DRAW_BACKGROUND | UI_DRAW_BORDER, "###window_%u", led->window).index)
	ui_flags(UI_DRAW_ROUNDED_CORNERS | UI_TEXT_ALLOW_OVERFLOW)
	ui_child_layout_axis(AXIS_2_X)
	ui_height(ui_size_pixel(32.0f, 1.0f))
	/* Testbed for ui features*/
	{
		win->cmd_console->visible = 1;
		ui_width(ui_size_perc(1.0f))
		if (win->cmd_console->visible)
		{
			ui_cmd_console(win->cmd_console, "###console_%p", win->ui);
		};


		for (u32 r = 0; r < 5; ++r)
		{
			ui_height(ui_size_perc(0.1f))
			ui_parent(ui_node_alloc_f(UI_FLAG_NONE, "###row_%u", r).index)
			{
				ui_sprite_color(vec4_inline(0.4f, 0.15f, 0.75f, 0.7f))
				ui_sprite(SPRITE_LED_FOLDER)
				ui_background_color(vec4_inline(204.0f/256.0f, 48.0f/256.0f, 96.0f/256.0f, 0.7f))
				ui_intv_viewable_x(intv_inline(100.0f, 200.0f))	
				for (u32 i = 0; i <= 10; ++i)
				{
					ui_width(ui_size_unit(intv_inline(95.0f + i*10.0f, 105.0f + i*10.0f)))
					ui_height(ui_size_perc(1.0f))
					(i % 2) 
						? ui_node_alloc_f(UI_DRAW_BACKGROUND | UI_DRAW_GRADIENT | UI_DRAW_BORDER | UI_DRAW_ROUNDED_CORNERS, "###box_%u_%u", r, i)
						: ui_node_alloc_f(UI_DRAW_SPRITE, "###box_%u_%u", r, i);
				}
			}
		}
	
		ui_height(ui_size_perc(0.1f))
		ui_parent(ui_node_alloc_f(UI_FLAG_NONE, "###row_%u", 6).index)
		{
			ui_height(ui_size_perc(1.0f))
			{
				for (u32 i = 0; i < 8; ++i)
				{
					ui_width(ui_size_pixel(400, 1.0f / (2 << i)))
					ui_background_color(vec4_inline((214.0f - i*30.0f)/256.0f, (48.0f + i*30.0f)/256.0f, (44.0f + i*30.0f)/256.0f, 0.7f))
					ui_node_alloc_f(UI_DRAW_BACKGROUND, "###box_%u_%u", 6, i);
				}

				ui_width(ui_size_pixel(400, 1.0f / (2 << 8)))
				ui_background_color(vec4_inline((204.0f- 8*20.0f)/256.0f, (48.0f + 8*20.0f)/256.0f, (96.0f + 8*10.0f)/256.0f, 0.7f))
				ui_node_alloc_f(UI_DRAW_BACKGROUND, "###box_%u_%u", 6, 8);
			}
		}
		
		ui_height(ui_size_perc(0.1f))
		ui_parent(ui_node_alloc_f(UI_FLAG_NONE, "###row_%u", 5).index)
		{
			ui_height(ui_size_perc(1.0f))
			ui_parent(ui_node_alloc_f(UI_FLAG_NONE, "###row_%u", 7).index)
			{
				ui_width(ui_size_pixel(80, 1.0f))
				ui_height(ui_size_pixel(80, 1.0f))
				ui_fixed_x(220.0f)
				ui_fixed_y(220.0f)
				ui_background_color(vec4_inline(0.1f, 0.3f, 0.6f, 0.7f))
				ui_node_alloc_f(UI_DRAW_BACKGROUND, "###box_%u_%u", 7, 0);
			}
		}

		ui_height(ui_size_perc(0.1f))
		ui_sprite_color(vec4_inline(1.0f, 1.0f, 1.0f, 1.0f))
		ui_parent(ui_node_alloc_f(UI_FLAG_NONE, "###row_%u", 8).index)
		{
			ui_width(ui_size_text(F32_INFINITY, 1.0f))
			ui_height(ui_size_perc(1.0f))
			ui_node_alloc_f(UI_DRAW_TEXT | UI_DRAW_BACKGROUND | UI_DRAW_BORDER, "text centering!###box_%u_%u", 8, 0);

			ui_flags(UI_TEXT_ALLOW_OVERFLOW)
			ui_width(ui_size_pixel(96.0f, 1.0f))
			ui_height(ui_size_perc(1.0f))
			{
				ui_text_align_x(ALIGN_LEFT)
				ui_text_align_y(ALIGN_BOTTOM)
				ui_node_alloc_f(UI_DRAW_TEXT | UI_DRAW_BACKGROUND | UI_DRAW_BORDER, "LB###box_%u_%u", 8, 1);
				ui_text_align_x(ALIGN_LEFT)
				ui_text_align_y(ALIGN_Y_CENTER)
				ui_node_alloc_f(UI_DRAW_TEXT | UI_DRAW_BACKGROUND | UI_DRAW_BORDER, "LC###box_%u_%u", 8, 2);
				ui_text_align_x(ALIGN_LEFT)
				ui_text_align_y(ALIGN_TOP)
				ui_node_alloc_f(UI_DRAW_TEXT | UI_DRAW_BACKGROUND | UI_DRAW_BORDER, "LT###box_%u_%u", 8, 3);
				ui_text_align_x(ALIGN_X_CENTER)
				ui_text_align_y(ALIGN_BOTTOM)
				ui_node_alloc_f(UI_DRAW_TEXT | UI_DRAW_BACKGROUND | UI_DRAW_BORDER, "CB###box_%u_%u", 8, 4);
				ui_text_align_x(ALIGN_X_CENTER)
				ui_text_align_y(ALIGN_Y_CENTER)
				ui_node_alloc_f(UI_DRAW_TEXT | UI_DRAW_BACKGROUND | UI_DRAW_BORDER, "CC###box_%u_%u", 8, 5);
				ui_text_align_x(ALIGN_X_CENTER)
				ui_text_align_y(ALIGN_TOP)
				ui_node_alloc_f(UI_DRAW_TEXT | UI_DRAW_BACKGROUND | UI_DRAW_BORDER, "CT###box_%u_%u", 8, 6);
				ui_text_align_x(ALIGN_RIGHT)
				ui_text_align_y(ALIGN_BOTTOM)
				ui_node_alloc_f(UI_DRAW_TEXT | UI_DRAW_BACKGROUND | UI_DRAW_BORDER, "RB###box_%u_%u", 8, 7);
				ui_text_align_x(ALIGN_RIGHT)
				ui_text_align_y(ALIGN_Y_CENTER)
				ui_node_alloc_f(UI_DRAW_TEXT | UI_DRAW_BACKGROUND | UI_DRAW_BORDER, "RC###box_%u_%u", 8, 8);
				ui_text_align_x(ALIGN_RIGHT)
				ui_text_align_y(ALIGN_TOP)
				ui_node_alloc_f(UI_DRAW_TEXT | UI_DRAW_BACKGROUND | UI_DRAW_BORDER, "RT###box_%u_%u", 8, 9);
			}
		}

		ui_height(ui_size_perc(0.1f))
		ui_sprite_color(vec4_inline(1.0f, 1.0f, 1.0f, 1.0f))
		ui_font(FONT_DEFAULT_SMALL)
		ui_parent(ui_node_alloc_f(UI_FLAG_NONE, "###row_%u", 9).index)
		{
			ui_width(ui_size_text(F32_INFINITY, 1.0f))
			ui_height(ui_size_perc(1.0f))
			ui_background_color(vec4_inline(0.2f, 0.2, 0.4f, 0.7f))
			ui_node_alloc_f(UI_DRAW_TEXT | UI_DRAW_BACKGROUND | UI_DRAW_BORDER, "###box_%u_%u", 9, 0);

			ui_width(ui_size_text(F32_INFINITY, 1.0f))
			ui_height(ui_size_perc(1.0f))
			ui_background_color(vec4_inline(0.2f, 0.2, 0.4f, 0.7f))
			ui_node_alloc_f(UI_DRAW_TEXT | UI_DRAW_BACKGROUND | UI_DRAW_BORDER, "awd###box_%u_%u", 9, 1);
		}
	}

	ui_frame_end();
}

static void led_ui(struct led *led, const struct ui_visual *visual)
{
	system_window_set_global(led->window);
	cmd_queue_execute();

	struct system_window *win = system_window_address(led->window);
	ui_frame_begin(win->size, visual);

	static u32 count = 0;
	static u32 once = 1;

	static struct ui_dropdown_menu menu1;
	static struct ui_dropdown_menu menu2;
	static struct ui_dropdown_menu menu3;
	static struct ui_dropdown_menu menu4;

	if (once)
	{
		once = 0;
		for (; count < 50; count++)
		{
			const utf8 id = utf8_format(g_ui->mem_frame, "node_%u", count);
			cmd_submit_f(g_ui->mem_frame, "led_node_add \"%k\"", &id);
		}

		led->node_ui_list = ui_list_init(AXIS_2_Y, 256.0f, 24.0f, UI_SELECTION_MULTI); 
		led->node_selected_ui_list = ui_list_init(AXIS_2_Y, 512.0f, 24.0f + 3*24.0f + 12.0f, UI_SELECTION_NONE);
		led->collision_shape_list = ui_list_init(AXIS_2_Y, 200.0f, 24.0f, UI_SELECTION_UNIQUE);

	 	menu1 = ui_dropdown_menu_init(150.0f, vec2_inline(96.0f, 24.0f), UI_DROPDOWN_BELOW);	
	 	menu2 = ui_dropdown_menu_init(150.0f, vec2_inline(96.0f, 24.0f), UI_DROPDOWN_RIGHT);	
	 	menu3 = ui_dropdown_menu_init(150.0f, vec2_inline(96.0f, 24.0f), UI_DROPDOWN_LEFT);	
	 	menu4 = ui_dropdown_menu_init(150.0f, vec2_inline(96.0f, 24.0f), UI_DROPDOWN_ABOVE);	
	}

	ui_text_align_x(ALIGN_LEFT)
	ui_text_align_y(ALIGN_BOTTOM)
	ui_child_layout_axis(AXIS_2_X)
	ui_parent(ui_node_alloc_f(UI_DRAW_BACKGROUND | UI_DRAW_BORDER, "###window_%u", led->window).index)
	{
		ui_width(ui_size_perc(0.825f))
		ui_child_layout_axis(AXIS_2_Y)
		ui_parent(ui_node_alloc_non_hashed(UI_FLAG_NONE).index)
		ui_width(ui_size_perc(1.0f))
		{
			ui_height(ui_size_pixel(32.0f, 1.0f))
			ui_child_layout_axis(AXIS_2_X)
			ui_parent(ui_node_alloc_non_hashed(UI_DRAW_BORDER).index)
			{
				ui_pad_fill();

				ui_background_color(vec4_inline(0.0f, 0.125f, 0.125f, 1.0f))
				ui_flags(UI_DRAW_BACKGROUND)
				{
					struct ui_node *button;
					ui_width(ui_size_pixel(32.0f, 1.0f))
					ui_flags(UI_DRAW_SPRITE)
					ui_background_color(vec4_inline(0.5f, 0.5f, 0.5f, 0.5f))
					ui_sprite_color(vec4_inline(0.0f, 0.0f, 0.0f, 0.1f))
					ui_sprite(SPRITE_LED_PLAY)
					if (ui_button_f(UI_DRAW_BACKGROUND | UI_DRAW_SPRITE, "###play") & UI_INTER_LEFT_CLICK)
					{
						//ui_background_color(vec4_inline(0.0f, 0.5f, 0.5f, 0.5f))
						fprintf(stderr, "Play!\n");
					}
					
					ui_pad();

					ui_width(ui_size_pixel(32.0f, 1.0f))
					ui_flags(UI_DRAW_SPRITE)
					ui_sprite_color(vec4_inline(0.0f, 0.0f, 0.0f, 0.1f))
					ui_sprite(SPRITE_LED_PAUSE)
					if (ui_button_f(UI_DRAW_SPRITE, "###pause") & UI_INTER_LEFT_CLICK)
					{
						fprintf(stderr, "Pause!\n");
					}

					ui_pad();

					ui_width(ui_size_pixel(32.0f, 1.0f))
					ui_flags(UI_DRAW_SPRITE)
					ui_sprite_color(vec4_inline(0.0f, 0.0f, 0.0f, 0.1f))
					ui_sprite(SPRITE_LED_STOP)
					if (ui_button_f(UI_DRAW_SPRITE, "###stop") & UI_INTER_LEFT_CLICK)
					{
						fprintf(stderr, "Stop!\n");
					}
				}

				ui_pad_fill();
			}

			ui_height(ui_size_perc(1.0f))
			ui_text_align_y(ALIGN_TOP)
			{
				struct slot slot;
				const utf32 external_text = utf32_cstr(g_ui->mem_frame, "Viewport");

				slot = ui_node_alloc(UI_DRAW_BORDER | UI_INTER_FLAGS, &led->viewport_id);
				if (slot.index != HI_ORPHAN_STUB_INDEX)
				ui_parent(slot.index)
				{
					struct ui_node *node = slot.address;
					if (node->inter & UI_INTER_HOVER)
					{	
						ui_external_text(external_text)
						ui_background_color(vec4_inline(0.8f, 0.8f, 0.8f, 1.0f))
						ui_sprite_color(vec4_inline(0.1f, 0.1f, 0.1f, 1.0f))
						ui_height(ui_size_pixel(24.0f, 1.0f))
						ui_width(ui_size_text(F32_INFINITY, 1.0f))
						ui_fixed_x(g_ui->inter.cursor_position[0])
						ui_fixed_y(g_ui->inter.cursor_position[1])
						ui_node_alloc_non_hashed(UI_DRAW_BACKGROUND | UI_DRAW_BORDER | UI_TEXT_EXTERNAL | UI_DRAW_TEXT | UI_SKIP_HOVER_SEARCH);
					}

					if (node->inter & UI_INTER_LEFT_CLICK)
					{
						const utf8 id = utf8_format(g_ui->mem_frame, "node_%u", count++);
						cmd_submit_f(g_ui->mem_frame, "led_node_add \"%k\"", &id);
					}
				}
			}

			u32 shape_selected = U32_MAX;
			ui_height(ui_size_pixel(256.0f, 1.0f))
			ui_child_layout_axis(AXIS_2_X)
			ui_parent(ui_node_alloc_non_hashed(UI_DRAW_BORDER).index)
			ui_height(ui_size_perc(1.0f))
			{
				ui_pad();

				ui_width(ui_size_pixel(186.0f, 1.0f))
				ui_child_layout_axis(AXIS_2_Y)
				ui_parent(ui_node_alloc_non_hashed(UI_FLAG_NONE).index)
				ui_height(ui_size_pixel(24.0f, 1.0f))
				ui_width(ui_size_pixel(180.0f, 1.0f))
				{
					ui_pad();

					utf8 new_shape_id;
					new_shape_id = ui_field_utf8_f("Add Collision Shape...###new_shape");
					if (new_shape_id.len)
					{
						utf8_debug_print(new_shape_id);
						g_queue->cmd_exec->arg[0].utf8 = new_shape_id;
						cmd_submit_f(g_ui->mem_frame, "collision_shape_add \"%k\"", &new_shape_id);
					}

					ui_pad();

					const struct collision_shape *shape;
					ui_list(&led->collision_shape_list, "###%p", &led->collision_shape_list)
					for (u32 i = led->collision_shape_db.allocated_dll.first; i != DLL_NULL; i = DB_NEXT(shape))
					{
						shape = string_database_address(&led->collision_shape_db, i);
						struct slot entry = ui_list_entry_alloc_f(&led->collision_shape_list, "###%p_%u", &led->collision_shape_list, i);
						if (entry.index)
						ui_parent(entry.index)
						{
							if (entry.index == led->collision_shape_list.last_selected)
							{
								shape_selected = i;
							}
							ui_node_alloc_f(UI_DRAW_TEXT | UI_TEXT_ALLOW_OVERFLOW, "%k##%u", &shape->id, i);
						}
					}

					ui_pad_fill();
				}

				ui_width(ui_size_pixel(192.0f, 1.0f))
				ui_child_layout_axis(AXIS_2_X)
				ui_parent(ui_node_alloc_non_hashed(UI_DRAW_BORDER).index)
				if (led->collision_shape_list.last_selection_happened == g_ui->frame)
				{
					ui_pad();

					ui_child_layout_axis(AXIS_2_Y)
					ui_width(ui_size_pixel(180.0f, 1.0f))
					ui_parent(ui_node_alloc_non_hashed(0).index)
					{
						ui_pad();

						struct collision_shape *shape = string_database_address(&led->collision_shape_db, shape_selected);
						ui_height(ui_size_pixel(24.0f, 1.0f))
						ui_node_alloc_f(UI_DRAW_TEXT | UI_TEXT_ALLOW_OVERFLOW | UI_DRAW_BORDER, "%k##shape_selected", &shape->id);

						ui_pad();

						switch (shape->type)
						{
							case COLLISION_SHAPE_SPHERE:
							{
								ui_height(ui_size_pixel(24.0f, 1.0f))
								ui_node_alloc_f(UI_DRAW_TEXT | UI_TEXT_ALLOW_OVERFLOW, "type: SPHERE");
								shape->sphere.radius = ui_field_f32_f(shape->sphere.radius, intv_inline(0.0125f, 100.0f), "%f###cs_radius", shape->sphere.radius);
							} break;

							case COLLISION_SHAPE_CAPSULE:
							{
								ui_height(ui_size_pixel(24.0f, 1.0f))
								ui_node_alloc_f(UI_DRAW_TEXT | UI_TEXT_ALLOW_OVERFLOW, "type: CAPSULE");
							} break;

							case COLLISION_SHAPE_CONVEX_HULL:
							{
								ui_height(ui_size_pixel(24.0f, 1.0f))
								ui_node_alloc_f(UI_DRAW_TEXT | UI_TEXT_ALLOW_OVERFLOW, "type: CONVEX HULL");
							} break;
						}

						ui_pad_fill();
					}

					ui_pad();
				}


				ui_width(ui_size_perc(1.0f))
				ui_child_layout_axis(AXIS_2_Y)
				ui_parent(ui_node_alloc_non_hashed(0).index)
				{
					ui_height(ui_size_pixel(24.0f, 1.0f))
					ui_child_layout_axis(AXIS_2_X)
					ui_parent(ui_node_alloc_non_hashed(UI_DRAW_BORDER).index)
					{
						if (ui_dropdown_menu_f(&menu1, "DD BELOW"))
						{
							ui_dropdown_menu_push(&menu1);

							for (u32 k = 0; k < 5; ++k)
							{
								struct ui_node *drop;
								ui_flags(UI_DRAW_TEXT | UI_TEXT_ALLOW_OVERFLOW)
								drop = ui_dropdown_menu_entry_f(&menu1, "entry_%u##%p", k, &menu1).address;
								if (drop->inter & UI_INTER_HOVER)
								{
									fprintf(stderr, "entry_%u\n", k);	
								}
								if (drop->inter & UI_INTER_SELECT)
								{
									fprintf(stderr, "woof\n");	
								}
							}

							ui_dropdown_menu_pop(&menu1);
						}

						if (ui_dropdown_menu_f(&menu2, "DD RIGHT"))
						{
							ui_dropdown_menu_push(&menu2);

							for (u32 k = 0; k < 5; ++k)
							{
								struct ui_node *drop;
								ui_flags(UI_DRAW_TEXT | UI_TEXT_ALLOW_OVERFLOW)
								drop = ui_dropdown_menu_entry_f(&menu2, "entry_%u##%p", k, &menu2).address;
								if (drop->inter & UI_INTER_SELECT)
								{
									fprintf(stderr, "meow\n");	
								}
							}

							ui_dropdown_menu_pop(&menu2);
						}

						if (ui_dropdown_menu_f(&menu3, "DD LEFT"))
						{
							ui_dropdown_menu_push(&menu3);

							for (u32 k = 0; k < 5; ++k)
							{
								struct ui_node *drop;
								ui_flags(UI_DRAW_TEXT | UI_TEXT_ALLOW_OVERFLOW)
								drop = ui_dropdown_menu_entry_f(&menu3, "entry_%u##%p", k, &menu3).address;
								if (drop->inter & UI_INTER_SELECT)
								{
									fprintf(stderr, "meow\n");	
								}
							}

							ui_dropdown_menu_pop(&menu3);
						}

						if (ui_dropdown_menu_f(&menu4, "DD ABOVE"))
						{
							ui_dropdown_menu_push(&menu4);

							for (u32 k = 0; k < 5; ++k)
							{
								struct ui_node *drop;
								ui_flags(UI_DRAW_TEXT | UI_TEXT_ALLOW_OVERFLOW)
								drop = ui_dropdown_menu_entry_f(&menu4, "entry_%u##%p", k, &menu4).address;
								if (drop->inter & UI_INTER_SELECT)
								{
									fprintf(stderr, "meow\n");	
								}
							}

							ui_dropdown_menu_pop(&menu4);
						}
					}

					ui_pad_fill();
				}
			}

			win->cmd_console->visible = 1;
			ui_height(ui_size_pixel(32.0f, 1.0f))
			if (win->cmd_console->visible)
			{
				ui_cmd_console(win->cmd_console, "###console_%p", win->ui);
			};
		}

		ui_width(ui_size_perc(0.175f))
		ui_child_layout_axis(AXIS_2_Y)
		ui_parent(ui_node_alloc_non_hashed(0).index)
		ui_flags(UI_DRAW_ROUNDED_CORNERS | UI_TEXT_ALLOW_OVERFLOW)
		ui_width(ui_size_perc(1.0f))
		{
			struct led_node *node = NULL;
			ui_height(ui_size_pixel(256.0f, 1.0f))
			ui_list(&led->node_ui_list, "###%p", &led->node_ui_list)
			for (u32 i = led->node_non_marked_list.first; i != DLL_NULL; i = DLL_NEXT(node))
			{
				node = gpool_address(&led->node_pool, i);
				ui_child_layout_axis(AXIS_2_X)
				node->cache = ui_list_entry_alloc_cached(&led->node_ui_list, 
						       	node->id,
							node->cache);

				if (node->cache.frame_node)
				ui_parent(node->cache.index)
				{
					ui_pad(); 

					ui_node_alloc_f(UI_DRAW_TEXT | UI_TEXT_ALLOW_OVERFLOW, "%k##%u", &node->id, i);
				}

				struct ui_node *ui_node = node->cache.frame_node;
				if (ui_node->inter & UI_INTER_SELECT)
				{
					if (!DLL2_IN_LIST(node))
					{
						dll_append(&led->node_selected_list, led->node_pool.buf, i);
					}
				}
				else
				{
					if (DLL2_IN_LIST(node))
					{
						dll_remove(&led->node_selected_list, led->node_pool.buf, i);
					}
				}
			}

			ui_list(&led->node_selected_ui_list, "###%p", &led->node_selected_ui_list)
			for (u32 i = led->node_selected_list.first; i != DLL_NULL; i = DLL2_NEXT(node))
			{
				node = gpool_address(&led->node_pool, i);
				ui_child_layout_axis(AXIS_2_Y)
				ui_parent(ui_list_entry_alloc_f(&led->node_selected_ui_list, "###%p_%u", &led->node_selected_ui_list, i).index)
				{
					ui_height(ui_size_pixel(24.0f, 1.0f))
					ui_node_alloc_f(UI_DRAW_TEXT | UI_TEXT_ALLOW_OVERFLOW, "%k##sel_%u", &node->id, i);
					ui_height(ui_size_pixel(3*24.0f + 12.0f, 1.0f))
					ui_child_layout_axis(AXIS_2_X)
					ui_parent(ui_node_alloc_non_hashed(UI_DRAW_BORDER).index)
					ui_child_layout_axis(AXIS_2_Y)
					{
						ui_text_align_y(ALIGN_TOP)	
						ui_width(ui_size_pixel(128.0f, 0.0f))
						ui_parent(ui_node_alloc_non_hashed(UI_FLAG_NONE).index)
						ui_height(ui_size_pixel(24.0f, 1.0f))
						{
							ui_pad_pixel(6.0f);

							ui_height(ui_size_pixel(3*24.0f, 1.0f))
							ui_node_alloc_f(UI_DRAW_TEXT | UI_TEXT_ALLOW_OVERFLOW, "position##%u", i);
							ui_pad_pixel(6.0f);
						}

						ui_parent(ui_node_alloc_non_hashed(UI_FLAG_NONE).index)
						ui_height(ui_size_pixel(24.0f, 1.0f))
						{
							ui_pad_pixel(6.0f);
							
							node->position[0] = ui_field_f32_f(node->position[0], intv_inline(-10.0f, 10.0f), "%f###field_x_%u", node->position[0], i);
							node->position[1] = ui_field_f32_f(node->position[1], intv_inline(-10.0f, 10.0f), "%f###field_y_%u", node->position[1], i);
							node->position[2] = ui_field_f32_f(node->position[2], intv_inline(-10.0f, 10.0f), "%f###field_z_%u", node->position[2], i);

							ui_pad_pixel(6.0f);
						}

						ui_pad_fill();
					}
				}
			}

			ui_pad_fill();
		}
	}

	ui_frame_end();
}

void led_ui_main(struct led *led)
{
	KAS_TASK(__func__, T_UI);

	const vec4 bg = { 0.0625f, 0.0625f, 0.0625f, 1.0f };
	const vec4 br = { 0.0f, 0.15f, 0.25f, 1.0f };
	const vec4 gr[BOX_CORNER_COUNT] = 
	{
		{0.0f, 0.15f, 0.8f, 0.8f },
		{0.0f, 0.7f, 0.25f, 0.8f },
		{0.0f, 0.7f, 0.25f, 0.8f },
		{0.0f, 0.15f, 0.8f, 0.8f },
	};
	const vec4 sp = { 0.9f, 0.9f, 0.9f, 1.0f };

	const f32 pad = 8.0f;
	const f32 edge_softness = 0.0f;
	const f32 corner_radius = 3.0f;
	const f32 border_size = 1.0f;	
	const f32 text_pad_x = 4.0f;
	const f32 text_pad_y = 4.0f;

	const struct ui_visual visual = ui_visual_init(bg, br, gr, sp, pad, edge_softness, corner_radius, border_size, FONT_DEFAULT_SMALL, ALIGN_X_CENTER, ALIGN_Y_CENTER, text_pad_x, text_pad_y);

	//led_ui_test(led, &visual);
	led_ui(led, &visual);

	if (led->project_menu.window)
	{
		led_project_menu_ui(led, &visual);
	}

	if (led->profiler.window)
	{
		led_profiler_ui(led, &visual);
	}
	else
	{
		g_kaspf_reader->read_state = KASPF_READER_CLOSED;
	}

	KAS_END;
}
