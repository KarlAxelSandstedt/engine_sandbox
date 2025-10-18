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

#include "wasm_local.h"

static void kas_thread_clone_start(int int_addr_thr)
{
	struct kas_thread *thr = (struct kas_thread *) int_addr_thr;
	thr->tid = emscripten_wasm_worker_self_id();
	atomic_store_rel_32(&thr->a_has_exit_jumped, 0);
	if (setjmp(thr->exit_jmp) == 0)
	{
		thr->start(thr);
	}

	atomic_store_rel_32(&thr->a_has_exit_jumped, 1);
}

void kas_thread_clone(struct arena *mem, void (*start)(kas_thread *), void *args, const u64 stack_size)
{
	struct kas_thread *thr = arena_push_aligned(mem, NULL, sizeof(struct kas_thread), g_arch_config->cacheline);
	thr->start = start;
	thr->args = args;
	thr->tls_size = __builtin_wasm_tls_size();
	thr->stack_size = stack_size;
	/* Force stack + tls to be aligned to pagesize, just to make sure size is aligned to internal STACK_ALIGN in emscripten, TODO, do this better  */
	if ((thr->stack_size + thr->tls_size) % __BIGGEST_ALIGNMENT__)
	{
		thr->stack_size +=  g_arch_config->pagesize - ((thr->stack_size + thr->tls_size) % __BIGGEST_ALIGNMENT__);
	}
	thr->stack = arena_push_aligned(mem, NULL, thr->stack_size + thr->tls_size, __BIGGEST_ALIGNMENT__);
	thr->ret = NULL;
	thr->ret_size = 0;
	thr->wasm_worker = emscripten_create_wasm_worker(thr->stack, thr->stack_size + thr->tls_size);

	kas_assert(((u64) thr->stack) % __BIGGEST_ALIGNMENT__ == 0);
	kas_assert((thr->stack_size + thr->tls_size) % __BIGGEST_ALIGNMENT__ == 0);
	kas_static_assert(sizeof(int) == sizeof(void *), "expecting (int) and (void*) to be of equal size in wasm build");
	emscripten_wasm_worker_post_function_vi(thr->wasm_worker, &kas_thread_clone_start, (int) thr);
}

void kas_thread_exit(kas_thread *thr)
{
	longjmp(thr->exit_jmp, 1);
}

void kas_thread_wait(const kas_thread *thr)
{
	while (atomic_load_acq_32(&thr->a_has_exit_jumped) == 0);
	LOG_MESSAGE(T_SYSTEM, S_NOTE, 0, "main thread acknowledged worker %u has exit jumped\n", thr->tid);
}

void kas_thread_release(kas_thread *thr)
{
	emscripten_terminate_wasm_worker(thr->wasm_worker);
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
	return emscripten_wasm_worker_self_id();
}
