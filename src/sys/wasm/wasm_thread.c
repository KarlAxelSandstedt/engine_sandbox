/*
==========================================================================
    Copyright (C) 2025,2026 Axel Sandstedt 

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

#define _GNU_SOURCE
#include <sched.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/mman.h>

#include "wasm_local.h"
#include "sys_public.h"

dsThreadLocal struct ds_thread *self = NULL;
u32	a_index_counter = 1;

const char *thread_profiler_id[] = 
{
	"Master", "Worker 1", "Worker 2", "Worker 3", "Worker 4", "Worker 5", "Worker 6", "Worker 7", "Worker 8", "Worker 9",
	"Worker 10", "Worker 11", "Worker 12", "Worker 13", "Worker 14", "Worker 15", "Worker 16", "Worker 17", "Worker 18", "Worker 19",
	"Worker 20", "Worker 21", "Worker 22", "Worker 23", "Worker 24", "Worker 25", "Worker 26", "Worker 27", "Worker 28", "Worker 29",
	"Worker 30", "Worker 31", "Worker 32", "Worker 33", "Worker 34", "Worker 35", "Worker 36", "Worker 37", "Worker 38", "Worker 39",
	"Worker 40", "Worker 41", "Worker 42", "Worker 43", "Worker 44", "Worker 45", "Worker 46", "Worker 47", "Worker 48", "Worker 49",
	"Worker 50", "Worker 51", "Worker 52", "Worker 53", "Worker 54", "Worker 55", "Worker 56", "Worker 57", "Worker 58", "Worker 59",
	"Worker 60", "Worker 61", "Worker 62", "Worker 63",
};

static void *ds_thread_clone_start(void *void_thr)
{
	self = void_thr;
	struct ds_thread *thr = void_thr;
	thr->tid = gettid();
	thr->index = AtomicFetchAddRlx32(&a_index_counter, 1);
	PROF_THREAD_NAMED(thread_profiler_id[thr->index]);
	thr->start(thr);

	return NULL;
}

void ds_thread_master_init(struct arena *mem)
{
	self = arena_push(mem, sizeof(struct ds_thread));
	self->tid = gettid();
	self->index = 0;
	PROF_THREAD_NAMED(thread_profiler_id[self->index]);
}

void ds_thread_clone(struct arena *mem, void (*start)(ds_thread *), void *args, const u64 stack_size)
{
	ds_Assert(stack_size > 0);

	const u64 thr_size = (sizeof(ds_thread) % g_arch_config->cacheline == 0)
		? sizeof(ds_thread)
		: sizeof(ds_thread) + g_arch_config->cacheline - (sizeof(ds_thread) % g_arch_config->cacheline);

	ds_thread *thr = NULL;
	if (mem)
	{
		thr = arena_push_aligned(mem, thr_size, g_arch_config->cacheline);
	}
	else
	{
		memory_alloc_aligned(&thr, g_arch_config->cacheline, thr_size);
	}

	if (thr == NULL)
	{
		log_string(T_SYSTEM, S_FATAL, "Failed to alloc thread memory, aborting.");
		fatal_cleanup_and_exit(gettid());
	}

	ds_Assert((u64) thr % g_arch_config->cacheline == 0);

	thr->start = start;
	thr->args = args;
	thr->ret = NULL;
	thr->ret_size = 0;
	thr->stack_size = (stack_size % g_arch_config->pagesize == 0) 
				? stack_size 
				: stack_size + (g_arch_config->pagesize - stack_size % g_arch_config->pagesize);

	pthread_attr_t attr;
	if (pthread_attr_init(&attr) != 0)
	{
		LOG_SYSTEM_ERROR(S_FATAL);	
		fatal_cleanup_and_exit(gettid());
	}

	if (pthread_attr_setstacksize(&attr, thr->stack_size) != 0)
	{
		LOG_SYSTEM_ERROR(S_FATAL);	
		fatal_cleanup_and_exit(gettid());
	}

	size_t real_size;
	pthread_attr_getstacksize(&attr, &real_size);
	ds_Assert(real_size == thr->stack_size);

	if (pthread_create(&thr->pthread, &attr, ds_thread_clone_start, thr) != 0)
	{
		LOG_SYSTEM_ERROR(S_FATAL);	
		fatal_cleanup_and_exit(gettid());
	}

	if (pthread_attr_destroy(&attr) != 0)
	{
		LOG_SYSTEM_ERROR(S_FATAL);	
		fatal_cleanup_and_exit(gettid());
	}
}

void ds_thread_exit(ds_thread *thr)
{
	self = NULL;
	pthread_exit(0);
}

void ds_thread_wait(const ds_thread *thr)
{
	void *garbage;

	i32 status = pthread_join(thr->pthread, &garbage);
	if (status != 0)
	{
		log_string(T_SYSTEM, S_FATAL, "Failed to alloc thread memory, aborting.");
		fatal_cleanup_and_exit(gettid());
	}
}

void ds_thread_release(ds_thread *thr)
{
	return;
}

void *ds_thread_ret_value(const ds_thread *thr)
{
	return thr->ret;	
}

void *ds_thread_args(const ds_thread *thr)
{
	return thr->args;
}

u64 ds_thread_ret_value_size(const ds_thread *thr)
{
	return thr->ret_size;
}

tid ds_thread_tid(const ds_thread *thr)
{
	return thr->tid;
}

tid ds_thread_self_tid(void)
{
	return self->tid;
}

u32 ds_thread_index(const ds_thread *thr)
{
	return thr->index;
}

u32 ds_thread_self_index(void)
{
	return self->index;
}
