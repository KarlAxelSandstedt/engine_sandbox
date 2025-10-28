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

#include <stdlib.h>
#include <string.h>

#include "sys_local.h"
#include "r_public.h"

struct hierarchy_index *g_window_hierarchy = NULL;
u32 g_window = HI_NULL_INDEX;
u32 g_process_root_window = HI_NULL_INDEX;

static void system_window_free_resources(struct system_window *sys_win)
{
	gl_state_free(sys_win->gl_state);
	ui_dealloc(sys_win->ui);
	r_scene_free(sys_win->r_scene);
	cmd_queue_free(sys_win->cmd_queue);
	arena_free_1MB(&sys_win->mem_persistent);
	native_window_destroy(sys_win->native);
}

u32 system_window_alloc(const char *title, const vec2u32 position, const vec2u32 size, const u32 parent)
{
	struct slot slot = hierarchy_index_add(g_window_hierarchy, parent);
	kas_assert(parent != HI_ROOT_STUB_INDEX || slot.index == 2);

	struct system_window *sys_win = slot.address;

	sys_win->mem_persistent = arena_alloc_1MB();
	sys_win->native = native_window_create(&sys_win->mem_persistent, (const char *) title, position, size);

	sys_win->ui = ui_alloc();
	sys_win->r_scene = r_scene_alloc();
	sys_win->cmd_queue = cmd_queue_alloc();
	sys_win->cmd_console = arena_push_zero(&sys_win->mem_persistent, sizeof(struct cmd_console));
	sys_win->cmd_console->prompt = ui_text_input_alloc(&sys_win->mem_persistent, 256);
	sys_win->tagged_for_destruction = 0;
	sys_win->text_input_mode = 0;
	
	if (slot.index == 2)
	{
		/* root window */
		native_window_gl_set_current(sys_win->native);
		sys_win->gl_state = gl_state_alloc();
		gl_state_set_current(sys_win->gl_state);
	}
	else
	{
		/* set context before we initalize gl function pointers ***POSSIBLY*** Local to the new context on 
		 * som platforms */
		native_window_gl_set_current(sys_win->native);
		sys_win->gl_state = gl_state_alloc();

		struct system_window *root = hierarchy_index_address(g_window_hierarchy, g_process_root_window);
		native_window_gl_set_current(root->native);
	}

	system_window_config_update(slot.index);

	return slot.index;
}

void system_window_tag_sub_hierarchy_for_destruction(const u32 root)
{
	struct arena tmp = arena_alloc_1MB();
	struct hierarchy_index_iterator	it = hierarchy_index_iterator_init(&tmp, g_window_hierarchy, root);
	while (it.count)
	{
		const u32 index = hierarchy_index_iterator_next_df(&it);
		struct system_window *sys_win = hierarchy_index_address(g_window_hierarchy, index);
		sys_win->tagged_for_destruction = 1;
	}
	hierarchy_index_iterator_release(&it);
	arena_free_1MB(&tmp);
}



static void func_system_window_free(const struct hierarchy_index *hi, const u32 index, void *data)
{
	struct system_window *win = hierarchy_index_address(hi, index);
	system_window_free_resources(win);
}

void system_free_tagged_windows(void)
{
	struct arena tmp1 = arena_alloc_1MB();
	struct arena tmp2 = arena_alloc_1MB();
	struct hierarchy_index_iterator	it = hierarchy_index_iterator_init(&tmp1, g_window_hierarchy, g_process_root_window);
	while (it.count)
	{
		const u32 index = hierarchy_index_iterator_peek(&it);
		struct system_window *sys_win = hierarchy_index_address(g_window_hierarchy, index);
		if (sys_win->tagged_for_destruction)
		{
			hierarchy_index_iterator_skip(&it);
			hierarchy_index_apply_custom_free_and_remove(&tmp2, g_window_hierarchy, index, func_system_window_free, NULL);

		}
		else
		{
			hierarchy_index_iterator_next_df(&it);
		}
	}
	hierarchy_index_iterator_release(&it);
	arena_free_1MB(&tmp1);
	arena_free_1MB(&tmp2);
}

struct slot system_window_lookup(const u64 native_handle)
{
	struct system_window *win = NULL;
	u32 index = U32_MAX;

	struct arena tmp = arena_alloc_1MB();
	struct hierarchy_index_iterator	it = hierarchy_index_iterator_init(&tmp, g_window_hierarchy, g_process_root_window);
	while (it.count)
	{
		const u32 win_index = hierarchy_index_iterator_next_df(&it);
		struct system_window *sys_win = hierarchy_index_address(g_window_hierarchy, win_index);
		if (native_window_get_native_handle(sys_win->native) == native_handle)
		{
			win = sys_win;
			index = win_index;
			break;
		}
	}

	hierarchy_index_iterator_release(&it);
	arena_free_1MB(&tmp);

	return (struct slot) { .index = index, .address = win };
}

u32 system_process_root_window_alloc(const char *title, const vec2u32 position, const vec2u32 size)
{
	kas_assert(g_process_root_window == HI_NULL_INDEX);
	g_process_root_window = system_window_alloc(title, position, size, HI_ROOT_STUB_INDEX);
	kas_assert(g_process_root_window == 2);
	return g_process_root_window;
}

void system_window_config_update(const u32 window)
{
	struct system_window *sys_win = hierarchy_index_address(g_window_hierarchy, window);
	native_window_config_update(sys_win->position, sys_win->size, sys_win->native);
}

void system_window_size(vec2u32 size, const u32 window)
{
	struct system_window *sys_win = hierarchy_index_address(g_window_hierarchy, window);
	size[0] = sys_win->size[0];
	size[1] = sys_win->size[1];
}

struct system_window *system_window_address(const u32 index)
{
	return hierarchy_index_address(g_window_hierarchy, index);
}

void system_window_set_current_gl_context(const u32 window)
{
	struct system_window *sys_win = system_window_address(window);
	native_window_gl_set_current(sys_win->native);
	gl_state_set_current(sys_win->gl_state);
}

void system_window_swap_gl_buffers(const u32 window)
{
	struct system_window *sys_win = system_window_address(window);
	native_window_gl_swap_buffers(sys_win->native);
}

void system_window_set_global(const u32 index)
{
	g_window = index;
	struct system_window *sys_win = hierarchy_index_address(g_window_hierarchy, index);
	ui_set(sys_win->ui);
	cmd_queue_set(sys_win->cmd_queue);
}

void system_graphics_init(void)
{
#if __GAPI__ == __X11__	
#elif __GAPI__ == __WAYLAND__
#elif __GAPI__ == __SDL3__
	sdl3_wrapper_init();
#elif __GAPI__ == __WIN64__
#endif
	g_window_hierarchy = hierarchy_index_alloc(NULL, 8, sizeof(struct system_window), ARRAY_LIST_GROWABLE);
	
	gl_state_list_alloc();
}

void system_graphics_destroy(void)
{
	struct arena tmp = arena_alloc_1MB();
	hierarchy_index_apply_custom_free_and_remove(&tmp, g_window_hierarchy, g_process_root_window, func_system_window_free, NULL);
	arena_free_1MB(&tmp);

	gl_state_list_free();
	hierarchy_index_free(g_window_hierarchy);
}

void system_window_text_input_mode_enable(void)
{
	struct system_window *sys_win = hierarchy_index_address(g_window_hierarchy, g_window);
	if (system_enter_text_input_mode(sys_win->native))
	{
		sys_win->text_input_mode = 1;
	}
	else
	{
		sys_win->text_input_mode = 0;
	}
}

void system_window_text_input_mode_disable(void)
{
	struct system_window *sys_win = hierarchy_index_address(g_window_hierarchy, g_window);
	if (system_exit_text_input_mode(sys_win->native))
	{
		sys_win->text_input_mode = 0;
	}
	else
	{
		sys_win->text_input_mode = 1;
	}
}
