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

#include <string.h>

#include "kas_profiler.h"
#include "asset_public.h"

#ifdef KAS_PROFILER

static struct kaspf_reader storage = { 0 };
static struct frame_table table_stub = { .ns_start = U64_MAX, .ns_end = U64_MAX };
static struct hw_frame_header hw_h_stub = { 0 };

struct kaspf_reader *g_kaspf_reader = &storage;

void kaspf_reader_alloc(const u64 bufsize)
{
	g_kaspf_reader->persistent = arena_alloc_1MB();
	g_kaspf_reader->buf = ring_alloc(bufsize, NOT_GROWABLE);
	g_kaspf_reader->ui_cache_buf = ring_alloc(16*1024*1024, NOT_GROWABLE);
	g_kaspf_reader->task_info = malloc(sizeof(struct kaspf_task_info) * KASPF_UNIQUE_TASK_COUNT_MAX);
	memset(g_kaspf_reader->task_info, 0, sizeof(struct kaspf_task_info) * KASPF_UNIQUE_TASK_COUNT_MAX);
	g_kaspf_reader->read_state = KASPF_READER_CLOSED;
	g_kaspf_reader->ns_start = 0;
	g_kaspf_reader->ns_end = 0;
	g_kaspf_reader->frame_low = U64_MAX;
	g_kaspf_reader->frame_high = U64_MAX;
	g_kaspf_reader->interval_low[0] = U64_MAX;
	g_kaspf_reader->interval_low[1] = U64_MAX;
	g_kaspf_reader->interval_high[0] = U64_MAX;
	g_kaspf_reader->interval_high[1] = U64_MAX;
	g_kaspf_reader->mm_branch_low[0] = &table_stub;
	g_kaspf_reader->mm_branch_low[1] = &table_stub;
	g_kaspf_reader->mm_branch_high[0] = &table_stub;
	g_kaspf_reader->mm_branch_high[1] = &table_stub;
	g_kaspf_reader->low = &hw_h_stub;
	g_kaspf_reader->high = &hw_h_stub;
}

void kaspf_reader_shutdown(void)
{
	struct hw_frame_header *cur = g_kaspf_reader->low;
	while (cur != &hw_h_stub)
	{
		cur = cur->next;
	}
	ring_dealloc(&g_kaspf_reader->buf);
	ring_dealloc(&g_kaspf_reader->ui_cache_buf);
	arena_free_1MB(&g_kaspf_reader->persistent);
}

//static void internal_schedule_switch_determine_workers(u64 *worker_prev, u64 *worker_next, const struct kas_profiler *profiler, const struct kas_schedule_switch *ss)
//{
//	*worker_prev = U64_MAX;
//	*worker_next = U64_MAX;
//
//	for (u32 i = 0; i < g_profiler->worker_count; ++i)
//	{
//		const tid id = g_profiler->worker_frame[i].thread_id;
//		if (ss->tid_prev == id)
//		{
//			*worker_prev = i;	
//		}
//		else if (ss->tid_next == id)
//		{
//			*worker_next = i;
//		}
//	}
//}

//static void internal_set_wo_variables(u64 *wo_cur, u64 *wo_single_begin, u64 *wo_single_end, const u32 is_prev)
//{
//	if (*wo_cur == 0)
//	{
//		if (is_prev)
//		{
//			*wo_single_begin = 1;
//			*wo_single_end = 0;
//		}
//		else
//		{
//			*wo_single_begin = 0;
//			*wo_single_end = 1;
//		}
//		*wo_cur += 1;
//	}
//	else
//	{
//		if (is_prev)
//		{
//			*wo_single_end = 0;
//		}
//		else
//		{
//			*wo_cur += 1;
//			*wo_single_end = 1;
//		}
//	}
//}

//static u64 internal_frame_calculate_workers_online_time(struct arena *tmp, const struct kt_header *kt_h, const struct kernel_event_header *first)
//{
//	struct arena record = *tmp;
//
//	u64 *wo_single_begin = arena_push(tmp, g_profiler->worker_count * sizeof(u64));
//	u64 *wo_single_end = arena_push(tmp, g_profiler->worker_count * sizeof(u64));	
//	u64 *wo_cur = arena_push(tmp, g_profiler->worker_count * sizeof(u64));	
//
//	for (u32 i = 0; i < g_profiler->worker_count; ++i)
//	{
//		wo_single_begin[i] = 0;
//		wo_single_end[i] = 0;
//		wo_cur[i] = 0;
//	}
//
//	const struct kernel_event_header *ev = first;
//	for (u64 i = 0; i < kt_h->count; ++i)
//	{
//		//kas_assert(ev->type == KERNEL_EVENT_SCHED_SWITCH);
//		if (ev->type == KERNEL_EVENT_SCHED_SWITCH)
//		{
//			const struct kas_schedule_switch *ss = (const struct kas_schedule_switch *) ev;
//			ev = (const struct kernel_event_header *)(((u8 *) ev) + KAS_SCHEDULE_SWITCH_PACKED_SIZE);
//			u64 w_prev;
//			u64 w_next;
//			internal_schedule_switch_determine_workers(&w_prev, &w_next, g_kas, ss);
//			if (w_prev != U64_MAX)
//			{
//				internal_set_wo_variables(wo_cur + w_prev, wo_single_begin + w_prev, wo_single_end + w_prev, 1);
//			}
//			if (w_next != U64_MAX)
//			{
//				internal_set_wo_variables(wo_cur + w_next, wo_single_begin + w_next, wo_single_end + w_next, 0);
//			}
//		}
//	}
//
//	u64 wo_count = 0;
//	for (u32 i = 0; i < g_profiler->worker_count; ++i)
//	{
//		wo_count += wo_cur[i];
//	}
//
//	*tmp = record;
//
//	return wo_count * sizeof(struct hw_worker_online);
//}

u64 internal_frame_size(struct arena *tmp, const struct frame_header *fh)
{
	u64 size = sizeof(struct hw_frame_header);
	size += g_profiler->worker_count*sizeof(struct hw_profile_header) + g_profiler->kernel_buffer_count*sizeof(struct cpu_frame_header);

	struct lw_header *lw_h = (struct lw_header *)(((u8 *) fh) + sizeof(struct frame_header));
	struct kt_header *kt_h = (struct kt_header *)(((u8 *) fh) + sizeof(struct frame_header) + g_profiler->worker_count*sizeof(struct lw_header));

	for (u32 i = 0; i < g_profiler->worker_count; ++i)
	{
		size += lw_h[i].profile_count*sizeof(struct hw_profile);
		size += lw_h[i].activity_count*sizeof(struct worker_activity);
	}

	for (u32 i = 0; i < g_profiler->kernel_buffer_count; ++i)
	{
		size += kt_h[i].pr_count*sizeof(struct process_runtime);
	}
	
	return size;
}

static u64 hw_frames_size(struct arena *tmp, const struct frame_header *fh, const u64 low, const u64 high)
{
	u64 l1_i, l2_i, l3_i;

	//fprintf(stderr, "low, high: %lu, %lu\n", low, high);
	u64 size = 0;
	for (u64 i = low; i <= high; ++i)
	{
		size += internal_frame_size(tmp, fh);

		kaspf_frame_table_indices(&l1_i, &l2_i, &l3_i, i);
		//fprintf(stderr, "fh: { %lu, %lu, %lu }, (%lu, %lu, %lu)\n", fh->ns_start, fh->ns_end, fh->size, l1_i, l2_i, l3_i);
		fh = kaspf_next_header(fh, l1_i, l2_i, l3_i);
	}

	return size;
}

static u64 internal_get_branch_frame(u32 faults[3], struct frame_table *mm_b[2], u64 li[3], struct frame_table *cur_mm_b[2], const u64 cur_li[3], const u64 cur_frame_time[2], const struct kaspf_header *header, const u64 ns_time)
{
	if (ns_time < cur_mm_b[0]->ns_start || cur_mm_b[0]->ns_end < ns_time)
	{
		faults[0] = 1;
		faults[1] = 1;
		faults[2] = 1;
	} 
	else if (ns_time < cur_mm_b[1]->ns_start || cur_mm_b[1]->ns_end < ns_time)
	{
		faults[0] = 0;
		faults[1] = 1;
		faults[2] = 1;
	}
	else if (ns_time < cur_frame_time[0] || cur_frame_time[1] < ns_time)
	{
		faults[0] = 0;
		faults[1] = 0;
		faults[2] = 1;
	}
	else
	{
		faults[0] = 0;
		faults[1] = 0;
		faults[2] = 0;
	}

	if (faults[0])
	{
		li[0] = kaspf_frame_table_index_from_time(&header->l1_table, FRAME_TABLE_SIZE / sizeof(struct ft_entry), ns_time);
		mm_b[0] = file_memory_map_partial(&g_profiler->file, FRAME_TABLE_FULL_SIZE, header->l1_table.entries[li[0]].offset, FS_PROT_READ, FS_MAP_SHARED);
	}
	else
	{
		li[0] = cur_li[0];
		mm_b[0] = cur_mm_b[0];	
	}
		

	if (faults[1])
	{
		li[1] = kaspf_frame_table_index_from_time(mm_b[0], L3_FRAME_COUNT, ns_time);
		mm_b[1] = file_memory_map_partial(&g_profiler->file, FRAME_TABLE_FULL_SIZE, mm_b[0]->entries[li[1]].offset, FS_PROT_READ, FS_MAP_SHARED);
	}
	else
	{
		li[1] = cur_li[1];
		mm_b[1] = cur_mm_b[1];
	}
	
	li[2] = (faults[2]) ? kaspf_frame_table_index_from_time(mm_b[1], L3_FRAME_COUNT, ns_time) : cur_li[2];

	return li[0] * L2_FRAME_COUNT + li[1] * L3_FRAME_COUNT + li[2];
}

/* high and low are both inclusive */
static void internal_discard_frame_range(struct kaspf_reader *reader, const u64 low, const u64 high)
{
	kas_assert(reader->frame_low == low || reader->frame_high == high);

	struct hw_frame_header *cur;
	const u64 frame_count = high-low+1;
	u64 task_popsize = 0;
	u64 ui_cache_popsize = 0;

	if (reader->frame_low == low)
	{
		kas_assert(high <= reader->frame_high);
		cur = reader->low;
		for (u64 i = 0; i < frame_count; ++i)
		{
			task_popsize += cur->size;
			ui_cache_popsize += cur->ui_cache_size;
			kas_assert(cur->size);
			cur = cur->next;
		}
		
		reader->low = cur;
		cur->prev = &hw_h_stub;
		ring_pop_start(&reader->buf, task_popsize);
		ring_pop_start(&reader->ui_cache_buf, ui_cache_popsize);
		reader->frame_low = high+1;
	}
	else
	{
		kas_assert(reader->frame_low <= low);
		cur = reader->high;
		for (u64 i = 0; i < frame_count; ++i)
		{
			task_popsize += cur->size;
			ui_cache_popsize += cur->ui_cache_size;
			kas_assert(cur->size);
			cur = cur->prev;
		}

		reader->high = cur;
		cur->next = &hw_h_stub;
		ring_pop_end(&reader->buf, task_popsize);
		ring_pop_end(&reader->ui_cache_buf, ui_cache_popsize);
		reader->frame_high = low-1;
	}

	if (cur == &hw_h_stub)
	{
		reader->low = &hw_h_stub;
		reader->high = &hw_h_stub;
		reader->frame_low = U64_MAX;
		reader->frame_high = U64_MAX;
	}
}

static void internal_process_worker_profiles(struct arena *frame_persistent, struct hw_profile_header *hw_h, struct lw_profile *lw_profiles, const u64 ns_frame, const u64 tsc_frame, const u32 worker, const u64 frame)
{
	//u64 wo_cur = 0;
	for (u64 i = 0; i < hw_h->profile_count; ++i)
	{
		struct hw_profile *hw_p = hw_h->profiles + i;
		struct lw_profile *lw_p = lw_profiles + i;
		hw_p->ns_start = time_ns_from_tsc_truth_source(lw_p->tsc_start, ns_frame, tsc_frame);
		hw_p->ns_end = time_ns_from_tsc_truth_source(lw_p->tsc_end, ns_frame, tsc_frame);
		hw_p->ns_in_children = 0;
		hw_p->task_id = lw_p->task_id;
		hw_p->parent = lw_p->parent-1;	/* parent index, U32_MAX for no parent */
		hw_p->child_tasks = 0;
		hw_p->cache = ui_node_cache_null();

		hw_p->id = utf8_format(frame_persistent, "t%u_%lu_%u", worker, frame, i);
		struct kaspf_task_info *info = g_kaspf_reader->task_info + lw_p->task_id;
		if (!info->initiated)
		{
			info->initiated = 1;
			info->system = g_profiler->header->mm_task_systems[lw_p->task_id];
			info->id = utf32_cstr(&g_kaspf_reader->persistent, (const char *) g_profiler->header->mm_labels[lw_p->task_id]);
			struct asset_font *asset = asset_database_request_font(&g_kaspf_reader->persistent, FONT_DEFAULT_SMALL);
			info->layout = utf32_text_layout(&g_kaspf_reader->persistent, &info->id, F32_INFINITY, TAB_SIZE, asset->font);
		}

		if (lw_p->parent)
		{
			struct hw_profile *parent = hw_h->profiles + hw_p->parent;
			hw_p->depth = parent->depth + 1;
		}
		else
		{
			hw_p->depth = 0;		
		}
	}
}

static void internal_process_frames(struct arena *tmp, struct kas_buffer *buf, const struct frame_header *fh, const u64 low, const u64 high, struct hw_frame_header *prev, struct hw_frame_header *last)
{
	arena_push_record(tmp);
	struct lw_profile **lw_ps = arena_push(tmp, g_profiler->worker_count * sizeof(struct lw_profile *));

	struct arena tmp1 = arena_alloc_1MB();

	const u32 push_end = (prev == &hw_h_stub);

	struct hw_frame_header *hw;
	for (u64 frame = low; frame <= high; ++frame)
	{
		hw = (struct hw_frame_header *) (buf->data + (buf->size - buf->mem_left));
		hw->size = buf->mem_left;	/* push mem_left, substract mem_left when done */
		hw->ui_cache_size = tmp1.mem_left;
		buf->mem_left -= sizeof(struct hw_frame_header);
		hw->ns_start = fh->ns_start;
		hw->ns_end = fh->ns_end;
		hw->tsc_start = fh->tsc_start;
		hw->tsc_end = fh->tsc_end;
		hw->prev = prev;
		prev->next = hw;
		prev = hw;

		hw->hw_profile_h = (struct hw_profile_header *) (buf->data + (buf->size - buf->mem_left));
		buf->mem_left -= sizeof(struct hw_profile_header) * g_profiler->worker_count;
		hw->cpu_h = (struct cpu_frame_header *) (buf->data + (buf->size - buf->mem_left));
		buf->mem_left -= sizeof(struct cpu_frame_header) * g_profiler->kernel_buffer_count;

		struct lw_header *lw_h = (struct lw_header *)(((u8 *) fh) + sizeof(struct frame_header));
		struct kt_header *kt_h = (struct kt_header *)(((u8 *) fh) + sizeof(struct frame_header) + g_profiler->worker_count*sizeof(struct lw_header));
		u8 *next_read_data = ((u8 *) fh + sizeof(struct frame_header) 
					   + g_profiler->worker_count*sizeof(struct lw_header)
					   + g_profiler->kernel_buffer_count*sizeof(struct kt_header));

		for (u32 j = 0; j < g_profiler->worker_count; ++j)
		{
			lw_ps[j] = (struct lw_profile *) next_read_data;
			hw->hw_profile_h[j].profile_count = lw_h[j].profile_count;
			hw->hw_profile_h[j].profiles = (struct hw_profile *) (buf->data + (buf->size - buf->mem_left));
			next_read_data += lw_h[j].profile_count * sizeof(struct lw_profile);
			buf->mem_left -= sizeof(struct hw_profile) * lw_h[j].profile_count;

			const u64 data_size = lw_h[j].activity_count * sizeof(struct worker_activity);
			hw->hw_profile_h[j].activity_count = lw_h[j].activity_count;
			hw->hw_profile_h[j].activity = (struct worker_activity *) (buf->data + (buf->size - buf->mem_left));
			memcpy(hw->hw_profile_h[j].activity, next_read_data, data_size);
			next_read_data += data_size;
			buf->mem_left -= data_size;
		}

		for (u32 j = 0; j < g_profiler->kernel_buffer_count; ++j)
		{
			hw->cpu_h->pr_count = kt_h[j].pr_count;
			hw->cpu_h->pr = (struct process_runtime *) (buf->data + (buf->size - buf->mem_left));
			const u64 data_size = sizeof(struct process_runtime) * kt_h[j].pr_count;
			buf->mem_left -= data_size;
			memcpy(hw->cpu_h->pr, next_read_data, data_size);
			next_read_data += data_size;
		}

		//struct kernel_event_header *first = (struct kernel_event_header *) next_read_data;
		//next_read_data += kt_h->ss_count * KAS_SCHEDULE_SWITCH_PACKED_SIZE;
		//kas_assert_string(kt_h->count == kt_h->ss_count, "We currently only expects kernel\n header to contain schedule switches,\n update kaspf_reader size calculations\n to account for new types");
		//internal_process_frame_kernel_events(tmp, buf, hw, kt_h, first);

		for (u32 j = 0; j < g_profiler->worker_count; ++j)
		{
			internal_process_worker_profiles(&tmp1, hw->hw_profile_h + j, lw_ps[j], hw->ns_start, hw->tsc_start, j, frame);
		}

		
		hw->size -= buf->mem_left;
		hw->ui_cache_size -= tmp1.mem_left;

		u64 l1, l2, l3;
		kaspf_frame_table_indices(&l1, &l2, &l3, frame);
		fh = kaspf_next_header(fh, l1, l2, l3);
	}
	kas_assert(buf->mem_left == 0);

	prev->next = last;
	last->prev = prev;

	const u64 ui_cache_mem = tmp1.mem_size - tmp1.mem_left;
	struct kas_buffer cache_buf = (push_end)
		? ring_push_end(&g_kaspf_reader->ui_cache_buf, ui_cache_mem)
		: ring_push_start(&g_kaspf_reader->ui_cache_buf, ui_cache_mem);

	struct arena cache_arena = 
	{
		.stack_ptr = cache_buf.data,
		.mem_size = cache_buf.size,
		.mem_left = cache_buf.size,
		.record = NULL,
	};

	/* Lazy; redo the ui cache work in a correct placed buffer */
	buf->mem_left = buf->size;
	for (u64 frame = low; frame <= high; ++frame)
	{
		hw = (struct hw_frame_header *) (buf->data + (buf->size - buf->mem_left));
		buf->mem_left -= hw->size;

		for (u32 j = 0; j < g_profiler->worker_count; ++j)
		{
			for (u64 i = 0; i < hw->hw_profile_h[j].profile_count; ++i)
			{
				struct hw_profile *hw_p = hw->hw_profile_h[j].profiles + i;
				hw_p->id = utf8_format(&cache_arena, "t%u_%lu_%u", j, frame, i);
			}
		}

		hw = hw->next;
	}
	kas_assert(cache_arena.mem_left == 0);
	kas_assert(buf->mem_left == 0);

	/* last frame to be processed pads the rest of the buffer */
		
	arena_free_1MB(&tmp1);
	arena_pop_record(tmp);
}

void kaspf_reader_stream(const u64 ns_interval)
{
	g_kaspf_reader->read_state = KASPF_READER_STREAM;
	g_kaspf_reader->ns_stream_interval = ns_interval;
}

void kaspf_reader_fixed(const u64 ns_start, const u64 ns_end)
{
	kas_assert(ns_start <= ns_end);
	g_kaspf_reader->read_state = KASPF_READER_FIXED;
	g_kaspf_reader->ns_start = ns_start;
	g_kaspf_reader->ns_end = ns_end;
	g_kaspf_reader->ns_stream_interval = ns_end - ns_start;
}

void kaspf_reader_process(struct arena *tmp)
{
	u64 ns_start = g_kaspf_reader->ns_start;
	u64 ns_end = g_kaspf_reader->ns_end;
	kas_assert(ns_start <= ns_end);

	if (ns_start < g_profiler->header->l1_table.ns_start)
	{
		ns_end = g_profiler->header->l1_table.ns_start + (ns_end - ns_start);
		ns_start = g_profiler->header->l1_table.ns_start;
	}

	if (g_profiler->ns_frame_prev < ns_end)
	{
		ns_end = g_profiler->ns_frame_prev;
	}

	struct kaspf_reader *reader = g_kaspf_reader;

	u32 faults_low[3];		/* mmap faults for low branch */
	u32 faults_high[3];		/* mmap faults for low branch */
	u64 new_li_low[3];		/* layer indices to frame_low */
	u64 new_li_high[3];		/* layer indices to frame_high */

	/* 
	 * for quick access and frame searching we memory map the frame_tables which contain the low
	 * and high frame, reducing the amount of times we have to memory map file regions. 
	 */
	struct frame_table *new_mm_branch_low[2];
	struct frame_table *new_mm_branch_high[2];
	
	const u64 new_frame_low = internal_get_branch_frame(faults_low, new_mm_branch_low, new_li_low, reader->mm_branch_low, reader->li_low, reader->interval_low, g_profiler->header, ns_start);
	const u64 new_frame_high = internal_get_branch_frame(faults_high, new_mm_branch_high, new_li_high, reader->mm_branch_high, reader->li_high, reader->interval_high, g_profiler->header, ns_end);

	struct kas_buffer buf, map;
	const struct frame_header *fh;

	/* no work to be saved, re-process whole new span */
	if (reader->frame_high < new_frame_low || new_frame_high < reader->frame_low)
	{
		struct hw_frame_header *cur = reader->low;
		while (cur != &hw_h_stub)
		{
			cur = cur->next;
		}
			
		ring_flush(&reader->buf);
		ring_flush(&reader->ui_cache_buf);
		/* get file map */
		{
			const u64 low_offset = new_mm_branch_low[1]->entries[new_li_low[2]].offset;
			u64 high_offset = new_mm_branch_high[1]->entries[new_li_high[2]].offset;
			struct frame_header *frame = file_memory_map_partial(&g_profiler->file, 
					sizeof(struct frame_header),
				       	high_offset,
					FS_PROT_READ,
					FS_MAP_SHARED);
			high_offset += frame->size;
			file_memory_unmap(frame, sizeof(struct frame_header));

			map.size = high_offset - low_offset;
			map.data = file_memory_map_partial(&g_profiler->file, map.size, low_offset, FS_PROT_READ, FS_MAP_SHARED);
			fh = (const struct frame_header *) map.data;
		}

		const u64 size = hw_frames_size(tmp, fh, new_frame_low, new_frame_high);
		struct kas_buffer buf = ring_push_end(&reader->buf, size);
		internal_process_frames(tmp ,&buf, fh, new_frame_low, new_frame_high, &hw_h_stub, &hw_h_stub);
		reader->low = hw_h_stub.next;
		reader->high = hw_h_stub.prev;
		file_memory_unmap(map.data, map.size);
	}
	/* overlap, some work can be saved */
	else
	{
		if (reader->frame_low < new_frame_low)
		{
			internal_discard_frame_range(reader, reader->frame_low, new_frame_low-1);
		}
		else if (reader->frame_low > new_frame_low)
		{
			/* get file map */
			{
				const u64 low_offset = new_mm_branch_low[1]->entries[new_li_low[2]].offset;
				const u64 high_offset = reader->mm_branch_low[1]->entries[reader->li_low[2]].offset;
				map.size = high_offset - low_offset;
				map.data = file_memory_map_partial(&g_profiler->file, map.size, low_offset, FS_PROT_READ, FS_MAP_SHARED);
				fh = (const struct frame_header *) map.data;
			}

			const u64 size = hw_frames_size(tmp, fh, new_frame_low, reader->frame_low-1);
			struct kas_buffer buf = ring_push_start(&reader->buf, size);
			internal_process_frames(tmp, &buf, fh, new_frame_low, reader->frame_low-1, &hw_h_stub, reader->low);
			reader->low = hw_h_stub.next;
			file_memory_unmap(map.data, map.size);
		}

		if (reader->frame_high < new_frame_high)
		{
			/* get file map */
			{
				u64 low_offset = reader->mm_branch_high[1]->entries[reader->li_high[2]].offset;
				u64 high_offset = new_mm_branch_high[1]->entries[new_li_high[2]].offset;
				struct frame_header *frame = file_memory_map_partial(&g_profiler->file, 
						sizeof(struct frame_header),
					       	low_offset,
						FS_PROT_READ,
						FS_MAP_SHARED);
				//fprintf(stderr, "reading frame %lu at offset %lu\n", reader->frame_high, low_offset);
				low_offset += (u64) kaspf_next_header(frame, reader->li_high[0], reader->li_high[1], reader->li_high[2]) - (u64) frame;
				file_memory_unmap(frame, sizeof(struct frame_header));

				frame = file_memory_map_partial(&g_profiler->file, 
						sizeof(struct frame_header),
					       	high_offset,
						FS_PROT_READ,
						FS_MAP_SHARED);
				//fprintf(stderr, "reading frame %lu at offset %lu\n", new_frame_high, high_offset);
				high_offset += frame->size;

				file_memory_unmap(frame, sizeof(struct frame_header));

				map.size = high_offset - low_offset;
				map.data = file_memory_map_partial(&g_profiler->file, map.size, low_offset, FS_PROT_READ, FS_MAP_SHARED);
				//fprintf(stderr, "reading frame %lu at offset %lu\n", reader->frame_high+1, low_offset);

				fh = (const struct frame_header *) map.data;
			}

			const u64 size = hw_frames_size(tmp, fh, reader->frame_high+1, new_frame_high);
			struct kas_buffer buf = ring_push_end(&reader->buf, size);
			internal_process_frames(tmp, &buf, fh, reader->frame_high+1, new_frame_high, reader->high, &hw_h_stub);
			reader->high = hw_h_stub.prev;
			file_memory_unmap(map.data, map.size);
		}
		else if (reader->frame_high > new_frame_high)
		{
			internal_discard_frame_range(reader, new_frame_high+1, reader->frame_high);
		}
	}

	if (faults_low[0])
	{
		file_memory_unmap(reader->mm_branch_low[0], FRAME_TABLE_FULL_SIZE);
		reader->mm_branch_low[0] = new_mm_branch_low[0];
	}

	if (faults_low[1])
	{
		file_memory_unmap(reader->mm_branch_low[1], FRAME_TABLE_FULL_SIZE);
		reader->mm_branch_low[1] = new_mm_branch_low[1];
	}

	if (faults_high[0])
	{
		file_memory_unmap(reader->mm_branch_high[0], FRAME_TABLE_FULL_SIZE);
		reader->mm_branch_high[0] = new_mm_branch_high[0];
	}

	if (faults_high[1])
	{
		file_memory_unmap(reader->mm_branch_high[1], FRAME_TABLE_FULL_SIZE);
		reader->mm_branch_high[1] = new_mm_branch_high[1];
	}

	reader->li_low[0] = new_li_low[0];
	reader->li_low[1] = new_li_low[1];
	reader->li_low[2] = new_li_low[2];

	reader->li_high[0] = new_li_high[0];
	reader->li_high[1] = new_li_high[1];
	reader->li_high[2] = new_li_high[2];

	reader->frame_low = new_frame_low;
	reader->frame_high = new_frame_high;

	const u64 l = new_li_low[2];
	const u64 h = new_li_high[2];

	reader->interval_low[0] = new_mm_branch_low[1]->entries[l].ns_start;
	reader->interval_low[1] = (l+1 == L3_FRAME_COUNT) 
		? new_mm_branch_low[1]->ns_end
		: new_mm_branch_low[1]->entries[l+1].ns_start;

	reader->interval_high[0] = new_mm_branch_high[1]->entries[h].ns_start;
	reader->interval_high[1] = (h+1 == L3_FRAME_COUNT) 
		? new_mm_branch_high[1]->ns_end
		: new_mm_branch_high[1]->entries[h+1].ns_start;

	fprintf(stderr, "frames: [%lu, %lu]\ttask ring buffer mem left: [%lu]\tui cache buffer mem left: [%lu]\ttime interval (%.7f(s), %7f(s))\n",
		reader->frame_low,
		reader->frame_high,
		reader->buf.mem_left,
		reader->ui_cache_buf.mem_left,
		(f64) reader->interval_low[0] / NSEC_PER_SEC,
		(f64) reader->interval_high[1] / NSEC_PER_SEC);
}

#endif
