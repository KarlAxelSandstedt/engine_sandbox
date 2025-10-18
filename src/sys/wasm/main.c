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

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <float.h>
#include <emscripten/console.h>
#include <emscripten/wasm_worker.h>
#include <emscripten/threading.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/random.h>
#include <unistd.h>

#include "task.h"
#include "memory.h"
#include "timer.h"
#include "kas_string.h"
#include "sys_public.h"
#include "asset_public.h"
#include "kas_logger.h"
#include "wasm_local.h"
#include "r_public.h"
#include "game_public.h"
#include "widget.h"

struct arena global_arena;
struct render_state *r_state;
struct system_graphics *sys_graphics;
struct game game;
struct ui_state *ui;

static void main_loop(void)
{
	static u64 old_time = 0;
	const u64 new_time = time_ns();
	const u64 ns_tick = new_time - old_time;
	old_time = new_time;

	if (game.running)
	{
		//KAS_NEW_FRAME;

		//TODO
		//system_graphics_update(sys_graphics);
		
		game_process_system_events(&game, ui, sys_graphics);

		game_main(&game, ns_tick);

		r_main(r_state, sys_graphics, &game, ns_tick);
	}
	else
	{
		static u32 cleanup = 1;
		if (cleanup)
		{
			//KAS_PROFILER_SHUTDOWN;
			//KASPF_READER_SHUTDOWN;
			system_resources_cleanup();
			LOG_SHUTDOWN();
			arena_free(&global_arena);
			cleanup = 0;
		}
	}
}

int main(int argc, char *argv[])
{	
	/* init global memory (full program lifetime memory) */
	const u64 memsize = 4*1024*1024;
	global_arena = arena_alloc(memsize);

	filesystem_init_func_ptrs();

	const kas_string log_path = KAS_COMPILE_TIME_STRING("log.txt");
	LOG_INIT(&global_arena, &log_path);

	/* inititate system resources, timers, arch configuration ... */
	system_resources_init(&global_arena);

	//KASPF_READER_ALLOC(16*1024*1024);
	//KAS_PROFILER_INIT(&global_arena, 0, g_arch_config->logical_core_count, 4096, 1024, 144, 144, freq_rdtsc());
	
	task_context_init(&global_arena, g_arch_config->logical_core_count);

	asset_database_init(&global_arena);

	/* init system_graphics */
	char *title = "Unamed Game 1";
	const vec2u32 win_position = { 200, 200 };
	const vec2u32 win_size = { 1280, 720 };
	const u32 border_size = 0;
	sys_graphics = system_graphics_init(&global_arena, title, win_position, win_size, border_size);
	
	ui = ui_init(&global_arena, 2*4096);

	//const u64 renderer_framerate = 60;	
	//r_state = r_init(&global_arena, NSEC_PER_SEC / renderer_framerate, 2*1024*1024, 1024);
	r_state = r_init(&global_arena, 0, 2*1024*1024, 1024);
	
	const u64 game_framerate = 60;
	const u64 seed = 435340213;
	game = game_init(&global_arena, NSEC_PER_SEC / game_framerate, 2*1024*1024, seed);

	emscripten_set_main_loop(main_loop, 0, 1);

	return 0;
}
