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
#include "log.h"
#include "kas_profiler.h"

#ifdef KAS_PROFILER

struct kas_profiler profiler = { 0 };
struct kas_profiler *g_profiler = NULL;

kas_thread_local struct kas_frame *tls_frame = NULL;

static utf8 stub_process = { 0 };
static utf8 process_state_strings[PROCESS_COUNT] = { 0 };

void process_runtime_debug_print(const struct process_runtime *pr)
{
	fprintf(stderr, "process runtime at %p:					\
			\n{							\
			\n\t.waking_start_ns = %lu				\
			\n\t.online_start_ns = %lu				\
			\n\t.online_end_ns = %lu				\
			\n\t.pid = %i						\
			\n\t.state_end = %s					\
			\n\t.process = %s					\
			\n}\n", 
			pr,
			pr->waking_start_ns,
			pr->online_start_ns,
			pr->online_end_ns,
			pr->pid,
			(char *) utf8_alias_process_state_string(pr->state_end).buf,
			(char *) pr->process
			);

}

void static_assert_process_runtime(void)
{
	kas_static_assert((u64) &((struct process_runtime *) 0)->waking_start_ns == 0, "");
	kas_static_assert((u64) &((struct process_runtime *) 0)->online_start_ns == 8, "");
	kas_static_assert((u64) &((struct process_runtime *) 0)->online_end_ns == 16, "");
	kas_static_assert((u64) &((struct process_runtime *) 0)->pid == 24, "");
	kas_static_assert((u64) &((struct process_runtime *) 0)->state_end == 28, "");
	kas_static_assert((u64) &((struct process_runtime *) 0)->process == 32, "");
}

void static_assert_kernel_event_header(void)
{
	kas_static_assert((u64) &((struct kernel_event_header *) 0)->time == 0, "");
	kas_static_assert((u64) &((struct kernel_event_header *) 0)->type == sizeof(u64), "");
}

void static_assert_kas_schedule_switch(void)
{
	kas_static_assert((u64) &((struct kas_schedule_switch *) 0)->time == 0, "");
	kas_static_assert((u64) &((struct kas_schedule_switch *) 0)->type == sizeof(u64), "");
	kas_static_assert((u64) &((struct kas_schedule_switch *) 0)->state_prev == sizeof(u64) + sizeof(enum kernel_event_type), "");
	kas_static_assert((u64) &((struct kas_schedule_switch *) 0)->tid_prev == sizeof(u64) + sizeof(enum kernel_event_type) + sizeof(enum process_state), "");
	kas_static_assert((u64) &((struct kas_schedule_switch *) 0)->tid_next == sizeof(u64) + sizeof(enum kernel_event_type) + sizeof(enum process_state) + sizeof(tid), "");
	kas_static_assert((u64) &((struct kas_schedule_switch *) 0)->process_prev == sizeof(u64) + sizeof(enum kernel_event_type) + sizeof(enum process_state) + 2*sizeof(tid), "");
	kas_static_assert((u64) &((struct kas_schedule_switch *) 0)->process_next == sizeof(u64) + sizeof(enum kernel_event_type) + sizeof(enum process_state) + 2*sizeof(tid) + 16*sizeof(u8), "");
	kas_static_assert((u64) &((struct kas_schedule_switch *) 0)->cpu == sizeof(u64) + sizeof(enum kernel_event_type) + sizeof(enum process_state) + 2*sizeof(tid) + 32*sizeof(u8), "");
}

void static_assert_kas_schedule_waking(void)
{
	kas_static_assert((u64) &((struct kas_schedule_waking *) 0)->time == 0, "");
	kas_static_assert((u64) &((struct kas_schedule_waking *) 0)->type == sizeof(u64), "");
	kas_static_assert((u64) &((struct kas_schedule_waking *) 0)->pid == sizeof(u64) + sizeof(enum kernel_event_type), "");
	kas_static_assert((u64) &((struct kas_schedule_waking *) 0)->process == sizeof(u64) + sizeof(enum kernel_event_type) + sizeof(tid), "");
	kas_static_assert((u64) &((struct kas_schedule_waking *) 0)->cpu == sizeof(u64) + sizeof(enum kernel_event_type) + sizeof(tid) + 16*sizeof(u8), "");
}

void static_assert_kaspf_header_format(void)
{
	kas_static_assert(sizeof(struct kaspf_header) == 8192, "header size should be a multiple of a pagesize");
	kas_static_assert(FRAME_TABLE_FULL_SIZE == 4096, "header size should be a multiple of a pagesize");
	kas_static_assert(KASPF_LABEL_TABLE_SIZE % 4096 == 0, "label table should be multiple of 4096");
}

/* first iteration storage for mm_branch to simplify code (skip nasty if statements checking frame_count == 0) */
static struct frame_header frame_stub = { 0 };
static struct frame_table table_stub =  { .ns_start = U64_MAX, .ns_end = U64_MAX };

static void kaspf_init_header(struct kas_profiler *profiler)
{	
	struct kaspf_header *h = profiler->header;
	h->major = KASPF_MAJOR;
	h->minor = KASPF_MINOR;
	h->frame_count = 0;
	h->worker_count = profiler->worker_count;
	h->kernel_buffer_count = profiler->kernel_buffer_count;	
        h->clock_freq = profiler->clock_freq;		
        h->rdtsc_freq = profiler->rdtsc_freq;		
        h->bytes = sizeof(struct kaspf_header);
	h->page_size = g_arch_config->pagesize;
	h->pid = g_arch_config->pid;
        h->mm_branch[0] = &table_stub;		
        h->mm_branch[1] = &table_stub;		
        h->mm_branch[2] = &frame_stub;		
	h->l1_table.ns_start = 0;
	h->l1_table.ns_end = U64_MAX;
	h->task_count_max = KASPF_UNIQUE_TASK_COUNT_MAX;
	memset(h->l1_table.entries, 0xff, FRAME_TABLE_SIZE);
}

static void kaspf_init_task_tables(struct kas_profiler *profiler)
{
	kas_assert(profiler->header->bytes % profiler->header->page_size == 0);	

	const u64 table_offset = profiler->header->bytes;
	profiler->header->bytes += KASPF_LABEL_TABLE_SIZE;
	profiler->header->bytes += KASPF_UNIQUE_TASK_COUNT_MAX * sizeof(profiler->header->mm_task_systems[0]);

	file_set_size(&profiler->file, profiler->header->bytes);
	
	profiler->header->mm_labels = file_memory_map_partial(&profiler->file,
						KASPF_LABEL_TABLE_SIZE,
						table_offset,
						FS_PROT_READ | FS_PROT_WRITE,
						FS_MAP_SHARED);
	memset(profiler->header->mm_labels, 0, KASPF_LABEL_TABLE_SIZE);

	profiler->header->mm_task_systems = file_memory_map_partial(&profiler->file,
						KASPF_UNIQUE_TASK_COUNT_MAX * sizeof(profiler->header->mm_task_systems[0]),
						table_offset + KASPF_LABEL_TABLE_SIZE,
						FS_PROT_READ | FS_PROT_WRITE,
						FS_MAP_SHARED);
	memset(profiler->header->mm_task_systems, 0, KASPF_UNIQUE_TASK_COUNT_MAX * sizeof(profiler->header->mm_task_systems[0]));

	kas_assert(profiler->header->bytes % profiler->header->page_size == 0);	
}

static u64 internal_alloc_frame_header(struct kas_profiler *profiler)
{
	const u64 offset = profiler->header->bytes;
	profiler->header->bytes += sizeof(struct frame_header);
	void *map = file_memory_map_partial(&profiler->file, sizeof(struct frame_header), offset, FS_PROT_READ | FS_PROT_WRITE, FS_MAP_SHARED);
	memset(map, 0, sizeof(struct frame_header));
	file_memory_unmap(map, sizeof(struct frame_header));

	return offset;
}

static u64 internal_alloc_frame_table(struct kas_profiler *profiler) 
{
	const u64 offset = profiler->header->bytes;
	profiler->header->bytes += FRAME_TABLE_FULL_SIZE;
	void *map = file_memory_map_partial(&profiler->file, FRAME_TABLE_FULL_SIZE, offset, FS_PROT_READ | FS_PROT_WRITE, FS_MAP_SHARED);
	memset(map, 0xff, FRAME_TABLE_FULL_SIZE);
	file_memory_unmap(map, sizeof(struct frame_header));

	return offset;
}

void kaspf_frame_table_indices(u64 *l1_i, u64 *l2_i, u64 *l3_i, const u64 frame)
{
	*l3_i = (frame % L3_FRAME_COUNT);
	*l2_i = (frame / L3_FRAME_COUNT) % L3_FRAME_COUNT;
	*l1_i = (frame / L2_FRAME_COUNT);
}

/* assumes all unused entries in table have time intervals [U64_MAX, U64_MAX] */
u64 kaspf_frame_table_index_from_time(const struct frame_table *table, const u64 table_len, const u64 ns_time)
{
	if (ns_time < table->ns_start || table->ns_end < ns_time)
	{
		log_string(T_SYSTEM, S_ERROR, "searching for time outside of table!");
		return U64_MAX;
	}


	u64 index = table_len >> 1;
	u64 low = 0;
	u64 high = table_len-1;
	while (low < high)
	{
		if (ns_time < table->entries[index].ns_start)
		{
			high = index-1;
			index = ((high + low) >> 1);
		}
		else
		{
			low = index;
			index =  1 + ((high + low) >> 1);
		}
	}

	if (low == high) { index = low; }

	{
		kas_assert(index < table_len);
		const u64 ns_start = table->entries[index].ns_start;
		const u64 ns_end = (index < table_len-1) ? table->entries[index+1].ns_start : table->ns_end;
		kas_assert(ns_start <= ns_time && ns_time <= ns_end);
	}

	return index;
}

struct frame_header *kaspf_next_header(const struct frame_header *fh, const u64 l1_i, const u64 l2_i, const u64 l3_i)
{
	const u8 *addr = (const u8 *) fh;

	const u64 mod = fh->size % g_profiler->header->page_size;
	addr += fh->size;
	addr += (mod) ? g_profiler->header->page_size - mod : 0; 
	addr += (l3_i+1 == L3_FRAME_COUNT) ? FRAME_TABLE_FULL_SIZE : 0;
	addr += (l2_i+1 == L3_FRAME_COUNT) ? FRAME_TABLE_FULL_SIZE : 0;

	return (struct frame_header *) addr;
}

static void kaspf_alloc_headers_in_frame(struct kas_profiler *profiler, const u64 ns_time, const u64 tsc_time)
{
	struct kaspf_header *h = profiler->header;
		
	if (profiler->frame_counter == L1_FRAME_COUNT)
	{
		log(T_SYSTEM, S_ERROR, "kaspf files support a maximum of %lu frames, limit reached", L1_FRAME_COUNT);
		return;
	}

	struct frame_table *l2_table = h->mm_branch[0];
	struct frame_table *l3_table = h->mm_branch[1];
	struct frame_header *prev_frame = h->mm_branch[2];

	u64 l1_i, l2_i, l3_i;
	kaspf_frame_table_indices(&l1_i, &l2_i, &l3_i, h->frame_count);

	if (h->frame_count % L2_FRAME_COUNT == 0)
	{
		l2_table->ns_end = ns_time;
		if (h->frame_count != 0)
		{
			file_memory_unmap(h->mm_branch[0], FRAME_TABLE_FULL_SIZE);
		}

		h->l1_table.entries[l1_i].ns_start = ns_time;
		h->l1_table.entries[l1_i].offset = internal_alloc_frame_table(profiler);
		h->mm_branch[0] = file_memory_map_partial(&profiler->file, 
				FRAME_TABLE_FULL_SIZE,
				h->l1_table.entries[l1_i].offset,
				FS_PROT_READ | FS_PROT_WRITE,
				FS_MAP_SHARED);

		l2_table = h->mm_branch[0];
		l2_table->ns_start = ns_time;
	}
	
	if (h->frame_count % L3_FRAME_COUNT == 0)
	{
		l3_table->ns_end = ns_time;
		if (h->frame_count != 0)
		{
			file_memory_unmap(h->mm_branch[1], FRAME_TABLE_FULL_SIZE);
		}

		l2_table->entries[l2_i].ns_start = ns_time;
		l2_table->entries[l2_i].offset = internal_alloc_frame_table(profiler);
		h->mm_branch[1] = file_memory_map_partial(&profiler->file, 
				FRAME_TABLE_FULL_SIZE,
			       	l2_table->entries[l2_i].offset,
				FS_PROT_READ | FS_PROT_WRITE,
				FS_MAP_SHARED);

		l3_table = h->mm_branch[1];
		l3_table->ns_start = ns_time;
	}


	prev_frame->ns_end = ns_time;
	prev_frame->tsc_end = tsc_time;
	if (h->frame_count != 0)
	{
		file_memory_unmap(h->mm_branch[2], sizeof(struct frame_header));
	}

	l3_table->entries[l3_i].ns_start = ns_time;
	l3_table->entries[l3_i].offset = internal_alloc_frame_header(profiler);
	h->mm_branch[2] = file_memory_map_partial(&profiler->file, 
				sizeof(struct frame_header),
			       	l3_table->entries[l3_i].offset,
				FS_PROT_READ | FS_PROT_WRITE,
				FS_MAP_SHARED);

	kas_assert(h->l1_table.entries[l1_i].offset % h->page_size == 0);
	kas_assert(l2_table->entries[l2_i].offset % h->page_size == 0);
	kas_assert(l3_table->entries[l3_i].offset % h->page_size == 0);

	struct frame_header *frame = h->mm_branch[2];
	frame->ns_start = ns_time;
	frame->tsc_start = tsc_time;
	frame->ns_end = 0;	
	frame->size = 0; 

	h->frame_count += 1;
}

u64 kaspf_frame_offset(const struct kas_profiler *profiler, const u64 frame)
{
	/* not allowed to lookup frame we are building */
	kas_assert_string(frame < profiler->header->frame_count, "Should never lookup frames beyond end-of-file\n");

	u64 l1_i, l2_i, l3_i;
	kaspf_frame_table_indices(&l1_i, &l2_i, &l3_i, frame);

	u64 offset = profiler->header->l1_table.entries[l1_i].offset;
	struct frame_table *l2_table = file_memory_map_partial(&profiler->file, FRAME_TABLE_FULL_SIZE, offset, FS_PROT_READ, FS_MAP_SHARED);

	offset = l2_table->entries[l2_i].offset;
	struct frame_table *l3_table = file_memory_map_partial(&profiler->file, FRAME_TABLE_FULL_SIZE, offset, FS_PROT_READ, FS_MAP_SHARED);

	offset = l3_table->entries[l3_i].offset;

	file_memory_unmap(l3_table, FRAME_TABLE_FULL_SIZE);
	file_memory_unmap(l2_table, FRAME_TABLE_FULL_SIZE);

	return offset;
}

static void kaspf_write_completed_frame(struct kas_profiler *profiler)
{
	struct kaspf_header *h = profiler->header;
	if (!h->frame_count) { return; }

	/* grab index of completed frame */
	const u64 frame_offset = kaspf_frame_offset(profiler, h->frame_count-1);
	const u64 headers_offset = frame_offset + sizeof(struct frame_header);
	const u64 lw_kt_headers_size = profiler->worker_count*sizeof(struct lw_header) + profiler->kernel_buffer_count*sizeof(struct kt_header);
	u64 data_offset = headers_offset + lw_kt_headers_size;

	for (u32 i = 0; i < profiler->worker_count; ++i)
	{
		const struct kas_frame *f = profiler->worker_frame + i;
		const u64 profile_count = f->completed_count-1;
		data_offset += profile_count * sizeof(struct lw_profile);
		const u64 data_size_worker_activity = profiler->frame_worker_activity_count[i] * sizeof(struct worker_activity);
		data_offset += data_size_worker_activity;
	}

	for (u32 i = 0; i < profiler->kernel_buffer_count; ++i)
	{
		const u64 data_size = profiler->frame_pr_count[i] * sizeof(struct process_runtime);
		data_offset += data_size;
	}
	const u64 frame_size = data_offset - frame_offset;
	struct frame_header *frame = h->mm_branch[2];
	frame->size = frame_size;
	h->bytes += frame_size - sizeof(struct frame_header);	/* we have already taken frame header size into account when we allocated the frame's header */
	h->bytes += (h->bytes % h->page_size) ? h->page_size - (h->bytes % h->page_size) : 0;

	//fprintf(stderr, "writing frame %lu at offset %lu\n", h->frame_count-1, frame_offset);
	//fprintf(stderr, "frame header: (%lu, %lu)\n", frame_offset, frame_offset + sizeof(struct frame_header));
	//fprintf(stderr, "lw_kt_headers: (%lu, %lu)\n", (u64) headers_offset, (u64) headers_offset + lw_kt_headers_size);

	u8 *map = file_memory_map_partial(&profiler->file, frame_size, frame_offset, FS_PROT_READ | FS_PROT_WRITE, FS_MAP_SHARED);
	//log(T_SYSTEM, S_NOTE, "FRAME WRITE: (%lu, %lu)", frame_offset, frame_size);

	struct lw_header *lw_headers = (struct lw_header *) (map + sizeof(struct frame_header));
	struct kt_header *kt_headers = (struct kt_header *) (map + sizeof(struct frame_header) + profiler->worker_count*sizeof(struct lw_header));

	data_offset = sizeof(struct frame_header) + lw_kt_headers_size;
	for (u32 i = 0; i < profiler->worker_count; ++i)
	{
		const struct kas_frame *f = profiler->worker_frame + i;
		lw_headers[i].profile_offset = data_offset;
		lw_headers[i].profile_count = f->completed_count-1;
		u64 data_size = lw_headers[i].profile_count * sizeof(struct lw_profile);
		memcpy(map + data_offset, f->completed + 1, data_size);
		//fprintf(stderr, "lw_profile[%u]: (%lu, %lu)\n", i, lw_headers[i].profile_offset, lw_headers[i].profile_offset + data_size_lw_profiles);
		data_offset += data_size;

		lw_headers[i].activity_offset = data_offset;
		lw_headers[i].activity_count = profiler->frame_worker_activity_count[i];
		data_size = profiler->frame_worker_activity_count[i] * sizeof(struct worker_activity);
		memcpy(map + data_offset, profiler->frame_worker_activity[i], data_size);
		data_offset += data_size;

		//log(T_SYSTEM, S_NOTE, "WRITE P(%lu, %u)", (u64) (&lw_headers[i]) - (u64)map, lw_headers[i].profile_count);
		//log(T_SYSTEM, S_NOTE, "WRITE A(%lu, %u)", (u64) (&lw_headers[i]) - (u64)map, profiler->frame_worker_activity_count[i]);
	}

	for (u32 i = 0; i < profiler->kernel_buffer_count; ++i)
	{
		kt_headers[i].pr_offset = data_offset;
		kt_headers[i].pr_count = profiler->frame_pr_count[i];
		const u64 data_size = profiler->frame_pr_count[i] * sizeof(struct process_runtime);
		memcpy(map + data_offset, profiler->frame_pr[i], data_size);
		data_offset += data_size;
	}

	file_memory_unmap(map, frame_size);
	//fprintf(stderr, "frame %lu has size (unpadded) %lu\n", h->frame_count-1, frame_size);
}

static void kas_profiler_init_io(struct arena *mem, struct kas_profiler *profiler, const char *path)
{
	kaspf_reader_alloc(1024*1024*1024);
	profiler->file = file_null();
	if (file_try_create_at_cwd(mem, &profiler->file, path, FILE_TRUNCATE) != FS_SUCCESS)
	{
		fatal_cleanup_and_exit(0);
	}

	profiler->header = file_memory_map_partial(&profiler->file, sizeof(struct kaspf_header), 0, FS_PROT_READ | FS_PROT_WRITE, FS_MAP_SHARED);
	memset(profiler->header, 0, sizeof(struct kaspf_header));
	if (profiler->header == NULL)
	{
		fatal_cleanup_and_exit(0);
	}

	kaspf_init_header(profiler);
	kaspf_init_task_tables(profiler);
}

static void kas_profiler_io_shutdown(struct kas_profiler *profiler)
{
	if (!profiler->header->frame_count)
	{
		return;
	}

	/* write last frame to disk, skip the new one created */
	kas_profiler_new_frame();
	profiler->header->frame_count -= 1;

	file_memory_sync_unmap(profiler->header->mm_branch[0], FRAME_TABLE_FULL_SIZE);
	file_memory_sync_unmap(profiler->header->mm_branch[1], FRAME_TABLE_FULL_SIZE);
	file_memory_sync_unmap(profiler->header->mm_branch[2], sizeof(struct kaspf_header));
	file_memory_sync_unmap(profiler->header->mm_labels, KASPF_LABEL_TABLE_SIZE);
	file_memory_sync_unmap(profiler->header->mm_task_systems, sizeof(profiler->header->mm_task_systems[0]) * KASPF_UNIQUE_TASK_COUNT_MAX);

	file_sync(&profiler->file);
	file_close(&profiler->file);
}

void kas_profiler_init(struct arena *mem, const u64 master_thread_id, const u32 worker_count, const u32 frame_len, const u32 stack_len, const u64 rdtsc_freq, const enum profile_level level)
{
	assert(worker_count >= 1);
 
	stub_process = utf8_inline("stub");

	process_state_strings[0] = utf8_inline("process running"),
	process_state_strings[1] = utf8_inline("process sleeping"),
	process_state_strings[2] = utf8_inline("process blocked"),
	process_state_strings[3] = utf8_inline("process unhandled state"),

	atomic_store_rel_32(&profiler.a_next_task_id, 0);
	profiler.ns_frame = 0;
	profiler.frame_counter = UINT64_MAX;
	profiler.tls_i = 0;
	profiler.rdtsc_freq = rdtsc_freq;
	profiler.worker_count = worker_count;
	profiler.worker_frame = arena_push(mem, worker_count * sizeof(struct kas_frame));
	profiler.mem = arena_alloc(16*1024*1024);
	profiler.level = level;
	profiler.kernel_buffer_count = 0;

	if (profiler.level >= PROFILE_LEVEL_TASK)
	{
		struct lw_profile stub = { 0 };
		for (u32 i = 0; i < worker_count; ++i)
		{
			profiler.worker_frame[i].completed = arena_push(mem, frame_len * sizeof(struct lw_profile));
			profiler.worker_frame[i].build = arena_push(mem, frame_len * sizeof(struct lw_profile));
			profiler.worker_frame[i].build_stack = arena_push(mem, stack_len * sizeof(u32));
			profiler.worker_frame[i].completed_count = 1;
			profiler.worker_frame[i].build_count = 1;
			profiler.worker_frame[i].stack_count = 1;
			((struct lw_profile *)(profiler.worker_frame[i].completed))[0] = stub; 
			profiler.worker_frame[i].build[0] = stub;
			profiler.worker_frame[i].build_stack[0] = 0;
			profiler.worker_frame[i].frame_len = frame_len;
			profiler.worker_frame[i].stack_len = stack_len;
		}

		profiler.worker_activity = arena_push(mem, profiler.worker_count * sizeof(struct worker_activity));
		profiler.frame_worker_activity = arena_push(mem, profiler.worker_count * sizeof(struct worker_activity *));
		profiler.frame_worker_activity_count = arena_push(mem, profiler.worker_count * sizeof(u32));

		for (u32 i = 0; i < profiler.worker_count; ++i)
		{
			profiler.frame_worker_activity[i] = NULL;
			profiler.frame_worker_activity_count[i] = 0;
		}

		if (profiler.level == PROFILE_LEVEL_KERNEL)
		{
			if (!system_user_is_admin())
			{
				log_string(T_SYSTEM, S_WARNING, "User is not privileged, skipping kernel profiling.");
				profiler.level = PROFILE_LEVEL_TASK;
			}
			else if ((profiler.kt = kernel_tracer_init(mem)) == NULL)
			{
				log_string(T_SYSTEM, S_WARNING, "Failed to initialize kernel tracer, skipping kernel profiling.");
				profiler.level = PROFILE_LEVEL_TASK;
			}
			else
			{
				profiler.kernel_buffer_count = profiler.kt->buffer_count;
				profiler.pr[0] = arena_push(mem, profiler.kernel_buffer_count * sizeof(struct process_runtime));
				profiler.pr[1] = arena_push(mem, profiler.kernel_buffer_count * sizeof(struct process_runtime));
				profiler.online_index = arena_push(mem, profiler.kernel_buffer_count * sizeof(u32));
				profiler.frame_pr_count = arena_push(mem, profiler.kernel_buffer_count * sizeof(u32));
				profiler.frame_pr = arena_push(mem, profiler.kernel_buffer_count * sizeof(struct process_runtime *));
				//TODO Setup initial dummy worker activites 
				kas_assert(stub_process.len <= 16);
				for (u32 i = 0; i < profiler.kernel_buffer_count; ++i)
				{
					profiler.online_index[i] = 0;
					profiler.pr[profiler.online_index[i]][i].waking_start_ns = 0;	
					profiler.pr[profiler.online_index[i]][i].online_start_ns = 0;	
					profiler.pr[profiler.online_index[i]][i].pid = U32_MAX;	
					profiler.pr[1-profiler.online_index[i]][i].waking_start_ns = PROCESS_NON_WAKING;	
					memcpy(profiler.pr[profiler.online_index[i]][i].process, stub_process.buf, stub_process.len);
				}

				kas_assert(profiler.kernel_buffer_count < U8_MAX && "Current kernel events use u8's to represent cpu, use u16's instead.");
			}
		}

		kas_profiler_init_io(mem, &profiler, "profile.kaspf");
	}
				
	switch (profiler.level)
	{
		case PROFILE_LEVEL_SYSTEM: { log_string(T_SYSTEM, S_NOTE, "Lightweight system level profiling initiated."); } break;
		case PROFILE_LEVEL_TASK:   { log_string(T_SYSTEM, S_NOTE, "Heavyweight task level profiling initiated."); } break;
		case PROFILE_LEVEL_KERNEL: { log_string(T_SYSTEM, S_NOTE, "Heavyweight kernel level profiling initiated."); } break;
	}

	atomic_store_rel_64(&g_profiler, &profiler);
	kas_profiler_acquire_thread_local_frame(master_thread_id, kas_thread_self_tid());
}

void kas_profiler_shutdown(void)
{
	if (g_profiler->level >= PROFILE_LEVEL_TASK)
	{
		kas_profiler_io_shutdown(g_profiler);
		arena_free(&profiler.mem);
		kaspf_reader_shutdown();
		if (g_profiler->level == PROFILE_LEVEL_KERNEL)
		{
			kernel_tracer_shutdown(g_profiler->kt);
		}
	}
}

void kas_profiler_try_set_task_id(volatile u32 *a_static_task_id, volatile u32 *a_static_setting, const char *label, const enum system_id system_id)
{
	kas_assert(g_profiler->level >= PROFILE_LEVEL_TASK);

	u32 not_being_set = 0;
	if (atomic_compare_exchange_seq_cst_32(a_static_setting, &not_being_set, 1))
	{
		const u32 id = atomic_fetch_add_rlx_32(&g_profiler->a_next_task_id, 1);
		kas_assert_string(id < KASPF_UNIQUE_TASK_COUNT_MAX, "max unique tasks reached, increase KASPF_UNIQUE_TASK_COUNT_MAX.\n");
		u64 len = strlen(label);
		len = (len < KASPF_LABEL_BUFSIZE) ? len : KASPF_LABEL_BUFSIZE-1;
		memcpy(g_profiler->header->mm_labels[id], label, len);
		g_profiler->header->mm_labels[id][len] = '\0';
		atomic_store_seq_cst_32(a_static_task_id, id);
		g_profiler->header->mm_task_systems[id] = system_id;
	}
	else
	{
		while (atomic_load_rlx_32(a_static_task_id) == U32_MAX);
	}
}

void kas_profiler_acquire_thread_local_frame(const u32 index, const tid thread_id)
{
	struct kas_profiler *kas = (struct kas_profiler *) atomic_load_acq_64(&g_profiler);
	const u32 i = atomic_fetch_add_rlx_32(&kas->tls_i, 1);
	kas_assert(i < kas->worker_count);
	tls_frame = kas->worker_frame + i;	
	tls_frame->thread_index = index;
	tls_frame->thread_id = thread_id;
}

utf8 utf8_alias_process_state_string(const enum process_state state)
{
	kas_assert(state <= PROCESS_COUNT);
	return process_state_strings[state];
}

#if (__OS__ == __LINUX__)

/* include/linux/sched.h */
#define TASK_RUNNING			0x00000000
#define TASK_INTERRUPTIBLE		0x00000001
#define TASK_UNINTERRUPTIBLE		0x00000002
#define TASK_STOPPED			0x00000004
#define TASK_TRACED			0x00000008
#define EXIT_DEAD			0x00000010
#define EXIT_ZOMBIE			0x00000020
#define EXIT_TRACE			(EXIT_ZOMBIE | EXIT_DEAD)
#define TASK_PARKED			0x00000040
#define TASK_DEAD			0x00000080
#define TASK_WAKEKILL			0x00000100

struct cpu_process
{
	u32 worker;	/* worker == WORKER_COUNT means not worker thread */
	u32 cpu;
	struct process_runtime pr;
};

u32 internal_is_process_schedule_waking(const struct kas_profiler *g_profiler, const struct kt_sched_waking *w)
{
	u32 is_process_event = 0;

	for (u32 i = 0; i < g_profiler->worker_count; ++i)
	{
		if (w->pid == g_profiler->worker_frame[i].thread_id)
		{
			is_process_event = 1;
			break;
		}
	}

	return is_process_event;
}

u32 internal_is_process_schedule_switch(const struct kas_profiler *g_profiler, const struct kt_sched_switch *ss)
{
	u32 is_process_event = 0;

	for (u32 i = 0; i < g_profiler->worker_count; ++i)
	{
		const tid id = g_profiler->worker_frame[i].thread_id;
		if (ss->prev_pid == id || ss->next_pid == id)
		{
			is_process_event = 1;
			break;
		}
	}

	return is_process_event;
}


void kas_profiler_gather_kernel_profiles(void)
{
	for (u32 i = 0; i < g_profiler->kernel_buffer_count; ++i)
	{
		g_profiler->kt->buffers[i].frame_start = atomic_load_acq_64(&g_profiler->kt->buffers[i].metadata->data_tail);
		g_profiler->kt->buffers[i].frame_end = atomic_load_acq_64(&g_profiler->kt->buffers[i].metadata->data_head);
		g_profiler->kt->buffers[i].offset = g_profiler->kt->buffers[i].frame_start;
		g_profiler->frame_pr_count[i] = 0;
		g_profiler->frame_pr[i] = NULL;
	}

	for (u32 i = 0; i < g_profiler->worker_count; ++i)
	{
		g_profiler->frame_worker_activity_count[i] = 0;
		g_profiler->frame_worker_activity[i] = NULL;
	}

	struct kt_event ev;
	struct kt_datapoint *next_cpu_dp = arena_push(&g_profiler->mem, g_profiler->kernel_buffer_count*sizeof(struct kt_datapoint));
	/* initiate first events and timestamps */
	for (u32 i = 0; i < g_profiler->kernel_buffer_count; ++i)
	{
		kernel_tracer_try_read_bytes(next_cpu_dp+i, g_profiler->kt->buffers+i, KT_DATAPOINT_PACKED_SIZE);
		const u64 tsc = g_profiler->kt->tsc_from_kt_time(g_profiler->kt->buffers+i, next_cpu_dp[i].time);
		next_cpu_dp[i].time = time_ns_from_tsc_truth_source(tsc, g_profiler->ns_frame_prev, g_profiler->tsc_frame_prev);
	}

	struct allocation_array alloc = arena_push_aligned_all(&g_profiler->mem, sizeof(struct cpu_process), 16);
	u64 stack_count = 0;
	u64 stack_len = alloc.len;
	struct cpu_process *cpu_process = alloc.addr;
	while (stack_count < stack_len)
	{
		u64 earliest_time = g_profiler->ns_frame;
		u32 earliest_buf = U32_MAX;
		for (u32 i = 0; i < g_profiler->kernel_buffer_count; ++i)
		{
			if (next_cpu_dp[i].raw_size != U32_MAX && next_cpu_dp[i].time < earliest_time)
			{
				earliest_time = next_cpu_dp[i].time;
				earliest_buf = i;
			}
		}

		if (earliest_buf == U32_MAX)
		{
			break;
		}
		
		kas_assert_string(next_cpu_dp[earliest_buf].time < g_profiler->ns_frame, "Sanity check; it doesn't actually matter if this does not hold, since we do have small errors in all time calculations, but it is useful for the moment to see how often and when it happens.");

		struct kt_ring_buffer *kt_buf = g_profiler->kt->buffers + earliest_buf;
		kernel_tracer_read_bytes(&ev, kt_buf, next_cpu_dp[earliest_buf].raw_size);

		{
			//kt_datapoint_debug_print(next_cpu_dp + earliest_buf);
			//kt_event_debug_print(g_profiler->kt, &ev);
		}

		const u32 on_i = g_profiler->online_index[earliest_buf];
		const u32 off_i	= 1 - on_i;

		if (ev.common.type == g_profiler->kt->sched_switch_id)
		{
			struct kt_sched_switch *ss = &ev.ss;
			g_profiler->frame_pr_count[earliest_buf] += 1;
			struct cpu_process *process = cpu_process + stack_count;
			stack_count += 1;

			//ss_count += 1;
			//struct kas_schedule_switch *kss = (struct kas_schedule_switch *) (stack + size);

			//kss->time = next_cpu_dp[earliest_buf].time;
			//kss->type = KERNEL_EVENT_SCHED_SWITCH;
			//kss->tid_prev = ss->prev_pid;
			//kss->tid_next = ss->next_pid;
			//kss->cpu = (u8) earliest_buf;
			//memcpy(kss->process_prev, ss->prev_comm, sizeof(ss->prev_comm));
			//memcpy(kss->process_next, ss->next_comm, sizeof(ss->next_comm));
			
			//if (ss->prev_state == TASK_RUNNING) { kss->state_prev = PROCESS_RUNNING; }
			//else if (ss->prev_state == TASK_INTERRUPTIBLE) { kss->state_prev = PROCESS_SLEEPING; }
			//else if (ss->prev_state == TASK_UNINTERRUPTIBLE) { kss->state_prev = PROCESS_BLOCKED; }
			//else { kss->state_prev = PROCESS_UNHANDLED_STATE; }

			//size += KAS_SCHEDULE_SWITCH_PACKED_SIZE;
			//kas_assert(size <= g_profiler->mem.mem_left);
			
			/* update process_runtime state */
			g_profiler->online_index[earliest_buf] = off_i;

			if (g_profiler->pr[off_i][earliest_buf].waking_start_ns == PROCESS_NON_WAKING)
			{
				g_profiler->pr[off_i][earliest_buf].waking_start_ns = next_cpu_dp[earliest_buf].time;
			}
			g_profiler->pr[off_i][earliest_buf].online_start_ns = next_cpu_dp[earliest_buf].time;
			g_profiler->pr[off_i][earliest_buf].pid = ss->next_pid;
			memcpy(g_profiler->pr[off_i][earliest_buf].process, ss->next_comm, sizeof(ss->next_comm));

			g_profiler->pr[on_i][earliest_buf].online_end_ns = next_cpu_dp[earliest_buf].time;
			if (ss->prev_state == TASK_RUNNING) { g_profiler->pr[on_i][earliest_buf].state_end = PROCESS_RUNNING; }
			else if (ss->prev_state == TASK_INTERRUPTIBLE) { g_profiler->pr[on_i][earliest_buf].state_end = PROCESS_SLEEPING; }
			else if (ss->prev_state == TASK_UNINTERRUPTIBLE) { g_profiler->pr[on_i][earliest_buf].state_end = PROCESS_BLOCKED; }
			else { g_profiler->pr[on_i][earliest_buf].state_end = PROCESS_UNHANDLED_STATE; }

			
			u32 worker;
			for (worker = 0; worker < g_profiler->worker_count; ++worker)
			{
				if (g_profiler->pr[on_i][earliest_buf].pid == g_profiler->worker_frame[worker].thread_id)
				{
					g_profiler->frame_worker_activity_count[worker] += 1;
					break;
				}
			}

			process->worker = worker;
			process->cpu = earliest_buf;
			memcpy(&process->pr, &g_profiler->pr[on_i][earliest_buf], sizeof(struct process_runtime));
			//process_runtime_debug_print(g_profiler->pr[on_i] + earliest_buf);	
			g_profiler->pr[on_i][earliest_buf].waking_start_ns = PROCESS_NON_WAKING;
			
		}
		else if (ev.common.type == g_profiler->kt->sched_waking_id)
		{
			struct kt_sched_waking *waking = &ev.waking;
			//waking_count += 1;
			//struct kas_schedule_waking *w = (struct kas_schedule_waking *) (stack + size);

			//w->time = next_cpu_dp[earliest_buf].time;
			//w->type = KERNEL_EVENT_SCHED_WAKING;
			//w->pid = waking->pid;
			//w->cpu = (u8) earliest_buf;
			//memcpy(w->process, waking->comm, sizeof(waking->comm));
			//size += KAS_SCHEDULE_WAKING_PACKED_SIZE;
			//kas_assert(size <= g_profiler->mem.mem_left);

			/* update process_runtime state */
			{
				g_profiler->pr[off_i][earliest_buf].waking_start_ns = next_cpu_dp[earliest_buf].time;
			}
		}
		else
		{
			kas_assert_string(0, "handle new event in kas_gather_kernel_profiles");
		}
		
		kernel_tracer_try_read_bytes(next_cpu_dp + earliest_buf, kt_buf, KT_DATAPOINT_PACKED_SIZE);
		const u64 tsc = g_profiler->kt->tsc_from_kt_time(kt_buf, next_cpu_dp[earliest_buf].time);
		next_cpu_dp[earliest_buf].time = time_ns_from_tsc_truth_source(tsc, g_profiler->ns_frame_prev, g_profiler->tsc_frame_prev);
	}

	for (u32 i = 0; i < g_profiler->kernel_buffer_count; ++i)
	{
		/* If we managed to read last header in buffer, but not consuming it, the event happened 
		   in a later frame, so we must backtrack our offset  */
		if (next_cpu_dp[i].raw_size != U32_MAX)
		{
			g_profiler->kt->buffers[i].offset -= KT_DATAPOINT_PACKED_SIZE;
		}

		kas_assert((g_profiler->kt->buffers[i].offset == g_profiler->kt->buffers[i].frame_end 
			   && next_cpu_dp[i].raw_size == U32_MAX)
			   || next_cpu_dp[i].raw_size != U32_MAX)
		atomic_store_rel_64(&g_profiler->kt->buffers[i].metadata->data_tail, g_profiler->kt->buffers[i].offset);
	}
	
	arena_pop_packed(&g_profiler->mem, (stack_len-stack_count)*sizeof(struct cpu_process)); 
	u32 *index = arena_push_packed(&g_profiler->mem, sizeof(u32) * g_profiler->kernel_buffer_count);
	u32 pr_count = 0;
	for (u32 i = 0; i < g_profiler->kernel_buffer_count; ++i)
	{
		index[i] = 0;
		pr_count += g_profiler->frame_pr_count[i];
		g_profiler->frame_pr[i] = arena_push_packed(&g_profiler->mem, sizeof(struct process_runtime) * g_profiler->frame_pr_count[i]);
	}

	u32 *wi = arena_push_packed(&g_profiler->mem, sizeof(u32) * g_profiler->worker_count);
	for (u32 i = 0; i < g_profiler->worker_count; ++i)
	{
		wi[i] = 0;
		g_profiler->frame_worker_activity[i] = arena_push_packed(&g_profiler->mem, sizeof(struct worker_activity) * g_profiler->frame_worker_activity_count[i]);
	}


	for (u32 i = 0; i < pr_count; ++i)
	{
		g_profiler->frame_pr[cpu_process[i].cpu][index[cpu_process[i].cpu]] = cpu_process[i].pr;
		index[cpu_process[i].cpu] += 1;
		if (cpu_process[i].worker < g_profiler->worker_count)
		{
			//TODO if wakeup time != online start time, add wake up activity
			//TODO how to process and generate blocks, sleeps?

			g_profiler->frame_worker_activity[cpu_process[i].worker][wi[cpu_process[i].worker]].ns_start = cpu_process[i].pr.online_start_ns;
			g_profiler->frame_worker_activity[cpu_process[i].worker][wi[cpu_process[i].worker]].ns_end = cpu_process[i].pr.online_end_ns;
			g_profiler->frame_worker_activity[cpu_process[i].worker][wi[cpu_process[i].worker]].process_state = PROCESS_RUNNING;
			wi[cpu_process[i].worker] += 1;
		}
	}

	//for (u32 i = 0; i < g_profiler->worker_count; ++i)
	//{
	//	kas_assert(wi[i] == g_profiler->frame_worker_activity_count[i]);
	//	fprintf(stderr, "\n");
	//	for (u32 j = 0; j < wi[i]; ++j)
	//	{
	//		fprintf(stderr, "[%lu, %lu] ",
	//				g_profiler->frame_worker_activity[i][j].ns_start,
	//				g_profiler->frame_worker_activity[i][j].ns_end);
	//	}
	//}
}

#elif (__OS__ == __WIN64__)
void kas_profiler_gather_kernel_profiles(void)
{
	return;
}
#endif

void kas_profiler_new_frame(void)
{
	u32 core_tmp;

	arena_flush(&g_profiler->mem);
	g_profiler->ns_frame_prev = g_profiler->ns_frame;
	g_profiler->tsc_frame_prev = g_profiler->tsc_frame;
	g_profiler->ns_frame = time_ns();
	g_profiler->tsc_frame = rdtscp(&core_tmp);
	g_profiler->frame_counter += 1;

	if (g_profiler->level == PROFILE_LEVEL_TASK)
	{
		if (g_profiler->frame_counter == 0) 
		{ 
			g_profiler->header->l1_table.ns_start = g_profiler->ns_frame;
			/* alloc new frame header (write to temporary stubs) in file */
			kaspf_alloc_headers_in_frame(g_profiler, g_profiler->ns_frame, g_profiler->tsc_frame);
			return; 
		}	

		for (u32 i = 0; i < g_profiler->worker_count; ++i)
		{
			/* NOTE: Ordering here very important, see kas_new_frame */
			u32 stack_count = atomic_load_acq_32(&g_profiler->worker_frame[i].stack_count);
			kas_assert(stack_count == 1);

			const struct lw_profile *completed = g_profiler->worker_frame[i].completed;
			g_profiler->worker_frame[i].completed_count = g_profiler->worker_frame[i].build_count;
			g_profiler->worker_frame[i].completed = g_profiler->worker_frame[i].build;
			g_profiler->worker_frame[i].stack_count = 1;
			g_profiler->worker_frame[i].build_count = 1;

			/* NOTE: Ordering here very important, see kas_new_frame */
			atomic_store_rel_64(&g_profiler->worker_frame[i].build, (struct kas_frame *) completed);
		}
		
		if (g_profiler->level == PROFILE_LEVEL_KERNEL)
		{
			/* process kernel event data */
			KAS_TASK("kas kernel gather profile", T_PROFILER);
			//kas_kernel_profile();
			kas_profiler_gather_kernel_profiles();
			KAS_END;
		}

		/* alloc file frame_data and write data to disk */
		KAS_TASK("kaspf_write_completed_frame", T_PROFILER);
		kaspf_write_completed_frame(g_profiler);
		KAS_END;
		/* alloc new frame header (and finish old header(s) by writing time) in file */
		KAS_TASK("kaspf_alloc_headers_in_frame", T_PROFILER);
		kaspf_alloc_headers_in_frame(g_profiler, g_profiler->ns_frame, g_profiler->tsc_frame);
		KAS_END;

		if (g_kaspf_reader->read_state != KASPF_READER_CLOSED)
		{
			if (g_kaspf_reader->read_state == KASPF_READER_STREAM)
			{
				const u64 min_time = g_profiler->header->l1_table.entries[0].ns_start;
				g_kaspf_reader->ns_end = (g_profiler->ns_frame_prev < g_kaspf_reader->ns_stream_interval + min_time)
					? g_kaspf_reader->ns_stream_interval + min_time
					: g_profiler->ns_frame_prev;
				g_kaspf_reader->ns_start = g_kaspf_reader->ns_end - g_kaspf_reader->ns_stream_interval;
			}

			KAS_TASK("kaspf_reader_process", T_PROFILER);
			kaspf_reader_process(&g_profiler->mem);
			KAS_END;
		}
	}
}

#endif
