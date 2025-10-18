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

#include "sdl3_wrapper_local.h"

struct native_window
{
	SDL_Window *	sdl_win;
	SDL_GLContext	gl_context;	
};

/* GLOBAL FUNCTION POINTERS */
struct native_window *	(*native_window_create)(struct arena *mem, const char *title, const vec2u32 position, const vec2u32 size);
void 			(*native_window_destroy)(struct native_window *native);

u64 			(*native_window_get_native_handle)(const struct native_window *native);

void 			(*native_window_gl_set_current)(struct native_window *native);
void 			(*native_window_gl_swap_buffers)(struct native_window *native);

void 			(*native_window_config_update)(vec2u32 position, vec2u32 size, struct native_window *native);
void 			(*native_window_fullscreen)(struct native_window *native);
void 			(*native_window_windowed)(struct native_window *native);
void 			(*native_window_bordered)(struct native_window *native);
void 			(*native_window_borderless)(struct native_window *native);
u32 			(*native_window_is_fullscreen)(const struct native_window *native);
u32 			(*native_window_is_bordered)(const struct native_window *native);

void			(*cursor_show)(struct native_window *native);
void			(*cursor_hide)(struct native_window *native);
void			(*cursor_grab)(struct native_window *native);
void			(*cursor_ungrab)(struct native_window *native);
u32 			(*cursor_is_locked)(struct native_window *native);
u32 			(*cursor_is_visible)(struct native_window *native);
u32 			(*cursor_lock)(struct native_window *native);
u32 			(*cursor_unlock)(struct native_window *native);

void 			(*screen_position_native_to_system)(vec2u32 sys_pos, struct native_window *native, const vec2u32 nat_pos);
void 			(*screen_position_system_to_native)(vec2u32 nat_pos, struct native_window *native, const vec2u32 sys_pos);
void 			(*window_position_native_to_system)(vec2u32 sys_pos, struct native_window *native, const vec2u32 nat_pos);
void 			(*window_position_system_to_native)(vec2u32 nat_pos, struct native_window *native, const vec2u32 sys_pos);

utf8 			(*utf8_get_clipboard)(struct arena *mem);
void 			(*cstr_set_clipboard)(const char *str);

u32 			(*system_enter_text_input_mode)(struct native_window *native);
u32 			(*system_exit_text_input_mode)(struct native_window *native);
u32 			(*system_key_modifiers)(void);

u32 			(*system_event_consume)(struct system_event *event);

void 			(*gl_functions_init)(struct gl_functions *func);

static void sdl3_wrapper_native_window_gl_set_current(struct native_window *native)
{
	if (!SDL_GL_MakeCurrent(native->sdl_win, native->gl_context))
	{
		log_string(T_RENDERER, S_ERROR, SDL_GetError());
	}	
}

static void sdl3_wrapper_native_window_gl_swap_buffers(struct native_window *native)
{
	if (!SDL_GL_SwapWindow(native->sdl_win))
	{
		log_string(T_RENDERER, S_WARNING, SDL_GetError());
	}
}

static u64 sdl3_wrapper_native_window_get_native_handle(const struct native_window *native)
{
	return (u64) native->sdl_win;
}

static void sdl3_wrapper_cursor_show(struct native_window *native)
{
	if (!SDL_ShowCursor())
	{
		log_string(T_SYSTEM, S_WARNING, SDL_GetError());
	}
}

static void sdl3_wrapper_cursor_hide(struct native_window *native)
{
	if (!SDL_HideCursor())
	{
		log_string(T_SYSTEM, S_WARNING, SDL_GetError());
	}
}

static void sdl3_wrapper_cursor_grab(struct native_window *native)
{
       fprintf(stderr, "implement %s\n", __func__);
}

static void sdl3_wrapper_cursor_ungrab(struct native_window *native)
{
       fprintf(stderr, "implement %s\n", __func__);
}

static u32 sdl3_wrapper_cursor_lock(struct native_window *native)
{
	u32 lock = 1;
	if (!SDL_SetWindowRelativeMouseMode(native->sdl_win, 1))
	{
		lock = 0;
		log_string(T_SYSTEM, S_WARNING, SDL_GetError());
	}

	return lock;
}

static u32 sdl3_wrapper_cursor_unlock(struct native_window *native)
{
	u32 lock = 0;
	if (!SDL_SetWindowRelativeMouseMode(native->sdl_win, 0))
	{
		lock = 1;
		log_string(T_SYSTEM, S_WARNING, SDL_GetError());
	}
	
	return lock;
}

static u32 sdl3_wrapper_cursor_is_visible(struct native_window *native)
{
	return (SDL_CursorVisible()) ? 1 : 0;
}

static u32 sdl3_wrapper_cursor_is_locked(struct native_window *native)
{
	return SDL_GetWindowRelativeMouseMode(native->sdl_win);
}

static void sdl3_wrapper_native_window_config_update(vec2u32 position, vec2u32 size, struct native_window *native)
{
	int w, h;
	if (!SDL_GetWindowSize(native->sdl_win, &w, &h))
	{
		log_string(T_SYSTEM, S_FATAL, SDL_GetError());
		fatal_cleanup_and_exit(0);
	}

	int x = (int) position[0];
       	int y = (int) position[1];
	if (!SDL_GetWindowPosition(native->sdl_win, &x, &y))
	{
		log_string(T_SYSTEM, S_WARNING, SDL_GetError());
	}

	size[0] = (u32) w;
	size[1] = (u32) h;
	position[0] = (u32) x;
	position[1] = (u32) y;
}

static void sdl3_wrapper_native_window_fullscreen(struct native_window *native)
{
	if (!SDL_SetWindowFullscreen(native->sdl_win, 1))
	{
		log_string(T_SYSTEM, S_WARNING, SDL_GetError());
	}
}

static void sdl3_wrapper_native_window_windowed(struct native_window *native)
{
	if (!SDL_SetWindowFullscreen(native->sdl_win, 0))
	{
		log_string(T_SYSTEM, S_WARNING, SDL_GetError());
	}
}

static void sdl3_wrapper_native_window_bordered(struct native_window *native)
{
	if (!SDL_SetWindowBordered(native->sdl_win, 1))
	{
		log_string(T_SYSTEM, S_WARNING, SDL_GetError());
	}
}

static void sdl3_wrapper_native_window_borderless(struct native_window *native)
{
	if (!SDL_SetWindowBordered(native->sdl_win, 0))
	{
		log_string(T_SYSTEM, S_WARNING, SDL_GetError());
	}
}

static u32 sdl3_wrapper_native_window_is_fullscreen(const struct native_window *native)
{
	return (SDL_GetWindowFlags(native->sdl_win) & SDL_WINDOW_FULLSCREEN) ? 1 : 0;
}

static u32 sdl3_wrapper_native_window_is_bordered(const struct native_window *native)
{
	return (SDL_GetWindowFlags(native->sdl_win) & SDL_WINDOW_BORDERLESS) ? 0 : 1;
}

void sdl3_wrapper_screen_position_native_to_system(vec2u32 sys_pos, struct native_window *native, const vec2u32 nat_pos)
{
       fprintf(stderr, "implement %s\n", __func__);
}

void sdl3_wrapper_screen_position_system_to_native(vec2u32 nat_pos, struct native_window *native, const vec2u32 sys_pos)
{
       fprintf(stderr, "implement %s\n", __func__);
}

void sdl3_wrapper_window_position_native_to_system(vec2u32 sys_pos, struct native_window *native, const vec2u32 nat_pos)
{
	int w, h;
	if (!SDL_GetWindowSize(native->sdl_win, &w, &h))
	{
		log_string(T_SYSTEM, S_FATAL, SDL_GetError());
		fatal_cleanup_and_exit(0);
	}

	sys_pos[0] = nat_pos[0];
	sys_pos[1] = (u32) h - 1 - nat_pos[1];

	sys_pos[0] = (sys_pos[0] < (u32) w) ? sys_pos[0] : (u32) w-1;
	sys_pos[1] = (sys_pos[1] < (u32) h) ? sys_pos[1] : (u32) h-1;
}

void sdl3_wrapper_window_position_system_to_native(vec2u32 nat_pos, struct native_window *native, const vec2u32 sys_pos)
{
	int w, h;
	if (!SDL_GetWindowSize(native->sdl_win, &w, &h))
	{
		log_string(T_SYSTEM, S_FATAL, SDL_GetError());
		fatal_cleanup_and_exit(0);
	}

	nat_pos[0] = sys_pos[0];
	nat_pos[1] = (u32) h - 1 - sys_pos[1];

	nat_pos[0] = (nat_pos[0] < (u32) w) ? nat_pos[0] : (u32) w-1;
	nat_pos[1] = (nat_pos[1] < (u32) h) ? nat_pos[1] : (u32) h-1;
}

static void sdl3_destroy_gl_context(struct native_window *native)
{
	if (!SDL_GL_DestroyContext(native->gl_context))
	{
		log_string(T_SYSTEM, S_FATAL, SDL_GetError());
		fatal_cleanup_and_exit(0);
	}
}

static void sdl3_create_gl_context(struct native_window *native)
{
	native->gl_context = SDL_GL_CreateContext(native->sdl_win);
	if (native->gl_context == NULL)
	{
		log_string(T_SYSTEM, S_FATAL, SDL_GetError());
		fatal_cleanup_and_exit(0);
	}

	/* turn off vsync for context (dont block on SWAP until window refresh (or something...) */
	if (!SDL_GL_SetSwapInterval(0))
	{
		log_string(T_SYSTEM, S_FATAL, SDL_GetError());
		fatal_cleanup_and_exit(0);
	}

	static u64 once = 1;
	if (once)
	{
		once = 0;
		SDL_GL_SetAttribute(SDL_GL_SHARE_WITH_CURRENT_CONTEXT, 1);
	}
}

static struct native_window *sdl3_wrapper_native_window_create(struct arena *mem, const char *title, const vec2u32 position, const vec2u32 size)
{
	struct native_window *native = arena_push(mem, sizeof(struct native_window));
	native->sdl_win = SDL_CreateWindow(title, (i32) size[0], (i32) size[1], SDL_WINDOW_RESIZABLE | SDL_WINDOW_OPENGL);
	if (native->sdl_win == NULL)
	{
		log_string(T_SYSTEM, S_FATAL, SDL_GetError());
		fatal_cleanup_and_exit(0);
	}

	sdl3_create_gl_context(native);
	return native;
}

static void sdl3_wrapper_native_window_destroy(struct native_window *native)
{
	sdl3_destroy_gl_context(native);
	SDL_DestroyWindow(native->sdl_win);
}

u32 sdl3_wrapper_enter_text_input_mode(struct native_window *native)
{
	u32 success = 1;
	if (!SDL_TextInputActive(native->sdl_win) && !SDL_StartTextInput(native->sdl_win))
	{
		log_string(T_SYSTEM, S_ERROR, SDL_GetError());
		success = 0;
	}

	return success;
}

u32 sdl3_wrapper_exit_text_input_mode(struct native_window *native)
{
	u32 success = 1;
	if (SDL_TextInputActive(native->sdl_win) && !SDL_StopTextInput(native->sdl_win))
	{
		log_string(T_SYSTEM, S_ERROR, SDL_GetError());
		success = 0;
	}

	return success;
}

utf8 sdl3_wrapper_utf8_get_clipboard(struct arena *mem)
{
	utf8 ret = utf8_empty();
	if (SDL_HasClipboardText())
	{	
		utf8 utf8_null =  { .buf = (u8*) SDL_GetClipboardText() };
		if (utf8_null.buf)
		{
			u32 len = 0;
			u64 size = 0;
			while (utf8_read_codepoint(&size, &utf8_null, size))
			{
				len += 1;
			}
			/* skip null */
			size -= 1;

			u8 *buf = arena_push_memcpy(mem, utf8_null.buf, size);
			if (buf)
			{
				ret = (utf8) { .buf = buf, .len = len, .size = (u32) size };
			}
			free (utf8_null.buf);
		}
		else
		{
			log_string(T_SYSTEM, S_ERROR, SDL_GetError());
		}
	}

	return ret;
}

void sdl3_wrapper_cstr_set_clipboard(const char *str)
{
	if (!SDL_SetClipboardText(str))
	{
		log_string(T_SYSTEM, S_ERROR, SDL_GetError());
	}
}

void sdl3_wrapper_init(void)
{
	if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS))
	{
		log_string(T_SYSTEM, S_FATAL, SDL_GetError());
		fatal_cleanup_and_exit(0);
	}

#if __OS__ == __LINUX__ || __OS__ == __WIN64__
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	if (!SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1)
		|| !SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3)
		|| !SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3)
		)
	{
		log_string(T_SYSTEM, S_FATAL, SDL_GetError());
		fatal_cleanup_and_exit(0);
	}

	i32 major, minor;
	SDL_GL_GetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, &major);
	SDL_GL_GetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, &minor);
	if (major < 3 || minor < 3)
	{
		log_string(T_SYSTEM, S_FATAL, "Requires GL 3.3 or greater, exiting\n");
		fatal_cleanup_and_exit(0);
	}
#elif __OS__ == __WEB__
	SDL_SetHint(SDL_HINT_OPENGL_ES_DRIVER, "1");
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
	if (!SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1)
		|| !SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3)
		|| !SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0)
		)
	{
		log_string(T_SYSTEM, S_FATAL, SDL_GetError());
		fatal_cleanup_and_exit(0);
	}

	i32 major, minor;
	SDL_GL_GetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, &major);
	SDL_GL_GetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, &minor);
	if (major < 3)
	{
		log_string(T_SYSTEM, S_FATAL, "Requires GLES 3.0 or greater, exiting\n");
		fatal_cleanup_and_exit(0);
	}
#endif
	/* Must be done after initalizing the video driver but before creating any opengl windows */
	if (!SDL_GL_LoadLibrary(NULL))
	{
		log_string(T_SYSTEM, S_FATAL, SDL_GetError());
		fatal_cleanup_and_exit(0);
	}
	native_window_create = &sdl3_wrapper_native_window_create;
	native_window_destroy = &sdl3_wrapper_native_window_destroy;
	native_window_get_native_handle = &sdl3_wrapper_native_window_get_native_handle;
	native_window_gl_set_current = sdl3_wrapper_native_window_gl_set_current;
	native_window_gl_swap_buffers = &sdl3_wrapper_native_window_gl_swap_buffers;
	native_window_config_update = &sdl3_wrapper_native_window_config_update;
	native_window_fullscreen = &sdl3_wrapper_native_window_fullscreen;
	native_window_windowed = &sdl3_wrapper_native_window_windowed;
	native_window_bordered = &sdl3_wrapper_native_window_bordered;
	native_window_borderless = &sdl3_wrapper_native_window_borderless;
	native_window_is_fullscreen = &sdl3_wrapper_native_window_is_fullscreen;
	native_window_is_bordered = &sdl3_wrapper_native_window_is_bordered;

	cursor_show = &sdl3_wrapper_cursor_show;
	cursor_hide = &sdl3_wrapper_cursor_hide;
	cursor_grab = &sdl3_wrapper_cursor_grab;
	cursor_ungrab = &sdl3_wrapper_cursor_ungrab;
	cursor_is_visible = &sdl3_wrapper_cursor_is_visible;
	cursor_is_locked = &sdl3_wrapper_cursor_is_locked;
	cursor_lock = &sdl3_wrapper_cursor_lock;
	cursor_unlock = &sdl3_wrapper_cursor_unlock;

	screen_position_native_to_system = &sdl3_wrapper_screen_position_native_to_system;
	screen_position_system_to_native = &sdl3_wrapper_screen_position_system_to_native;
	window_position_native_to_system = &sdl3_wrapper_window_position_native_to_system;
	window_position_system_to_native = &sdl3_wrapper_window_position_system_to_native;

	utf8_get_clipboard = sdl3_wrapper_utf8_get_clipboard;
	cstr_set_clipboard = sdl3_wrapper_cstr_set_clipboard;

	system_enter_text_input_mode = &sdl3_wrapper_enter_text_input_mode;
	system_exit_text_input_mode = &sdl3_wrapper_exit_text_input_mode;
	system_key_modifiers = &sdl3_wrapper_key_modifiers;

	system_event_consume = &sdl3_wrapper_event_consume;

	gl_functions_init = &sdl3_wrapper_gl_functions_init;
}
