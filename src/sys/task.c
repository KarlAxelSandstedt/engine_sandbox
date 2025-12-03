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

#include "sys_public.h"
#include "log.h"
#include "kas_random.h"

struct task_context t_ctx;
struct task_context *g_task_ctx = &t_ctx;

u32 a_startup_complete = 0;

static void worker_init(struct worker *w)
{
	w->mem_frame = arena_alloc_1MB();
}

static void worker_exit(void *void_task)
{
	struct task *task = void_task;
	kas_thread_exit(task->executor->thr);
}

static void task_run(struct task *task_info, struct worker *w)
{
	if (atomic_load_acq_32(&w->a_mem_frame_clear))
	{
		arena_flush(&w->mem_frame);
		atomic_store_rel_32(&w->a_mem_frame_clear, 0);
	}

	task_info->executor = w;
	task_info->task(task_info);

	switch (task_info->batch_type)
	{
		case TASK_BATCH_BUNDLE:
		{
			/* ThreadSanitizer screams if we use anything less than ACQ_REL or SEQ_CST. Can the sanitizer not
			 * assume that the compiler won't reorder the native semaphore calls? 
			 * TODO: Investigate more.
			 */
			struct task_bundle *bundle = task_info->batch;
			if (atomic_sub_fetch_seq_cst_32(&bundle->a_tasks_left, 1) == 0)
			{
				semaphore_post(&bundle->bundle_completed);
			}
		} break;

		case TASK_BATCH_STREAM:
		{
			struct task_stream *stream = task_info->batch;
			atomic_add_fetch_rel_32(&stream->a_completed, 1);
		} break;
	}
}

void task_main(kas_thread *thr)
{
	struct worker *w = kas_thread_args(thr);
	thread_xoshiro_256_init_sequence();

	while (atomic_load_acq_32(&a_startup_complete) == 0);

	w->thr = thr;
	atomic_fetch_add_seq_cst_32(&a_startup_complete, 1);
	log_string(T_SYSTEM, S_NOTE, "task_worker setup finalized");

	while (1)
	{
		/* If there is work, we plow through it continuously */
		while (semaphore_trywait(&g_task_ctx->tasks->able_for_reservation))
		{
			task_run(fifo_spmc_pop(g_task_ctx->tasks), w);
		}

		/* No more work, we go to sleep and wait until we aquire new work.
		 * Spurious wake ups may happen, so we keep this in a loop.
		 */
		while (!semaphore_wait(&g_task_ctx->tasks->able_for_reservation));

		task_run(fifo_spmc_pop(g_task_ctx->tasks), w);
	};
}

void task_main_master_run_available_jobs(void)
{
	struct worker *master = g_task_ctx->workers + 0;
	while (semaphore_trywait(&g_task_ctx->tasks->able_for_reservation))
	{
		task_run(fifo_spmc_pop(g_task_ctx->tasks), master);
	}
}

static struct task_bundle task_bundle_init()
{
	struct task_bundle bundle = { 0 };
	semaphore_init(&bundle.bundle_completed, 0);
	return bundle;
}

static void task_bundle_destroy(struct task_bundle *bundle)
{
	semaphore_destroy(&bundle->bundle_completed);
}

void task_context_init(struct arena *mem_persistent, const u32 thread_count)
{
	//TODO 
	const u64 stack_size = 64*1024;
	#define TASK_MAX_COUNT 1024
	struct task_context ctx = 
	{ 
		.workers = NULL,
		.worker_count = thread_count,
	};

	log(T_SYSTEM, S_NOTE, "Task system worker count: %u", thread_count);

	*g_task_ctx = ctx;
	g_task_ctx->bundle = task_bundle_init();
	g_task_ctx->workers = arena_push(mem_persistent, thread_count * sizeof(struct worker));	
	g_task_ctx->tasks = fifo_spmc_init(mem_persistent, TASK_MAX_COUNT);

	for (u32 i = 0; i < thread_count; ++i)
	{
		worker_init(g_task_ctx->workers + i);
	}

	/* NOTE: worker 0: reserved for main thread */
	for (u32 i = 1; i < thread_count; ++i)
	{
		kas_thread_clone(mem_persistent, task_main, g_task_ctx->workers + i, stack_size);
	}

	atomic_store_rel_32(&a_startup_complete, 1);

	while ((u32) atomic_load_seq_cst_32(&a_startup_complete) < g_task_ctx->worker_count);
}

void task_context_frame_clear(void)
{
	for (u32 i = 0; i < g_task_ctx->worker_count; ++i)
	{
		atomic_store_rel_32(&g_task_ctx->workers[i].a_mem_frame_clear, 1);
	}
}

void task_context_destroy(struct task_context *ctx)
{
	struct task *exit_tasks = malloc(ctx->worker_count * sizeof(struct task));

	for (u32 i = 1; i < ctx->worker_count; ++i)
	{
		exit_tasks[i].task = &worker_exit;
		fifo_spmc_push(ctx->tasks, exit_tasks + i);
	}

	for (u32 i = 1; i < ctx->worker_count; ++i)
	{
		kas_thread_wait(ctx->workers[i].thr);
		kas_thread_release(ctx->workers[i].thr);
	}

	for (u32 i = 0; i < ctx->worker_count; ++i)
	{
		arena_free_1MB(&ctx->workers[i].mem_frame);
	}

	
	task_bundle_destroy(&ctx->bundle);
	fifo_spmc_destroy(ctx->tasks);
	free(exit_tasks);
}

struct task_bundle *task_bundle_split_range(struct arena *mem_task_lifetime, TASK task, const u32 split_count, void *inputs, const u64 input_count, const u64 input_element_size, void *shared_arguments)
{
	const u32 tasks_per_range = (u32) input_count / split_count;
	u32 extra_tasks = input_count % split_count;
	const u32 splits = (tasks_per_range) ? split_count : extra_tasks;

	if (!splits) { return NULL; }

	struct task_bundle *bundle = &g_task_ctx->bundle;
	struct task_range *range = arena_push(mem_task_lifetime, splits * sizeof(struct task_range));
	bundle->tasks = arena_push(mem_task_lifetime, splits * sizeof(struct task));
	bundle->task_count = splits;

	u64 offset = 0;
	for (u32 i = 0; i < splits; ++i)
	{
		bundle->tasks[i].task = task;
		bundle->tasks[i].input = shared_arguments;
		bundle->tasks[i].range = range + i;
		bundle->tasks[i].batch_type = TASK_BATCH_BUNDLE,
		range[i].count = tasks_per_range; 
		if (extra_tasks) 
		{ 
			extra_tasks -= 1;
			range[i].count += 1;
		}

		range[i].base = ((u8 *) inputs) + offset;
		offset += range[i].count * input_element_size;
		atomic_store_rel_64(&bundle->tasks[i].batch, bundle);
	}

	atomic_store_rel_32(&bundle->a_tasks_left, splits);
	/* Sync points, we release tasks->data, threads aquire tasks->data => threads will see all previous writes */
	for (u32 i = 0; i < splits; ++i)
	{
		fifo_spmc_push(g_task_ctx->tasks, bundle->tasks + i);
	}

	return bundle;
}

void task_bundle_wait(struct task_bundle *bundle)
{
	while (!semaphore_wait(&bundle->bundle_completed));
}

void task_bundle_release(struct task_bundle *bundle)
{
	atomic_store_rel_32(&bundle->a_tasks_left, 0);
}

struct task_stream *task_stream_init(struct arena *mem)
{
	struct task_stream *stream = arena_push(mem, sizeof(struct task_stream));
	atomic_store_rel_32(&stream->a_completed, 0);
	stream->task_count = 0;

	return stream;
}

void task_stream_dispatch(struct arena *mem, struct task_stream *stream, TASK func, void *args)
{
	struct task *task = arena_push(mem, sizeof(struct task));
	task->task = func;
	task->input = args;
	task->batch_type = TASK_BATCH_STREAM;
	task->batch = stream;
	
	stream->task_count += 1;
	fifo_spmc_push(g_task_ctx->tasks, task);
}

void task_stream_spin_wait(struct task_stream *stream)
{
	while ((u32) atomic_load_acq_32(&stream->a_completed) < stream->task_count);
}

void task_stream_cleanup(struct task_stream *stream)
{
	const u32 finished = (atomic_load_acq_32(&stream->a_completed) == stream->task_count);
	kas_assert_string(finished, "Bad use of task stream, when (and only) the main thread enters task_stream_cleanup, all tasks must have been dispatched and completed.");
}
