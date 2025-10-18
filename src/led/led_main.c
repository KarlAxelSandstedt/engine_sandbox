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

static void led_project_menu_window_interaction(struct led *led)
{
}

void led_project_menu_main(struct led *led)
{
	struct led_project_menu *menu = &led->project_menu;
	struct system_window *sys_win;
	if (menu->window == HI_NULL_INDEX)
	{
		menu->window = system_window_alloc("Project Menu", vec2u32_inline(0,0), vec2u32_inline(400, 400), g_process_root_window);
		menu->popup_new_project = ui_popup_null();

		struct system_window *sys_win = system_window_address(menu->window);
		menu->input_line_new_project = ui_input_line_alloc(&sys_win->mem_persistent, 32);
		menu->utf8_new_project = utf8_alloc(&sys_win->mem_persistent, 32*sizeof(u32));
	}

	sys_win = system_window_address(menu->window);
	if (menu->window != HI_NULL_INDEX && sys_win->tagged_for_destruction)
	{
		menu->window = HI_NULL_INDEX;
		menu->input_line_new_project = ui_input_line_empty();
	}

	if (menu->projects_folder_refresh || !menu->projects_folder_allocated)
	{
		enum fs_error ret = directory_navigator_enter_and_alias_path(&menu->dir_nav, led->root_folder.path);
		if (ret == FS_SUCCESS)
		{
			menu->projects_folder_allocated = 1;
			menu->projects_folder_refresh = 0;
		}
		else if (ret == FS_PATH_INVALID)
		{
			log(T_SYSTEM, S_ERROR, "Could not enter folder %k, bad path.", &led->root_folder.path);
		}
		else
		{
			log(T_SYSTEM, S_ERROR, "Unhandled error when entering folder %k.", &led->root_folder.path);
		}
	}

	if (led->project.initialized)
	{
		struct system_window *win = system_window_address(menu->window);
		win->tagged_for_destruction = 1;
		menu->window = HI_NULL_INDEX;
	}
}
static void led_project_main(struct led *led)
{
	struct system_window *win = system_window_address(led->window);
	if (win->tagged_for_destruction) 
	{
		led->running = 0;
	}

	if (win->ui->inter.key_pressed[KAS_P] && system_user_is_admin())
	{
		led->profiler.visible = 1;
	}
}

static void led_profiler_interaction(struct led *led)
{
	struct led_profiler *prof = &led->profiler;
	struct system_window *win = system_window_address(prof->window);
	if (win->tagged_for_destruction)
	{
		prof->visible = 0;
		prof->window = HI_NULL_INDEX;
	}
}

static void led_profiler_main(struct led *led)
{
	struct led_profiler *prof = &led->profiler;

	if (prof->window == HI_NULL_INDEX)
	{
		prof->window = system_window_alloc("Profiler", vec2u32_inline(0,0), vec2u32_inline(1280, 720), g_process_root_window);
		struct system_window *win = system_window_address(prof->window);

		prof->transparency = 0.7f;
		vec4_set(prof->system_colors[T_GAME],  34.0f/256.0f, 278.0f/256.0f,  144.0f/256.0f, prof->transparency);
		vec4_set(prof->system_colors[T_UTILITY],  84.0f/256.0f, 178.0f/256.0f,  84.0f/256.0f, prof->transparency);
		vec4_set(prof->system_colors[T_PHYSICS], 80.0f/256.0f, 120.0f/256.0f, 210.0f/256.0f, prof->transparency);
		vec4_set(prof->system_colors[T_RENDERER], 204.0f/256.0f, 48.0f/256.0f, 64.0f/256.0f, prof->transparency);
		vec4_set(prof->system_colors[T_UI], 194.0f/256.0f, 68.0f/256.0f, 191.0f/256.0f, prof->transparency);
		vec4_set(prof->system_colors[T_PROFILER], 235.0f/256.0f, 155.0f/256.0f, 74.0f/256.0f, prof->transparency);
		vec4_set(prof->system_colors[T_ASSET], 35.0f/256.0f, 155.0f/256.0f, 74.0f/256.0f, prof->transparency);

	  	prof->draw_worker_activity_online = 0;
	  
	  	prof->timeline_config.unit_line_width = 2.0f;
	  	prof->timeline_config.subline_width = 1.0f;
	  	prof->timeline_config.sublines_per_line = 4;
	  	prof->timeline_config.unit_line_preferred_count = 5;

		vec4_set(prof->timeline_config.task_gradient_br, 0.1225f, 0.1225f, 0.4225f, 0.5f);
		vec4_set(prof->timeline_config.task_gradient_tr, 0.0f, 0.0f, 0.0f, 0.0f);
		vec4_set(prof->timeline_config.task_gradient_tl, 0.0f, 0.0f, 0.0f, 0.0f);
		vec4_set(prof->timeline_config.task_gradient_bl, 0.1225f, 0.1225f, 0.4225f, 0.5f);
	  
	  	prof->timeline_config.draw_sublines = 1;
	  	prof->timeline_config.draw_edgelines = 1;
		prof->timeline_config.perc_width_row_title_column = 0.125f;
	  
	  	vec4_set(prof->timeline_config.unit_line_color, 0.50f, 0.50f, 0.50f, 0.2f);
	  	vec4_set(prof->timeline_config.subline_color, 0.3f, 0.3f, 0.3f, 0.2f);
	  	vec4_set(prof->timeline_config.text_color, 0.8f, 0.8f, 0.8f, 0.6f);
	  	vec4_set(prof->timeline_config.background_color, 0.0625f, 0.0625f, 0.0750f, 1.0f);
	  	vec4_set(prof->timeline_config.draggable_color, 0.0625f, 0.125f, 0.125f, 1.0f);

		prof->timeline_config.fixed = 0;

		prof->timeline_config.task_height = 32.0f;
		prof->timeline_config.row_count = g_task_ctx->worker_count;
		prof->timeline_config.row = arena_push(&win->mem_persistent, g_task_ctx->worker_count*sizeof(struct timeline_row_config));
		for (u32 i = 0; i < g_task_ctx->worker_count; ++i)
		{
			prof->timeline_config.row[i].height = prof->timeline_config.task_height;
			prof->timeline_config.row[i].depth_visible = intv_inline(0.0f, prof->timeline_config.row[i].height / prof->timeline_config.task_height);
		}
	}

	led_profiler_interaction(led);
}

void led_main(struct led *led, const u64 ns_delta)
{
	led->ns += ns_delta;
	led_project_main(led);

	if (!led->project.initialized)
	{
		led_project_menu_main(led);		
	}

	if (led->profiler.visible)
	{
		led_profiler_main(led);
	}
}
