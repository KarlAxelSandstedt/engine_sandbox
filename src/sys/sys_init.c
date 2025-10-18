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

#include "sys_local.h"
#include "kas_profiler.h"
#include "log.h"
#include "dtoa.h"

static struct kas_sys_env g_sys_env_storage = { 0 };
struct kas_sys_env *g_sys_env = &g_sys_env_storage;

void kas_sys_env_init(struct arena *mem)
{
	g_sys_env->user_privileged = system_user_is_admin();	
	g_sys_env->cwd = file_null();
	if (cwd_set(mem, ".") != FS_SUCCESS)
	{
		log_string(T_SYSTEM, S_FATAL, "Failed to open the current working directory");
		fatal_cleanup_and_exit(0);
	}
}

void system_resources_init(struct arena *mem)
{
	init_error_handling_func_ptrs();
	filesystem_init_func_ptrs();

 	kas_sys_env_init(mem);
	kas_thread_master_init(mem);
	time_init();
	log_init(mem, "log.txt");

	if (!kas_arch_config_init(mem))
	{
		log_string(T_SYSTEM, S_FATAL, "Unsupported instrincs required");
		fatal_cleanup_and_exit(0);
	}

	/* must initalize stuff in multithreaded dtoa/strtod */
	dmg_dtoa_init(g_arch_config->logical_core_count);

	const u32 count_256B = U16_MAX;
	const u32 count_1MB = 256;

#if __OS__ != __WEB__
	log(T_SYSTEM, S_NOTE, "clock resolution (us): %3f", (f64) time_ns_per_tick() / 1000.0);
	log(T_SYSTEM, S_NOTE, "rdtsc estimated frequency (GHz): %3f", (f32) freq_rdtsc() / 1000000000);
#endif

	global_thread_block_allocators_alloc(count_256B, count_1MB);
	KASPF_READER_ALLOC(16*1024*1024);
	KAS_PROFILER_INIT(mem, 0, g_arch_config->logical_core_count, 4*4096, 1024, freq_rdtsc());
	task_context_init(mem, g_arch_config->logical_core_count);
}

void system_resources_cleanup(void)
{
	task_context_destroy(g_task_ctx);
	KAS_PROFILER_SHUTDOWN;
	KASPF_READER_SHUTDOWN;

	global_thread_block_allocators_free();

	log_shutdown();
}
