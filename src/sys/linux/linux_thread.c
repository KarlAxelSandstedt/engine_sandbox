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

#define _GNU_SOURCE
#include <sched.h>
#include <linux/futex.h> 
#include <sys/syscall.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/mman.h>

#include "linux_local.h"
#include "sys_public.h"

kas_thread_local struct kas_thread *self = NULL;

static void *kas_thread_clone_start(void *void_thr)
{
	self = void_thr;
	struct kas_thread *thr = void_thr;
	thr->ppid = getppid();
	thr->gtid = getpid();
	thr->tid = gettid();
	thr->start(thr);

	return NULL;
}

void kas_thread_master_init(struct arena *mem)
{
	self = arena_push(mem, sizeof(struct kas_thread));
	self->ppid = getppid();
	self->gtid = getpid();
	self->tid = gettid();
}

void kas_thread_clone(struct arena *mem, void (*start)(kas_thread *), void *args, const u64 stack_size)
{
	kas_assert(stack_size > 0);

	const u64 thr_size = (sizeof(kas_thread) % g_arch_config->cacheline == 0)
		? sizeof(kas_thread)
		: sizeof(kas_thread) + g_arch_config->cacheline - (sizeof(kas_thread) % g_arch_config->cacheline);

	kas_thread *thr = NULL;
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

	kas_assert((u64) thr % g_arch_config->cacheline == 0);

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
	kas_assert(real_size == thr->stack_size);

	if (pthread_create(&thr->pthread, &attr, kas_thread_clone_start, thr) != 0)
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

void kas_thread_exit(kas_thread *thr)
{
	self = NULL;
	pthread_exit(0);
}

void kas_thread_wait(const kas_thread *thr)
{
	void *garbage;

	i32 status = pthread_join(thr->pthread, &garbage);
	if (status != 0)
	{
		log_string(T_SYSTEM, S_FATAL, "Failed to alloc thread memory, aborting.");
		fatal_cleanup_and_exit(gettid());
	}
}

void kas_thread_release(kas_thread *thr)
{
	return;
}

void *kas_thread_ret_value(const kas_thread *thr)
{
	return thr->ret;	
}

void *kas_thread_args(const kas_thread *thr)
{
	return thr->args;
}

u64 kas_thread_ret_value_size(const kas_thread *thr)
{
	return thr->ret_size;
}

tid kas_thread_tid(const kas_thread *thr)
{
	return thr->tid;
}

tid kas_thread_self_tid(void)
{
	return self->tid;
}
