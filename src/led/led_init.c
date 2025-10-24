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

struct led led_alloc(void)
{
	struct led editor = { 0 };
	editor.window = system_process_root_window_alloc("Level Editor", vec2u32_inline(400,400), vec2u32_inline(1280, 720));

	editor.project_menu = led_project_menu_alloc();
	editor.running = 1;
	editor.ns = time_ns();
	editor.root_folder = file_null();

	editor.csg = csg_alloc();

#if defined(KAS_PROFILER)
	editor.profiler.visible = 1;
#else
	editor.profiler.visible = 0;
#endif

	editor.project.initialized = 0;
	editor.project.folder = file_null();
	editor.project.file = file_null();

	struct system_window *sys_win = system_window_address(editor.window);
	enum fs_error err; 
	if ((err = directory_try_create_at_cwd(&sys_win->mem_persistent, &editor.root_folder, LED_ROOT_FOLDER_PATH)) != FS_SUCCESS)
	{
		if ((err = directory_try_open_at_cwd(&sys_win->mem_persistent, &editor.root_folder, LED_ROOT_FOLDER_PATH)) != FS_SUCCESS)
		{
			log_string(T_SYSTEM, S_FATAL, "Failed to open projects folder, exiting.");
			fatal_cleanup_and_exit(kas_thread_self_tid());
		}
	}
	
	editor.physics_viewport_id = utf8_format(&sys_win->mem_persistent, "physics_viewport_%u", editor.window);
	editor.csg_viewport_id = utf8_format(&sys_win->mem_persistent, "csg_viewport_%u", editor.window);

	return editor;
}

void led_dealloc(struct led *led)
{
	led_project_menu_dealloc(&led->project_menu);
	csg_dealloc(&led->csg);
}
