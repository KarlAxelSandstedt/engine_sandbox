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

#ifndef __LED_LOCAL_H__
#define __LED_LOCAL_H__

#include "led_public.h"
#include "sys_public.h"
#include "r_public.h"
#include "kas_profiler.h"
#include "ui_public.h"

#define LED_ROOT_FOLDER_PATH	"../asset_components/led_projects"

/*******************************************/
/*                 led_visual.c            */
/*******************************************/

struct led_visual
{
	//TODO seperate cameras for seperate modes 
	struct r_camera	cam;	

	/* general visual aspects */

	vec4		unit_grid_color;
	f32		unit_grid_equidistance;
	u32		unit_grid_lines_per_axis;
	u32		unit_grid_draw;			/* Boolean */
	u32		unit_r_handle;

	u32		axes_draw;
	u32		axes_r_handle;

	vec4		border_color;
	vec4		background_color;
	vec4		background_highlight_color;
	vec4		background_invalid_color;
	vec4		text_color;
	i32		border_size;
	f32		edge_softness;
	f32		corner_radius;

	struct
	{
		u32 tmp;
	} menu;
};

extern struct led_visual *	g_visual;

/* init led visual defaults */
void 	led_visual_init_defaults(const u32 window);


/*******************************************/
/*                 led_init.c              */
/*******************************************/

/* Allocate initial project menu resources */
struct led_project_menu	led_project_menu_alloc(void);
/* release project menu resources */
void			led_project_menu_dealloc(struct led_project_menu *menu);

/*******************************************/
/*                 led_main.c              */
/*******************************************/

/* main entrypoint */
void			led_project_menu_main(struct led *led);

/*******************************************/
/*                 led_ui.c                */
/*******************************************/

/*******************************************/
/*                 led_core.c              */
/*******************************************/

/* TODO: tmp, until we implenent cvars or something */
extern struct led *g_editor;

/* initate global commands and identifers */
void 		led_core_init_commands(void);
/* run level editor systems */
void 		led_core(struct led *led);

/* Allocate node with the given id. Returns (NULL, U32_MAX) if id.size > 256B or id.len == 0 */
struct slot 	led_node_add(struct led *led, const utf8 id);
/* Mark node for remval if it exist; otherwise no-op.  */
void 		led_node_remove(struct led *led, const utf8 id);
/* Return node with the given id if it exist; otherwise return (NULL, U32_MAX).  */
struct slot 	led_node_lookup(struct led *led, const utf8 id);

/* Allocate node with the given id. Returns (NULL, U32_MAX) if id.size > 256B or bad shape paramters */
struct slot	led_collision_shape_add(struct led *led, const struct collision_shape *shape);
/* Remove node if it exists and is not being referenced; otherwise no-op.  */
void 		led_collision_shape_remove(struct led *led, const utf8 id);
/* Return node with the given id if it exist; otherwise return (NULL, U32_MAX).  */
struct slot 	led_collision_shape_lookup(struct led *led, const utf8 id);

/* Allocate prefab with the given id. Returns (NULL, U32_MAX) if id.size > 256B or bad shape identifier */
struct slot 	led_rigid_body_prefab_add(struct led *led, const utf8 id, const utf8 shape, const f32 density, const f32 restitution, const f32 friction, const u32 dynamic);
/* Remove prefab if it exists and is not being referenced; otherwise no-op.  */
void 		led_rigid_body_prefab_remove(struct led *led, const utf8 id);
/* Return prefab with the given id if it exist; otherwise return (NULL, U32_MAX).  */
struct slot 	led_rigid_body_prefab_lookup(struct led *led, const utf8 id);

/* command identifiers */
extern u32 	cmd_led_node_add_id;
extern u32 	cmd_led_node_remove_id;

extern u32 	cmd_rigid_body_prefab_add_id;
extern u32 	cmd_rigid_body_prefab_remove_id;

extern u32 	cmd_collision_shape_add_id;
extern u32 	cmd_collision_shape_remove_id;
extern u32 	cmd_collision_box_add_id;
extern u32 	cmd_collision_sphere_add_id;
extern u32 	cmd_collision_capsule_add_id;

#endif
