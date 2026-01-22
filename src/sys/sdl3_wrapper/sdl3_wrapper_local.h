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

#ifndef __SDL3_WRAPPER_LOCAL_H__
#define __SDL3_WRAPPER_LOCAL_H__

#include "SDL3/SDL.h"

#undef log_write_message

#include "sys_common.h"
#include "sys_gl.h"
#include "sdl3_wrapper_public.h"

u32 			sdl3_wrapper_event_consume(struct system_event *event);

u32 			sdl3_wrapper_key_modifiers(void);
enum mouse_button	sdl3_wrapper_to_system_mouse_button(const u8 mouse_button);
enum ds_keycode	sdl3_wrapper_to_system_keycode(const SDL_Keycode sdl_key);
enum ds_keycode 	sdl3_wrapper_to_system_scancode(const SDL_Scancode sdl_key);

void 			sdl3_wrapper_gl_functions_init(struct gl_functions *func);

#endif
