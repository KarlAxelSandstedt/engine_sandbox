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
#include "led_local.h"

struct led g_editor_storage = { 0 };
struct led *g_editor = &g_editor_storage;

struct led_project_menu	led_project_menu_alloc(void)
{
	struct led_project_menu menu =
	{
		.projects_folder_allocated = 0,
		.projects_folder_refresh = 0,
		.selected_path = utf8_empty(),
		.dir_nav = directory_navigator_alloc(4096, 64, 64),
		.dir_list = ui_list_init(AXIS_2_Y, 200.0f, 24.0f),
		.window = HI_NULL_INDEX,
		.popup_new_project = ui_popup_null(),
		.utf8_new_project = utf8_empty(),
		.input_line_new_project = ui_input_line_empty(),
	};

	return menu;	
}

void led_project_menu_dealloc(struct led_project_menu *menu)
{
	directory_navigator_dealloc(&menu->dir_nav);
}

struct led *led_alloc(void)
{
	led_core_init_commands();

	g_editor->window = system_process_root_window_alloc("Level Editor", vec2u32_inline(400,400), vec2u32_inline(1280, 720));

	g_editor->frame = arena_alloc(1024*1024*1024);
	g_editor->project_menu = led_project_menu_alloc();
	g_editor->running = 1;
	g_editor->ns = time_ns();
	g_editor->root_folder = file_null();

#if defined(KAS_PROFILER)
	g_editor->profiler.visible = 1;
#else
	g_editor->profiler.visible = 0;
#endif

	g_editor->project.initialized = 0;
	g_editor->project.folder = file_null();
	g_editor->project.file = file_null();

	struct system_window *sys_win = system_window_address(g_editor->window);
	enum fs_error err; 
	if ((err = directory_try_create_at_cwd(&sys_win->mem_persistent, &g_editor->root_folder, LED_ROOT_FOLDER_PATH)) != FS_SUCCESS)
	{
		if ((err = directory_try_open_at_cwd(&sys_win->mem_persistent, &g_editor->root_folder, LED_ROOT_FOLDER_PATH)) != FS_SUCCESS)
		{
			log_string(T_SYSTEM, S_FATAL, "Failed to open projects folder, exiting.");
			fatal_cleanup_and_exit(kas_thread_self_tid());
		}
	}
	
	g_editor->viewport_id = utf8_format(&sys_win->mem_persistent, "viewport_%u", g_editor->window);
	g_editor->node_pool = gpool_alloc(NULL, 4096, struct led_node, GROWABLE);
	g_editor->node_map = hash_map_alloc(NULL, 4096, 4096, GROWABLE);
	g_editor->node_marked_list = dll_init(struct led_node);
	g_editor->node_non_marked_list = dll_init(struct led_node);
	g_editor->node_selected_list = dll2_init(struct led_node);
	g_editor->csg = csg_alloc();

	return g_editor;
}

void led_dealloc(struct led *led)
{
	led_project_menu_dealloc(&led->project_menu);
	csg_dealloc(&led->csg);
	gpool_dealloc(&led->node_pool);
	arena_free(&g_editor->frame);
}
