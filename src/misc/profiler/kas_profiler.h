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

#ifndef __KAS_PROFILER_H__
#define __KAS_PROFILER_H__

#include "kas_common.h"

#include "sys_public.h"
#include "ui_public.h"

/*
 * Some short notes of timestamping. We expect to find rdtsc, rdtscp supported and that the tsc is INVARIANT, i.e,
 * the counter on all cpus run on a fixed frequency (tsc_contant on linux) and is not reset or anything on sleep
 * and such (this is probably what is meant with ALL ACPI P-, C-, and T-states). The counter can then be used as
 * a way of measuring elapsed wall-clock time locally on a cpu.
 *
 * Now, if kernel tracing is used, at least on linux for now, we may specify if we want timestamps using a tsc
 * for the kernel events. The kernel can, however, ignore that request, and give us results probably using
 * CLOCK_MONOTONIC_RAW, which probably happens when it deems the counter unstable. Perhaps this instability
 * can occur when the machine has been running for a long time, or when it has been put to sleep for a longer
 * time. Nonetheless, the kernel profiler should be able to handle that by using the more costly 
 * CLOCK_MONOTONIC_RAW.
 */
  
#if defined (__GCC__) && defined(__SANITIZE_THREAD__)
#define TSAN_HELPER
#endif 

/*********************************************************************************/
/**                        	KAS PROFILER FORMAT	                        **/
/*********************************************************************************/

struct lw_header;
struct kt_header;
struct frame_header;
struct frame_table;
struct kaspf_header;
struct kas_profiler;

/* header for a single worker's frame data */
struct lw_header
{
	u64 profile_offset;	/* offset to first profile */
	u64 profile_count;	/* profile count */
	u64 activity_offset;	/* worker activity array offset */
	u64 activity_count;	/* worker activity array size */
	
};

/* header for a single kernel tracer buffer's frame data */
struct kt_header
{
	u64 pr_offset;	/* process_runtime array offset */
	u64 pr_count;	/* pr[pr_count] */
};

/*
 * ==================== (2) File Format: KasProfileFormat kaspf ====================
 *
 * Layout: Every entry below is page aligned (including every frame) 
 *	[   
 *	    kaspf_header(KASPF_HEADER_SIZE)
 *	  | task_labels(KASPF_LABEL_TABLE_SIZE)
 *	  | L2_TABLE(FRAME_TABLE_FULL_SIZE)
 *	  | L3_TABLE(FRAME_TABLE_FULL_SIZE)
 *	  | Frame 0 | ... | Frame L3_FRAME_COUNT-1
 *	  | L3_TABLE(FRAME_TABLE_FULL_SIZE)
 *	  | Frame L3_FRAME_COUNT | ... | Frame 2*L3_FRAME_COUNT-1
 *	  ...
 *	  | ... | L2_FRAME_COUNT-1
 *	  | L2_TABLE(FRAME_TABLE_FULL_SIZE)
 *	  | L3_TABLE(FRAME_TABLE_FULL_SIZE)
 *	  | Frame 0 | ... | Frame L3_FRAME_COUNT-1
 *	  ...
 *	]
 */
struct frame_header
{
	u64	ns_start;		/* os ns_timer at start of frame */
	u64	ns_end;			/* os ns_timer at end of last frame in table */
	u64 	tsc_start;		/* cpu tsc value corresponding to tsc(system_time( time )), 
			   		   i.e. the synced cpu tsc value at the time of when time was 
			   		   taken */
	u64	tsc_end;
	u64	size;			/* sizeof(frame_header) + sizeof(lw_header[]) 
					   + sizeof(kt_header[]) + data_size */

	/*
	 *	struct lw_header[worker_count]
	 *	struct kt_header[1]
	 *	u8 rawdata[data_size]
	 */
	u8	data[];
};

struct ft_entry
{
	u64 ns_start;
	u64 offset;
};

#define KASPF_HEADER_SIZE	8192
#define FRAME_TABLE_SIZE	4080	/* size of entry array in frame_header_tale */
#define FRAME_TABLE_FULL_SIZE	sizeof(struct frame_table)
#define L3_FRAME_COUNT		(FRAME_TABLE_SIZE / 16)
#define L2_FRAME_COUNT		(L3_FRAME_COUNT*L3_FRAME_COUNT)
#define L1_FRAME_COUNT		(L3_FRAME_COUNT*L3_FRAME_COUNT*L3_FRAME_COUNT)

struct frame_table
{
	u64	ns_start;				/* os ns_timer at start of first frame in table */
	u64	ns_end;					/* os ns_timer at end of last frame in table */
	struct ft_entry entries[L3_FRAME_COUNT];	/* offsets into file to a frame_table or a frame_header */
};

#define KASPF_UNIQUE_TASK_COUNT_MAX	1024
#define KASPF_LABEL_BUFSIZE		64
#define KASPF_LABEL_TABLE_SIZE	(KASPF_LABEL_BUFSIZE * KASPF_UNIQUE_TASK_COUNT_MAX)

#define KASPF_MAJOR		1
#define KASPF_MINOR		0
/*
 * Found at offset 0 within kaspf file. header should always be of size 4096.
 */
struct kaspf_header
{
	u32			major;			/* major version */
	u32			minor;			/* minor version */
	u64			frame_count;		/* frame count in file */
	u64			worker_count;		/* thread count used in the profiling */
	u64			kernel_buffer_count;	/* kernel buffers used in the profiling */
	i32			pid;			/* process id */
	u64			page_size;		/* os page size */
	u64			clock_freq;		/* accurate os clock frequency */
	u64			rdtsc_freq;		/* cpu tsc frequency */
	u64			bytes;			/* bytes in file, used for allocation */
	void *			mm_branch[3];		/* Current branch in the frame header table 
							   hierarchy being built, memory mapped. 
							   [L2_TABLE -> L3_TABLE -> FRAME */
	u32			task_count_max;
	u8			(*mm_labels)[KASPF_LABEL_BUFSIZE];	/* ascii-null terminated strings (c strings) */
	u32 *			mm_task_systems;	/* table[task.id] = USER_SUSBSYSTEM_IDENTIFIER */

	u8 			pad[3976];

	/* page - aligned */
	struct frame_table 	l1_table;
};

/* caclulate layer 1,2,3 indices for given frame */
void kaspf_frame_table_indices(u64 *l1_i, u64 *l2_i, u64 *l3_i, const u64 frame);
/* Returns index of table entry whose time interval contains ns_time (Or U64_MAX on out-of-range) */
u64 kaspf_frame_table_index_from_time(const struct frame_table *table, const u64 table_len, const u64 ns_time);
/* returns the address of the following frame_header */
struct frame_header *kaspf_next_header(const struct frame_header *fh, const u64 l1_i, const u64 l2_i, const u64 l3_i);

/*********************************************************************************/
/**                        	KAS PROFILE READER                            	**/
/*********************************************************************************/

struct hw_profile
{
	struct ui_node_cache	cache;
	utf8	id;
	u64 	ns_start;
	u64 	ns_end;
	u64 	ns_in_children;
	u32	parent;
	u32	id_hash;
	u32	child_tasks;	/* TODO: number of tasks decendant of profile */
	u32	depth;		/* depth in task tree, range [0, U32_MAX] */
	u16	task_id;	/* unique id for every specific task in the codebase. Retrieved at first creation
				   of specific task, which associates the id with the specific label provided. */
};

/* heavy weight profile  header */
struct hw_profile_header
{
	struct hw_profile *profiles;
	struct worker_activity *activity;
	u64 profile_count;
	u64 activity_count;
};

/* cpu acitivty header */
struct cpu_frame_header
{
	struct process_runtime *pr;
	u32			pr_count; 
};

/* heavy weight frame header */
struct hw_frame_header
{
	u64				ui_cache_size;
	struct hw_frame_header *	prev;		/* previous hw_frame in ring buffer, or NULL */
	struct hw_frame_header *	next;  		/* next hw_frame in ring buffer, or NULL */
	u64				ns_start;
	u64				ns_end;

	/* cpu tsc value corresponding to tsc(system_time( ns_time )), 
	   i.e. the synced cpu tsc value at the time of when time was 
	   taken */
	u64 				tsc_start;		
	u64				tsc_end;
	struct hw_profile_header * 	hw_profile_h;	/* hw_headers[worker_count] */
	struct cpu_frame_header *	cpu_h;		/* cpu[kernel_buffer_count] */
	u64 				size;		/* total size for whole hw_frame in buffer */
};

enum kaspf_reader_state
{
	KASPF_READER_CLOSED,
	KASPF_READER_FIXED,
	KASPF_READER_STREAM,
	KASPF_READER_COUNT,
};

struct kaspf_task_info
{
	utf32 			id;
	struct text_layout *	layout;
	u8 			initiated;
	u8 			system;
};

struct kaspf_reader
{
	struct arena		persistent;
	struct ring 		buf;		/* heavy weight frame data buffer */
	struct ring 		ui_cache_buf;	/* heavy weight frame data buffer */
	struct kaspf_task_info *task_info;	/* overview information of every unique task that has been profiled */
	struct hw_frame_header *low;		/* points to the first processed frame in buf */
	struct hw_frame_header *high;		/* points to the last processed frame in buf */

	enum kaspf_reader_state read_state;	/* helper to keep track of reader state */

	/* The frames processed will have the smallest superset interval that contains the user requested time interval */
	u64			ns_stream_interval; /* When streaming, ns_start, is derived from the stream interval size   */
	u64 			ns_start;	/* start of user requested interval (nanoseconds) [ <= ns_start in process() ] */
	u64 			ns_end;		/* end of user requested interval (nanoseconds)   [ >= ns_end in process()   ] */

	u64			frame_low;	/* lowest frame found in buffer (U64_MAX on none) */
	u64			frame_high;	/* highest frame found in buffer (U64_MAX on none) */

	u64			li_low[3];		/* layer indices to frame_low */
	u64			li_high[3];		/* layer indices to frame_high */
	u64			interval_low[2];	/* frame low time interval { ns_start, ns_end } */
	u64			interval_high[2]; 	/* frame high time interval { ns_start, ns_end } */
	/* 
	 * for quick access and frame searching we memory map the frame_tables which contain the low
	 * and high frame, reducing the amount of times we have to memory map file regions. 
	 */
	struct frame_table *	mm_branch_low[2];
	struct frame_table *	mm_branch_high[2]; 
};

extern struct kaspf_reader *g_kaspf_reader;

/* alloc new kaspf_reader */
void	kaspf_reader_alloc(const u64 bufsize);
/* free (and unmap) any memory used by the reader */
void    kaspf_reader_shutdown(void);
/* set kaspf to stream with fixed interval width */
void	kaspf_reader_stream(const u64 ns_interval);
/* set kaspf to fixed with given start and end */
void	kaspf_reader_fixed(const u64 ns_start, const u64 ns_end);
/* process any data in the kaspf file (if needed) and set the ring buffer accordingly  */
void 	kaspf_reader_process(struct arena *tmp);


/******************* kernel tracing structs and utilities *******************/

enum kernel_event_type
{
	KERNEL_EVENT_SCHED_SWITCH,
	KERNEL_EVENT_SCHED_WAKING,
};

struct kernel_event_header
{
	u64 time;	/* process time in ns */
	enum kernel_event_type type;
};

enum process_state
{
	PROCESS_WAKING,			/* Process waking up */
	PROCESS_RUNNING,		/* Running or runnable */
	PROCESS_SLEEPING,		/* interruptable sleep */
	PROCESS_BLOCKED,		/* uninterruptable sleep, perhaps blocking on IO */
	PROCESS_UNHANDLED_STATE,	/* unhandled state we don't currently care about  */
	PROCESS_COUNT,
};

/* returns an alias into string table, TREAT returned strings as const */
utf8 utf8_alias_process_state_string(const enum process_state state);

#define PROCESS_NON_WAKING	U64_MAX
struct process_runtime
{
	u64	waking_start_ns;	/* process-time for when process is in waking context, for non-waking
					   processes, set this to 0. */
	u64	online_start_ns;	/* start and end process-time for when task is actually running */
	u64	online_end_ns;
	tid	pid;				/* thread (process) id */
	enum	process_state state_end;	/* state of process when switched out */
	u8	process[16];			/* null-terminated process name */
};

/*
 * Kernel event from when the given process or thread is waking up 
 * 	- Linux: From within the waking context
 * 	- Windows. TODO
 */
struct kas_schedule_waking
{
	/* Should always exist in the following order in every kas_{kernel_event}, so we can cast struct to header. */
	u64	time;
	enum	kernel_event_type type;

	tid	pid;
	u8	process[16];
	u8 	cpu;
};
#define KAS_SCHEDULE_WAKING_PACKED_SIZE	(sizeof(u64) + sizeof(enum kernel_event_type) + sizeof(tid) + 16*sizeof(u8) + sizeof(u8))

/*
 * Kernel event from when the next process or thread has fully woken up, switching out the previous one with state_prev
 * 	- Linux: From within the waking context
 * 	- Windows. TODO
 */
struct kas_schedule_switch
{
	/* Should always exist in the following order in every kas_{kernel_event}, so we can cast struct to header. */
	u64	time;
	enum	kernel_event_type type;

	enum	process_state state_prev;
	tid 	tid_prev;
	tid 	tid_next;
	u8	process_prev[16];
	u8	process_next[16];
	u8 	cpu;
};
#define KAS_SCHEDULE_SWITCH_PACKED_SIZE	(sizeof(u64) + sizeof(enum kernel_event_type) + sizeof(enum process_state) + sizeof(tid) + sizeof(tid) + 32*sizeof(u8) + sizeof(u8))

void static_assert_kernel_event_header(void);
void static_assert_kas_schedule_switch(void);
void static_assert_kas_schedule_waking(void);

void static_assert_process_runtime(void);

void process_runtime_debug_print(const struct process_runtime *pr);

/******************* profiling structs *******************/

/*
 * worker thread system activity - contains information related to low level scheduling of the worker.
 */
struct worker_activity
{
	u64	ns_start;
	u64	ns_end;
	enum process_state process_state;
};

/*
 * This we write at runtime into the kaspf file, keeping it small and simple. Together with the lightweight
 * data, kernel tracing data and general frame data, we can create the heavyweight profiles which are to be
 * used for the actual profile viewing.
 */
struct lw_profile
{
	u64	tsc_start;
	u64	tsc_end;
	u32	core_start;
	u32	core_end;
	u32	parent;		/* frame id of parent which is it's index. Thus, if we chronologically traverse
				   the array of lw_profiles for a frame, if the parent id decreases from one step
				   to another, we know that the previous parents' task graph has been completed,
				   assuming that the first task in the array is always a stub, or dummy task. */

	u16	task_id;	/* unique id for every specific task in the codebase. Retrieved at first creation
				   of specific task, which associates the id with the specific label provided. */
};

/* Full double buffered thread frame memory */
struct kas_frame
{
	tid				thread_id;
	u64 				worker_id;	
	struct lw_profile *		completed;	/* master owned */
	struct lw_profile *		build;		/* thread owned */
	u32 *				build_stack;	/* thread build stack, used to index build for profiles in progress */
	u32 				completed_count;	
	u32 				build_count;	
	u32				stack_count;	
	u32				frame_len;
	u32				stack_len;
	u32				master_owned;
};

enum profile_level
{
	PROFILE_LEVEL_SYSTEM = 0,	/* lightweight; tracks system level performance TODO 	*/
	PROFILE_LEVEL_TASK = 1,		/* heavyweight; tracks task level performance   	*/
	PROFILE_LEVEL_KERNEL = 2,	/* heavyweight; tracks task+kernel level performance   	*/
};

struct kas_profiler
{
	enum profile_level	level;		/* granularit of profiling */
	struct file		file;		/* memory mapped file */
	struct kaspf_header *   header;		/* header at offset 0 of file */
	struct arena 		mem;		/* helper arena */

	u64			ns_frame;	/* start time of current frame being built */
	u64			ns_frame_prev;	/* start time of previous frame (completed frame) */
	u64 			tsc_frame;	/* cpu tsc value corresponding to tsc(system_time( ns_frame )), 
			   		   	  i.e. the synced cpu tsc value at the time of when time was 
			   		   	  taken */
	u64			tsc_frame_prev;

	u64			frame_counter;	/* elapsed frames */
	u64			clock_freq;	/* os clock frequency */
	u64			rdtsc_freq;	/* tsc clock frequency */
	u32			a_next_task_id;	/* atomically incremented task id retrieval */

	struct kas_frame *	worker_frame;
	u32 		  	worker_count;
	u32			tls_i;		/* Next frame index for tls */

	/* persistent constructions of cpu activity and thread activities between frames.
	 * If an activity has not ended at the end of a frame, say an online activity just started
	 * on some worker, we simply assign that activity to the following frame, so a frame is allowed
	 * to have acitivities assign to it that started before it. 
	 *
	 * We will usually be constructing two runtimes at a time, so we keep track of which one is currently
	 * online by the online index. 
	 */
	struct process_runtime *pr[2];		/* pr[2][kernel_buffer_count] */
	u32 *			online_index;	/* online_index[kernel_buffer_count] = 0,1 (index) */
	struct worker_activity *worker_activity;/* worker_activity[worker_count] */

	u32 *frame_worker_activity_count;
	struct worker_activity **frame_worker_activity;

	/* set on call to kas_gather_kernel_profiles */
	u32 *frame_pr_count;
	struct process_runtime **frame_pr;		/* per-cpu array of finished process runtimes during frame
       							   [cpu_count][cpu_process_count]			   */

	u32 			kernel_buffer_count;
	struct kernel_tracer *	kt;
};

extern struct kas_profiler *g_profiler;
extern kas_thread_local struct kas_frame *tls_frame;

void	kas_profiler_init(struct arena *mem, const u64 master_thread_id, const u32 worker_count, const u32 frame_len, const u32 stack_len, const u64 rdtsc_freq, const enum profile_level level);
void 	kas_profiler_shutdown(void);
void 	kas_profiler_new_frame(void);

/* Should be called once for every worker thread => every thread gets a unique index into the kas_profiler frame array */
#define TASK_ID_VAR(suf)			__task_id ## suf
#define SETTING_VAR(suf)			__setting ## suf
#define _KAS_TASK(label, system, suf)									\
{													\
	static volatile u32 TASK_ID_VAR(suf) = U32_MAX;							\
	if (TASK_ID_VAR(suf) == U32_MAX)								\
	{                                                                                               \
		static volatile u32 SETTING_VAR(suf) = 0;						\
		kas_profiler_try_set_task_id(&TASK_ID_VAR(suf), &SETTING_VAR(suf), label, system);	\
	}                                                                                               \
	/* NOTE: Ordering here very important, see kas_new_frame */					\
	struct kas_entry *build = (struct kas_entry *) atomic_load_acq_64(&tls_frame->build);		\
	const u32 next = tls_frame->build_count;							\
	tls_frame->build_count += 1;									\
													\
	kas_assert_string(next < tls_frame->frame_len, "next < tls_frame->frame_len, increase profiler size");	\
	kas_assert_string(tls_frame->stack_count < tls_frame->stack_len, "tls_frame->stack_count < tls_frame->stack_len, increase profiler size");	\
													\
	/* (2) reserve frame_entry and push index to stack */						\
	const u32 parent = tls_frame->build_stack[tls_frame->stack_count-1];				\
	tls_frame->build_stack[tls_frame->stack_count++] = next;					\
													\
	/* (3) write initial stuff to entry */								\
	struct lw_profile *p = tls_frame->build + next;							\
	p->parent = parent;										\
	p->task_id = TASK_ID_VAR(suf);									\
	p->tsc_start = rdtscp(&p->core_start);								\
	p->tsc_start -= g_tsc_skew[p->core_start];							\
}							
#define KAS_TASK(label, system)				_KAS_TASK(label, system, __COUNTER__)				
#define KAS_END												\
{													\
	/* index 0 reserved for dummy */								\
	kas_assert(tls_frame->stack_count > 1);								\
													\
	const u32 s = tls_frame->stack_count;								\
	const u32 pi = tls_frame->build_stack[s-1];							\
	struct lw_profile *p = tls_frame->build + pi;							\
	p->tsc_end = rdtscp(&p->core_end);								\
	p->tsc_end -= g_tsc_skew[p->core_end];								\
	kas_assert(p->tsc_end > p->tsc_start);								\
													\
	/* NOTE: Ordering here very important, see kas_new_frame */					\
	atomic_store_rel_32(&tls_frame->stack_count, s-1);						\
}						

void kas_profiler_try_set_task_id(volatile u32 *a_static_task_id, volatile u32 *a_static_setting, const char *label, const enum system_id system_id);
void kas_profiler_acquire_thread_local_frame(const u64 worker_id, const tid thread_id);

#endif
