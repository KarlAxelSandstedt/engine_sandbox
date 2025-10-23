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

#ifndef __LED_PUBLIC_H__
#define __LED_PUBLIC_H__

#include "kas_common.h"
#include "kas_string.h"
#include "sys_public.h"
#include "allocator.h"
#include "csg.h"

/*******************************************/
/*                 led_init.c              */
/*******************************************/

/* project navigation menu */
struct led_project_menu
{
	u32		window;

	utf8	selected_path;		/* selected path in menu, or empty string */

	u32		projects_folder_allocated; /* Boolean : Is directory contents allocated */
	u32		projects_folder_refresh; /* Boolean : on main entry, refresh projects folder contents */

	struct directory_navigator	dir_nav;
	struct ui_list			dir_list;
	
	struct ui_popup		popup_new_project;
	struct ui_popup		popup_new_project_extra;
	utf8			utf8_new_project;
	struct ui_input_line 	input_line_new_project;
};

struct led_profiler
{
	u32			window;
	u32			visible;

	f32			transparency;
	vec4			system_colors[T_COUNT];

	u32			draw_worker_activity_online;

	struct timeline_config 	timeline_config;
};

struct led_project
{
	u32			initialized;	/* is project setup/loaded and initialized? 	*/	
	struct file		folder;		/* project folder 				*/
	struct file		file;		/* project main file 				*/
};

/* Level editor main struct */
struct led
{
	u32			window;
	struct file		root_folder;

	struct led_project	project;
	struct led_project_menu project_menu;

	struct led_profiler	profiler;
		
	u64			ns;		/* current time in ns */
	u32			running;

	struct csg 		csg;
	struct ui_list 		brush_list;
};

/* Allocate initial led resources */
struct led 	led_alloc(void);
/* deallocate led resources */
void		led_dealloc(struct led *led);

/*******************************************/
/*                 led_utility.c              */
/*******************************************/

u32		led_filename_valid(const utf8 filename);

/*******************************************/
/*                 led_main.c              */
/*******************************************/

/* level editor entrypoint; handle ui interactions and update led state */
void		led_main(struct led *led, const u64 ns_delta);

/*******************************************/
/*                 led_ui.c                */
/*******************************************/

/* level editor ui entrypoint */
void 		led_ui_main(struct led *led);

#endif
