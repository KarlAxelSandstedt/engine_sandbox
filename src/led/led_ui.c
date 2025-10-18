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
			if (ui_button_f("Refresh###ref")->clicked)
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

			struct allocation_slot entry = ui_list_entry_alloc(&menu->dir_list);
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

			if (ui_button_f("New Project")->clicked && menu->popup_new_project.state == UI_POPUP_STATE_NULL)
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

			if (ui_button_f("Load")->clicked)
			{
				fprintf(stderr, "Load!\n");
			}

			ui_pad();

			if (ui_button_f("Delete")->clicked)
			{
				fprintf(stderr, "Delete!\n");
			}
		}
	}

	ui_frame_end();
}

//static void draw_kas_profiler(struct arena *mem_frame, struct ui_state *ui)
//{
//	if (g_kas->frame_counter == 0) { return; }
//	
//	const vec4 bg = { 0.0625f, 0.0625f, 0.0625f, 1.0f };
//	const vec4 br = { 0.4f, 0.4f, 0.7f, 0.5f };
//	const vec4 tx = { 0.7f, 0.7f, 0.9f, 1.0f };
//
//	const vec2i32 zero2i32 = { 0, 0 };
//
//	TEXT_COLOR(ui, tx)
//	{
//	BACKGROUND_COLOR(ui, bg)
//	{
//	BORDER_COLOR(ui, br)
//	{
//
//	/* PROFILER WINDOW */
//	const utf8 prof_id = KAS_COMPILE_TIME_STRING("prof_win");
//	const vec2i32 prof_pos = { 0, 0 };
//	const vec2i32 prof_size = { (i32) ui->comm.window_size[0], (i32) ui->comm.window_size[1] };
//	struct ui_unit *profiler_window = ui_area(ui, &prof_id, prof_pos, prof_size, UNIT_DRAW_BACKGROUND | UNIT_DRAW_BORDER | UNIT_DRAW_ROUNDED_CORNER | UNIT_DRAW_EDGE_SOFTNESS, UNIT_INTER_NONE, UNIT_INTER_NONE);
//
//	ui_unit_push(ui, profiler_window);
//	{
//		const u64 ns_full = g_kas->ns_frame_prev - g_kas->header->l1_table.ns_start;
//		u64 ns_start = g_kaspf_reader->ns_start;
//		u64 ns_end = g_kaspf_reader->ns_end;
//		u64 ns_interval = ns_end - ns_start;
//		u64 ns_full_interval = g_kaspf_reader->interval_high[1] - g_kaspf_reader->interval_low[0];
//		
//		/* PROFILER TITLE */
//		const utf8 title_center_id = KAS_COMPILE_TIME_STRING("title_center");
//		const utf8 title_id = KAS_COMPILE_TIME_STRING("title");
//		const vec2i32 title_pos = { (i32) ui->comm.window_size[0] / 2, -g_prof_config->title_bar_height / 2 };
//		utf32 title = utf32_duplicate_cstr(mem_frame, "KAS PROFILER");
//		ui_text_line_centered(ui, &title_id, title_pos, &title, 0);
//
//		/* INTERVAL STREAMING BUTTON */
//		const utf8 s_but_id = KAS_COMPILE_TIME_STRING("s_but_id");
//		const vec2i32 s_but_pos = { ui->comm.window_size[0] - 1*g_prof_config->worker_bar_x_pad, -g_prof_config->title_bar_height };
//		const vec2i32 s_but_size = { g_prof_config->scroll_bar_height, g_prof_config->scroll_bar_height };
//		if (ui_button(ui, &s_but_id, s_but_pos, s_but_size, UNIT_DRAW_BACKGROUND | UNIT_DRAW_BORDER | UNIT_DRAW_ROUNDED_CORNER | UNIT_DRAW_EDGE_SOFTNESS))
//		{
//			g_kaspf_reader->read_state = KASPF_READER_STREAM;
//		}
//
//		const vec4 checkbox_background_color = { 0.1f, 0.1f, 0.1f, 1.0f };
//		const vec4 checkbox_text_color = { 0.9f, 0.9f, 0.9f, 1.0f };
//		BACKGROUND_COLOR(ui, checkbox_background_color)
//		{
//			TEXT_COLOR(ui, checkbox_text_color)
//			{
//				/* DRAWING OPTIONS CHECKBOXES */
//				const vec2i32 checkbox_offset = { 0, 0 };
//				const i32 bar_height = 24;
//				const i32 checkbox_side = 20;
//				const vec2i32 checkbox_size = { checkbox_side, checkbox_side };
//
//				const utf8 draw_online_id = KAS_COMPILE_TIME_STRING("draw_online");
//				const vec2i32 box_online_pos = { g_prof_config->worker_bar_x_pad, -g_prof_config->title_bar_height + 32};
//				const utf32 box_online_title = utf32_duplicate_cstr(mem_frame, "Draw worker online activity");
//				const utf32 box_online_description = utf32_duplicate_cstr(mem_frame, "Draw the time intervals of when each worker is swapped in by the operating system scheduler.");
//				ui_descriptive_checkbox_bar(mem_frame, ui, &draw_online_id, box_online_pos, bar_height, checkbox_offset, checkbox_size, &box_online_title, &box_online_description, 256, UNIT_DRAW_NONE, &g_prof_config->draw_worker_activity_online);
//
//			}
//		}
//
//
//		/* INTERVAL SCROLLER */
//		const utf8 sc_bar_id = KAS_COMPILE_TIME_STRING("sc_bar");
//		const vec2i32 sc_pos = { g_prof_config->worker_bar_x_pad, -g_prof_config->title_bar_height };
//		const vec2i32 sc_bar_size = { ui->comm.window_size[0] - 2*g_prof_config->worker_bar_x_pad,  g_prof_config->scroll_bar_height };
//		const f64 ns_per_scroll_pixel = (f64) ns_full / sc_bar_size[0];
//		const vec2 sc_coverage = 
//		{ 
//			(f32) (ns_start - g_kas->header->l1_table.ns_start) / ns_full, 
//			(f32) (ns_end - g_kas->header->l1_table.ns_start) / ns_full 
//		};
//		struct ui_unit *timeline_scroll = ui_scroll(mem_frame, ui, &sc_bar_id, sc_pos, sc_bar_size, sc_coverage, UNIT_DRAW_BACKGROUND | UNIT_DRAW_BORDER | UNIT_DRAW_ROUNDED_CORNER | UNIT_DRAW_EDGE_SOFTNESS);
//		if (timeline_scroll)
//		{
//			const i64 ns_new_start = (i64) ns_start + ns_per_scroll_pixel * ui->comm.delta_pixel[0];
//			//fprintf(stderr, "ns_start before: %li\n", ns_start);
//			ns_start = (ns_new_start < 0) ? 0
//			    	 : (ns_new_start + ns_interval > ns_full) ? ns_full - ns_interval : (u64) ns_new_start;
//			ns_end = ns_start + ns_interval;
//			//fprintf(stderr, "ns_start after:  %li\n", ns_start);
//						  
//			g_kaspf_reader->read_state = KASPF_READER_FIXED;
//			kaspf_reader_process(mem_frame, ns_start, ns_end);
//			//fprintf(stderr, "ns_start end:    %li\n", g_kaspf_reader->ns_start);
//		}
//		g_prof_config->timeline.time_end = ns_full;
//		g_prof_config->timeline.interval_start = ns_start;
//		g_prof_config->timeline.interval_end = ns_end;
//		g_prof_config->timeline.title_column_width = 128;
//		
//		vec2i32_set(g_prof_config->timeline.timeline_size
//				, ui->comm.window_size[0] - g_prof_config->worker_bar_x_pad - g_prof_config->worker_bar_title_length
//				, 500);
//
//		const utf8 timeline_id = KAS_COMPILE_TIME_STRING("timeline");
//		const vec2i32 timeline_position = 
//		{ 
//			0,
//			-(g_prof_config->title_bar_height + g_prof_config->scroll_bar_height),
//		};
//	
//		ui_timeline(mem_frame, ui, &timeline_id, timeline_position, &g_prof_config->timeline, UNIT_DRAW_BACKGROUND | UNIT_DRAW_BORDER | UNIT_DRAW_ROUNDED_CORNER | UNIT_DRAW_EDGE_SOFTNESS, UNIT_INTER_NONE, UNIT_INTER_DRAG | UNIT_INTER_SCROLL);
//		
//		
//		u64 frame_task_id = 0;
//		for (u32 wi = 0; wi < g_task_ctx->worker_count; ++wi)
//		{
//			struct timeline_row_config *row_config = g_prof_config->timeline_row + wi;
//			KAS_TASK("gen worker ui units", T_UI);
//			ui_timeline_row_create_and_push(mem_frame, ui, row_config, &g_prof_config->timeline);
//			{
//				struct hw_frame_header *fh = g_kaspf_reader->low;
//				for (u64 fi = g_kaspf_reader->frame_low; fi <= g_kaspf_reader->frame_high; ++fi) 
//				{
//					struct hw_profile_header *hw_h = fh->hw_profile_h + wi;
//					for (u64 pi = 0; pi < hw_h->profile_count; ++pi)
//					{
//						struct hw_profile *p  = hw_h->profiles + pi;
//						f32 perc_x0 = (f32) ((i64) p->ns_start - (i64) ns_start) / ns_interval;
//						f32 perc_x1 = perc_x0 + (f32) (p->ns_end - p->ns_start) / ns_interval;
//
//						perc_x0 = f32_max(0.0f, perc_x0);
//						perc_x1 = f32_min(f32_max(perc_x0, perc_x1), 1.0f);
//
//						vec2i32 task_size =
//						{
//							(i32) ((perc_x1 - perc_x0) * (g_prof_config->timeline.timeline_size[0] - 2)),
//							g_prof_config->task_height,
//						};
//						if (task_size[0] < 1) 
//						{ 
//							continue;
//						}
//						
//						const vec2i32 task_pos =
//						{
//							1 + (i32) (perc_x0 * (g_prof_config->timeline.timeline_size[0]-1)),
//							-1 - g_prof_config->task_height * p->depth, 
//						};
//
//						const vec2i32 baseline =
//						{
//							2 + (i32) (perc_x0 * (g_prof_config->timeline.timeline_size[0]-1)),
//							task_pos[1]-task_size[1] - ui_unit_font(ui, g_prof_config->timeline_row[wi].ui_timeline)->descent
//						};
//						
//						u8 *task_buf = arena_push(mem_frame, BUFSIZE);
//						u8 *task_text_buf = arena_push(mem_frame, BUFSIZE);
//						const u64 u = frame_task_id++;
//						const utf8 task_id = utf8_format_buffered(task_buf, BUFSIZE, "t%lu_%lu", fi, u);
//						const utf8 task_text_id = utf8_format_buffered(task_buf, BUFSIZE, "tt%lu_%lu", fi, u);
//							
//						struct kaspf_task_info *info = g_kaspf_reader->task_info + p->task_id;
//						if (!info->initiated)
//						{
//							info->initiated = 1;
//							info->system = g_kas->header->mm_task_systems[p->task_id];
//							info->id = kas_utf32_duplicate_cstr_buffered(info->buf, KASPF_LABEL_BUFSIZE, (const char *) g_kas->header->mm_labels[p->task_id]);
//						}
//
//						const vec4 prof_text = { 0.0f, 0.0f, 0.0f, 0.8f };
//						const vec4 border_blend_color = { 0.0f, 0.0f, 0.0f, 1.0f };
//						vec4 border_color;
//						vec4_interpolate(border_color, border_blend_color, g_prof_config->system_colors[info->system], 0.225f);
//
//						BORDER_SIZE(ui, 1)
//						{
//						TEXT_COLOR(ui, prof_text)
//						{
//						BACKGROUND_COLOR(ui, g_prof_config->system_colors[info->system])
//						{
//						BORDER_COLOR(ui, border_color)
//						{
//
//							/* full visibility */
//							if (-task_pos[1] + task_size[1] < row_config->height)
//							{
//								ui_area(ui, &task_id, task_pos, task_size, UNIT_DRAW_BACKGROUND | UNIT_DRAW_BORDER | UNIT_DRAW_ROUNDED_CORNER, UNIT_INTER_NONE, UNIT_INTER_NONE);
//								const i32 low_pixel = (i32) task_pos[0];
//								const i32 high_pixel = (i32) task_pos[0] + task_size[0] - 1;
//								ui_text_line_bounded(ui, &task_text_id, baseline, high_pixel-low_pixel, &info->id, 0);
//							}
//							/* partial visibility, save 2 pixels for task bar border */
//							else if (-task_pos[1] < row_config->height - 2)
//							{
//
//								task_size[1] = row_config->height - 2 + task_pos[1];
//								ui_area(ui, &task_id, task_pos, task_size, UNIT_DRAW_BACKGROUND | UNIT_DRAW_BORDER | UNIT_DRAW_ROUNDED_CORNER, UNIT_INTER_NONE, UNIT_INTER_NONE);
//								const i32 low_pixel = (i32) task_pos[0];
//								const i32 high_pixel = (i32) task_pos[0] + task_size[0] - 1;
//								ui_text_line_bounded(ui, &task_text_id, baseline, high_pixel-low_pixel, &info->id, 0);
//							}
//						}
//						}
//						}
//						}
//					}
//
//					if (g_prof_config->draw_worker_activity_online)
//					{
//						const utf8 worker_activity_bar_id = utf8_format(mem_frame, "worker_activity_bar%lu_%lu", wi, fi);
//						const vec2i32 dummy_size = { 1, 1 };
//						struct ui_unit *worker_activity_bar = ui_area(ui, &worker_activity_bar_id, zero2i32, dummy_size, UNIT_DRAW_NONE, UNIT_INTER_NONE, UNIT_INTER_NONE);
//						worker_activity_bar->size.size_type[0] = UNIT_SIZE_PERC_OF_PARENT;
//						worker_activity_bar->size.size_type[1] = UNIT_SIZE_PERC_OF_PARENT;
//						worker_activity_bar->size.size[0] = 1.0f;
//						worker_activity_bar->size.size[1] = 1.0f;
//						ui_unit_push(ui, worker_activity_bar);
//						{
//							for (u64 activity = 0; activity < hw_h->activity_count; ++activity)
//							{
//								struct worker_activity *wa = hw_h->activity + activity;
//
//								f32 perc_x0 = (f32) ((i64) wa->ns_start - (i64) ns_start) / ns_interval;
//								f32 perc_x1 = perc_x0 + (f32) (wa->ns_end - wa->ns_start) / ns_interval;
//								perc_x0 = f32_max(0.0f, perc_x0);
//								perc_x1 = f32_min(f32_max(perc_x0, perc_x1), 1.0f);
//								const vec2i32 worker_activity_position =
//								{
//									(i32) 1 + llroundf(perc_x0 * (g_prof_config->timeline.timeline_size[0] - 2)),
//									0,
//								};
//								const vec2i32 worker_activity_size =
//								{
//									(i32) llroundf((perc_x1 - perc_x0) * (g_prof_config->timeline.timeline_size[0] - 2)),
//									row_config->height - 4,
//								};
//
//								const vec4 worker_activity_color = { 0.0f, 0.7f, 0.4f, 0.3f };
//								const i32 low_pixel = (i32) worker_activity_position[0];
//								const i32 high_pixel = (i32) worker_activity_position[0] + worker_activity_size[0] - 1;
//								BACKGROUND_COLOR(ui, worker_activity_color)
//								{
//
//									if (worker_activity_size[0] && (high_pixel - low_pixel > 1))
//									{
//										const utf8 worker_activity_id = utf8_format(mem_frame, "ac%lu_%lu_%lu", wi, fi, activity);
//										ui_area(ui, &worker_activity_id, worker_activity_position, worker_activity_size, UNIT_DRAW_BACKGROUND, UNIT_INTER_NONE, UNIT_INTER_NONE);
//									}
//								}
//
//							}
//						}
//						ui_unit_pop(ui);
//					}
//					fh = fh->next;
//				}
//			}
//			ui_timeline_row_pop(ui);
//			KAS_END;
//		}
//
//		//TODO can we skip inheritance completely using a hot stack? we can simply check whatever
//		// trait we want to act on immediately here...
//		//TODO We only want to activate something once, so should setup checks?
//		if (ui_unit_is_in_hot_path(ui, &g_prof_config->timeline.timeline_column_id))
//		{
//			if (ui->comm.interactions & UNIT_INTER_DRAG)
//			{
//				const f64 ns_per_window_pixel = (f64) (f64) (g_prof_config->timeline.interval_end - g_prof_config->timeline.interval_start) / g_prof_config->timeline.timeline_size[0];
//				const i64 ns_new_start = (i64) ns_start - ns_per_window_pixel * ui->comm.delta_pixel[0];
//				//fprintf(stderr, "ns_start before: %li\n", ns_start);
//				ns_start = (ns_new_start < 0) ? 0
//				    	 : (ns_new_start + ns_interval > ns_full) ? ns_full - ns_interval : (u64) ns_new_start;
//				ns_end = ns_start + ns_interval;
//				//fprintf(stderr, "ns_start after:  %li\n", ns_start);
//							  
//				g_kaspf_reader->read_state = KASPF_READER_FIXED;
//				kaspf_reader_process(mem_frame, ns_start, ns_end);
//				//fprintf(stderr, "ns_start end:    %li\n", g_kaspf_reader->ns_start);
//			}
//			else if (ui->comm.interactions & UNIT_INTER_SCROLL)
//			{
//				
//				const f64 scroll_perc = ((i64) ui->comm.scroll_up_count - (i64) ui->comm.scroll_down_count) * 0.125;
//				kas_assert(scroll_perc < 0.5);
//				ns_start += (u64) (scroll_perc * ns_interval);
//				ns_end   -= (u64) (scroll_perc * ns_interval);
//				ns_interval = ns_end - ns_start;
//
//				if (ns_interval > ns_full)
//				{
//					ns_interval = ns_full;
//					ns_start = 0;
//					ns_end = ns_full;
//				}	
//				else if (ns_end > ns_full)
//				{
//					ns_end = ns_full;
//					ns_start = ns_full-ns_interval;
//				}
//				else if ((i64) ns_start < 0)
//				{
//					ns_start = 0;
//					ns_end = ns_interval;
//				}
//
//				kaspf_reader_process(mem_frame, ns_start, ns_end);
//			}
//		}
//	}
//	ui_unit_pop(ui);
//
//	}
//	}
//	}
//}
//#endif

static void led_profiler_ui(struct led *led, const struct ui_visual *visual)
{
#if defined(KAS_PROFILER)
	if (g_kas->frame_counter == 0 || !system_user_is_admin())
	{
		return;
	}

	struct led_profiler *prof = &led->profiler;
	system_window_set_global(prof->window);
	cmd_queue_execute();

	struct system_window *win = system_window_address(prof->window);
	ui_frame_begin(win->size, visual);

	g_kaspf_reader->read_state = (prof->timeline_config.fixed)
		 ? KASPF_READER_FIXED
		 : KASPF_READER_STREAM;

	if (g_kaspf_reader->read_state == KASPF_READER_STREAM)
	{	
		const u64 ns_visible_size = NSEC_PER_SEC;
		const u64 ns_visible_end = (g_kaspf_reader->interval_high[1] < ns_visible_size)
			? ns_visible_size 
			: g_kaspf_reader->interval_high[1];
		const u64 ns_visible_start = ns_visible_end - ns_visible_size;

		prof->timeline_config.ns_interval_start = ns_visible_start;
		prof->timeline_config.ns_interval_end = ns_visible_end;
	}
	else
	{
		kaspf_reader_process(win->ui->mem_frame, prof->timeline_config.ns_interval_start, prof->timeline_config.ns_interval_end);
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
						struct kaspf_task_info *info = g_kaspf_reader->task_info + p->task_id;
						if (!info->initiated)
						{
							//TODO Bad, we tie kaspf lifetime with window lifetime, need to fix  
							info->initiated = 1;
							info->system = g_kas->header->mm_task_systems[p->task_id];
							info->id = utf32_cstr(&win->mem_persistent, (const char *) g_kas->header->mm_labels[p->task_id]);
							struct asset_font *asset = stack_ptr_top(&win->ui->stack_font);
							info->layout = utf32_text_layout(&win->mem_persistent, &info->id, F32_INFINITY, TAB_SIZE, asset->font);
						}

						const vec4 prof_text = { 0.0f, 0.0f, 0.0f, 0.8f };
						const vec4 border_blend_color = { 0.0f, 0.0f, 0.0f, 1.0f };
						vec4 border_color;
						vec4_interpolate(border_color, border_blend_color, prof->system_colors[info->system], 0.525f);
						ui_background_color(prof->system_colors[info->system])
						ui_border_color(border_color)
						ui_width(ui_size_unit(intv_inline(p->ns_start, p->ns_end)))
						ui_height(ui_size_unit(intv_inline(p->depth, p->depth+1)))
						ui_external_text(info->id)
						ui_external_text_layout(info->layout)
						ui_node_alloc_f(UI_DRAW_BACKGROUND | UI_DRAW_BORDER | UI_DRAW_GRADIENT | UI_DRAW_TEXT | UI_TEXT_EXTERNAL_LAYOUT | UI_DRAW_TEXT_FADE, "###t%u_%lu_%lu", wi, fi, pi);
					}

					fh = fh->next;
				}
			}
		}
	}

	ui_frame_end();
#endif
}

static void led_ui(struct led *led, const struct ui_visual *visual)
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
		ui_fixed_depth(64)
		ui_floating_x(0.0f)
		ui_floating_y(win->size[1] - 32.0f)
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
				ui_background_color(vec4_inline(204.0f/256.0f, 48.0f/256.0f, 64.0f/256.0f, 0.7f))
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
				ui_background_color(vec4_inline((204.0f- 8*20.0f)/256.0f, (48.0f + 8*20.0f)/256.0f, (64.0f + 8*10.0f)/256.0f, 0.7f))
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
				ui_floating_x(220.0f)
				ui_floating_y(220.0f)
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
			ui_width(ui_size_pixel(64.0f, 1.0f))
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

void led_ui_main(struct led *led)
{
	KAS_TASK(__func__, T_UI);

	const vec4 bg = { 0.0625f, 0.0625f, 0.0625f, 1.0f };
	const vec4 br = { 0.3f, 0.0f, 0.3f, 1.0f };
	const vec4 gr[BOX_CORNER_COUNT] = 
	{
		{0.3f, 0.0f, 0.8f, 0.7f },
		{0.8f, 0.0f, 0.3f, 0.7f },
		{0.8f, 0.0f, 0.3f, 0.7f },
		{0.3f, 0.0f, 0.8f, 0.7f },
	};
	const vec4 sp = { 0.9f, 0.9f, 0.9f, 1.0f };

	const f32 pad = 8.0f;
	const f32 edge_softness = 0.0f;
	const f32 corner_radius = 3.0f;
	const f32 border_size = 1.0f;	
	const f32 text_pad_x = 4.0f;
	const f32 text_pad_y = 4.0f;

	const struct ui_visual visual = ui_visual_init(bg, br, gr, sp, pad, edge_softness, corner_radius, border_size, FONT_DEFAULT_SMALL, ALIGN_X_CENTER, ALIGN_Y_CENTER, text_pad_x, text_pad_y);

	led_ui(led, &visual);

	if (led->project_menu.window)
	{
		led_project_menu_ui(led, &visual);
	}

	if (led->profiler.window)
	{
		led_profiler_ui(led, &visual);
	}

	KAS_END;
}
