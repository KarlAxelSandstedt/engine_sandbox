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

#ifndef __LINUX_PUBLIC_H__
#define __LINUX_PUBLIC_H__

#include <stdlib.h>
#include <sys/stat.h>
#include <sys/mman.h>

#include "kas_common.h"
#include "allocator.h"
#include "kas_string.h"

/******************** linux memory utils  ********************/

#define INLINE		inline
#define ALIGN(m)	__attribute__((aligned (m)))	

#if   _POSIX_C_SOURCE >= 200112L || _XOPEN_SOURCE >= 600 
/* returns aligned memory */
#define memory_alloc_aligned(ptr_addr, size, alignment)	posix_memalign((void **)(ptr_addr),(alignment),(size))
#endif

/******************** linux_error.c ********************/

#include <stdarg.h>
#include <signal.h>
#include <errno.h>
#include "log.h"

#ifdef __x86_64__
#define breakpoint(condition) if (!(condition)) { } else { asm ("int3; nop"); }
#endif 

#define kas_static_assert(assertion, str)	_Static_assert(assertion, str)

#ifdef KAS_ASSERT_DEBUG
#define kas_assert(assertion)			_kas_assert(assertion, __FILE__, __LINE__, __func__)
#define kas_assert_string(assertion, cstr)	_kas_assert_string(assertion, __FILE__, __LINE__, __func__, cstr) 
#define kas_assert_message(assertion, msg, ...)	_kas_assert_message(assertion, __FILE__, __LINE__, __func__, msg, __VA_ARGS__)

#define _kas_assert(assertion, file, line, func)							\
	if (assertion) { }										\
	else												\
	{												\
		log(T_ASSERT, S_FATAL, "assertion failed at %s:%u in function %s", file, line, func);	\
		raise(SIGTRAP);										\
	}

#define _kas_assert_string(assertion, file, line, func, cstr)							 \
	if (assertion) { }			     								 \
	else													 \
	{													 \
		log(T_ASSERT, S_FATAL, "assertion failed at %s:%u in function %s - %s", file, line, func, cstr); \
		raise(SIGTRAP);											 \
	}

#define _kas_assert_message(assertion, file, line, func, msg, ...)						    \
	if (assertion) { }											    \
	else													    \
	{													    \
		u8 __msg_buf[LOG_MAX_MESSAGE_SIZE];								    \
		const utf8 __fmsg = utf8_format_buffered(__msg_buf, LOG_MAX_MESSAGE_SIZE, msg, __VA_ARGS__);	    \
		log(T_ASSERT, S_FATAL, "assertion failed at %s:%u in function %s - %k", file, line, func, &__fmsg); \
		raise(SIGTRAP);											    \
	}

#else
#define kas_assert(assertion)
#define kas_assert_string(assertion, str)
#define kas_assert_message(assertion, msg, ...)
#define _kas_assert(assertion, file, line, func)
#define _kas_assert_string(assertion, file, line, func, str)
#define _kas_assert_message(assertion, file, line, func, str, ...)
#endif

#define ERROR_BUFSIZE	512
#define LOG_SYSTEM_ERROR(severity)		_LOG_SYSTEM_ERROR_CODE(severity, errno, __func__, __FILE__, __LINE__)
#define LOG_SYSTEM_ERROR_CODE(severity, code)	_LOG_SYSTEM_ERROR_CODE(severity, code, __func__, __FILE__, __LINE__)
#define _LOG_SYSTEM_ERROR_CODE(severity, code, func, file, line)						\
{														\
	u8 _err_buf[ERROR_BUFSIZE];										\
	const utf8 _err_str = utf8_system_error_code_string_buffered(_err_buf, ERROR_BUFSIZE, code);		\
	log(T_SYSTEM, severity, "At %s:%u in function %s - %k", file, line, func, &_err_str);			\
}

/* thread safe system error message generation */
utf8	utf8_system_error_code_string_buffered(u8 *buf, const u32 bufsize, const u32 code);

void 	init_error_handling_func_ptrs(void);

/******************** linux_filesystem.c ********************/

typedef struct stat	file_status;
typedef int 		file_handle;

#define FILE_HANDLE_INVALID 	-1

#include <sys/mman.h>
#define FS_PROT_READ         PROT_READ
#define FS_PROT_WRITE        PROT_WRITE
#define FS_PROT_EXECUTE      PROT_EXEC
#define FS_PROT_NONE         PROT_NONE

#define FS_MAP_SHARED        MAP_SHARED
#define FS_MAP_PRIVATE       MAP_PRIVATE

void	filesystem_init_func_ptrs(void);


/********************  linux_thread.c	********************/

typedef pid_t			tid;
typedef struct kas_thread 	kas_thread;

/********************  linux_sync_primitives.c	********************/

#include <semaphore.h>
typedef sem_t semaphore;

#ifdef FORCE_SEQ_CST
#define ATOMIC_RELAXED	__ATOMIC_SEQ_CST
#define ATOMIC_ACQUIRE	__ATOMIC_SEQ_CST
#define ATOMIC_RELEASE	__ATOMIC_SEQ_CST
#define ATOMIC_SEQ_CST	__ATOMIC_SEQ_CST
#else
#define ATOMIC_RELAXED	__ATOMIC_RELAXED
#define ATOMIC_ACQUIRE	__ATOMIC_ACQUIRE
#define ATOMIC_RELEASE	__ATOMIC_RELEASE
#define ATOMIC_SEQ_CST	__ATOMIC_SEQ_CST
#endif

#define atomic_fetch_add_rlx_32(fetch_addr, val)	__atomic_fetch_add(fetch_addr, val, ATOMIC_RELAXED)
#define atomic_fetch_add_acq_32(fetch_addr, val)	__atomic_fetch_add(fetch_addr, val, ATOMIC_ACQUIRE)
#define atomic_fetch_add_rel_32(fetch_addr, val)	__atomic_fetch_add(fetch_addr, val, ATOMIC_RELAXED)
#define atomic_fetch_add_seq_cst_32(fetch_addr, val)	__atomic_fetch_add(fetch_addr, val, ATOMIC_SEQ_CST)

#define atomic_fetch_add_rlx_64(fetch_addr, val)	__atomic_fetch_add(fetch_addr, val, ATOMIC_RELAXED)
#define atomic_fetch_add_acq_64(fetch_addr, val)	__atomic_fetch_add(fetch_addr, val, ATOMIC_ACQUIRE)
#define atomic_fetch_add_rel_64(fetch_addr, val)	__atomic_fetch_add(fetch_addr, val, ATOMIC_RELAXED)
#define atomic_fetch_add_seq_cst_64(fetch_addr, val)	__atomic_fetch_add(fetch_addr, val, ATOMIC_SEQ_CST)

#define atomic_fetch_sub_rlx_32(fetch_addr, val)	__atomic_fetch_sub(fetch_addr, val, ATOMIC_RELAXED)
#define atomic_fetch_sub_acq_32(fetch_addr, val)	__atomic_fetch_sub(fetch_addr, val, ATOMIC_ACQUIRE)
#define atomic_fetch_sub_rel_32(fetch_addr, val)	__atomic_fetch_sub(fetch_addr, val, ATOMIC_RELAXED)
#define atomic_fetch_sub_seq_cst_32(fetch_addr, val)	__atomic_fetch_sub(fetch_addr, val, ATOMIC_SEQ_CST)
                                                                       
#define atomic_fetch_sub_rlx_64(fetch_addr, val)	__atomic_fetch_sub(fetch_addr, val, ATOMIC_RELAXED)
#define atomic_fetch_sub_acq_64(fetch_addr, val)	__atomic_fetch_sub(fetch_addr, val, ATOMIC_ACQUIRE)
#define atomic_fetch_sub_rel_64(fetch_addr, val)	__atomic_fetch_sub(fetch_addr, val, ATOMIC_RELAXED)
#define atomic_fetch_sub_seq_cst_64(fetch_addr, val)	__atomic_fetch_sub(fetch_addr, val, ATOMIC_SEQ_CST)

#define atomic_compare_exchange_rlx_32(dst_addr, cmp_addr, exch_val)		__atomic_compare_exchange_n(dst_addr, cmp_addr, exch_val, 0, ATOMIC_RELAXED, ATOMIC_RELAXED)
#define atomic_compare_exchange_acq_32(dst_addr, cmp_addr, exch_val)		__atomic_compare_exchange_n(dst_addr, cmp_addr, exch_val, 0, ATOMIC_ACQUIRE, ATOMIC_ACQUIRE)
#define atomic_compare_exchange_rel_32(dst_addr, cmp_addr, exch_val)		__atomic_compare_exchange_n(dst_addr, cmp_addr, exch_val, 0, ATOMIC_RELEASE, ATOMIC_ACQUIRE)
#define atomic_compare_exchange_seq_cst_32(dst_addr, cmp_addr, exch_val)	__atomic_compare_exchange_n(dst_addr, cmp_addr, exch_val, 0, ATOMIC_SEQ_CST, ATOMIC_SEQ_CST)

#define atomic_compare_exchange_rlx_64(dst_addr, cmp_addr, exch_val)		__atomic_compare_exchange_n(dst_addr, cmp_addr, exch_val, 0, ATOMIC_RELAXED, ATOMIC_RELAXED)
#define atomic_compare_exchange_acq_64(dst_addr, cmp_addr, exch_val)		__atomic_compare_exchange_n(dst_addr, cmp_addr, exch_val, 0, ATOMIC_ACQUIRE, ATOMIC_ACQUIRE)
#define atomic_compare_exchange_rel_64(dst_addr, cmp_addr, exch_val)		__atomic_compare_exchange_n(dst_addr, cmp_addr, exch_val, 0, ATOMIC_RELEASE, ATOMIC_ACQUIRE)
#define atomic_compare_exchange_seq_cst_64(dst_addr, cmp_addr, exch_val)	__atomic_compare_exchange_n(dst_addr, cmp_addr, exch_val, 0, ATOMIC_SEQ_CST, ATOMIC_SEQ_CST)

#define atomic_store_rlx_32(dst_addr, val)		__atomic_store_n(dst_addr, val, ATOMIC_RELAXED)
#define atomic_store_rel_32(dst_addr, val)		__atomic_store_n(dst_addr, val, ATOMIC_RELEASE)
#define atomic_store_seq_cst_32(dst_addr, val)		__atomic_store_n(dst_addr, val, ATOMIC_SEQ_CST)
               
#define atomic_store_rlx_64(dst_addr, val)		__atomic_store_n(dst_addr, val, ATOMIC_RELAXED)
#define atomic_store_rel_64(dst_addr, val)		__atomic_store_n(dst_addr, val, ATOMIC_RELEASE)
#define atomic_store_seq_cst_64(dst_addr, val)		__atomic_store_n(dst_addr, val, ATOMIC_SEQ_CST)

#define atomic_add_fetch_rlx_32(fetch_addr, val)	__atomic_add_fetch(fetch_addr, val, ATOMIC_RELAXED) 
#define atomic_add_fetch_acq_32(fetch_addr, val)	__atomic_add_fetch(fetch_addr, val, ATOMIC_ACQUIRE) 
#define atomic_add_fetch_rel_32(fetch_addr, val)	__atomic_add_fetch(fetch_addr, val, ATOMIC_RELEASE) 
#define atomic_add_fetch_seq_cst_32(fetch_addr, val)	__atomic_add_fetch(fetch_addr, val, ATOMIC_SEQ_CST)
      
#define atomic_add_fetch_rlx_64(fetch_addr, val)	__atomic_add_fetch(fetch_addr, val, ATOMIC_RELAXED) 
#define atomic_add_fetch_acq_64(fetch_addr, val)	__atomic_add_fetch(fetch_addr, val, ATOMIC_ACQUIRE) 
#define atomic_add_fetch_rel_64(fetch_addr, val)	__atomic_add_fetch(fetch_addr, val, ATOMIC_RELEASE) 
#define atomic_add_fetch_seq_cst_64(fetch_addr, val)	__atomic_add_fetch(fetch_addr, val, ATOMIC_SEQ_CST)
     
#define atomic_sub_fetch_rlx_32(fetch_addr, val)	__atomic_sub_fetch(fetch_addr, val, ATOMIC_RELAXED) 
#define atomic_sub_fetch_acq_32(fetch_addr, val)	__atomic_sub_fetch(fetch_addr, val, ATOMIC_ACQUIRE) 
#define atomic_sub_fetch_rel_32(fetch_addr, val)	__atomic_sub_fetch(fetch_addr, val, ATOMIC_RELEASE) 
#define atomic_sub_fetch_seq_cst_32(fetch_addr, val)	__atomic_sub_fetch(fetch_addr, val, ATOMIC_SEQ_CST)
    
#define atomic_sub_fetch_rlx_64(fetch_addr, val)	__atomic_sub_fetch(fetch_addr, val, ATOMIC_RELAXED) 
#define atomic_sub_fetch_acq_64(fetch_addr, val)	__atomic_sub_fetch(fetch_addr, val, ATOMIC_ACQUIRE) 
#define atomic_sub_fetch_rel_64(fetch_addr, val)	__atomic_sub_fetch(fetch_addr, val, ATOMIC_RELEASE) 
#define atomic_sub_fetch_seq_cst_64(fetch_addr, val)	__atomic_sub_fetch(fetch_addr, val, ATOMIC_SEQ_CST)

#define atomic_load_rlx_32(fetch_addr)			__atomic_load_n(fetch_addr, ATOMIC_RELAXED)
#define atomic_load_acq_32(fetch_addr)			__atomic_load_n(fetch_addr, ATOMIC_ACQUIRE)
#define atomic_load_seq_cst_32(fetch_addr)		__atomic_load_n(fetch_addr, ATOMIC_SEQ_CST)

#define atomic_load_rlx_64(fetch_addr)			__atomic_load_n(fetch_addr, ATOMIC_RELAXED)
#define atomic_load_acq_64(fetch_addr)			__atomic_load_n(fetch_addr, ATOMIC_ACQUIRE)
#define atomic_load_seq_cst_64(fetch_addr)		__atomic_load_n(fetch_addr, ATOMIC_SEQ_CST)

#define atomic_load_to_addr_rlx_32(fetch_addr, dst_addr)	__atomic_load(fetch_addr, dst_addr, ATOMIC_RELAXED)
#define atomic_load_to_addr_acq_32(fetch_addr, dst_addr)	__atomic_load(fetch_addr, dst_addr, ATOMIC_ACQUIRE)
#define atomic_load_to_addr_seq_cst_32(fetch_addr, dst_addr)	__atomic_load(fetch_addr, dst_addr, ATOMIC_SEQ_CST)

#define atomic_load_to_addr_rlx_64(fetch_addr, dst_addr)	__atomic_load(fetch_addr, dst_addr, ATOMIC_RELAXED)
#define atomic_load_to_addr_acq_64(fetch_addr, dst_addr)	__atomic_load(fetch_addr, dst_addr, ATOMIC_ACQUIRE)
#define atomic_load_to_addr_seq_cst_64(fetch_addr, dst_addr)	__atomic_load(fetch_addr, dst_addr, ATOMIC_SEQ_CST)

#define atomic_store_from_addr_rlx_32(dst_addr, src_addr)	__atomic_store(dst_addr, src_addr, ATOMIC_RELAXED)
#define atomic_store_from_addr_rel_32(dst_addr, src_addr)	__atomic_store(dst_addr, src_addr, ATOMIC_ACQUIRE)
#define atomic_store_from_addr_seq_cst_32(dst_addr, src_addr)	__atomic_store(dst_addr, src_addr, ATOMIC_SEQ_CST)
                                                                         
#define atomic_store_from_addr_rlx_64(dst_addr, src_addr)	__atomic_store(dst_addr, src_addr, ATOMIC_RELAXED)
#define atomic_store_from_addr_rel_64(dst_addr, src_addr)	__atomic_store(dst_addr, src_addr, ATOMIC_ACQUIRE)
#define atomic_store_from_addr_seq_cst_64(dst_addr, src_addr)	__atomic_store(dst_addr, src_addr, ATOMIC_SEQ_CST)

/********************  Overflow Checking ********************/

#define	u64_add_return_overflow(dst_addr, src1, src2)		__builtin_add_overflow(src1, src2, dst_addr)
#define	u64_mul_return_overflow(dst_addr, src1, src2)		__builtin_mul_overflow(src1, src2, dst_addr)

/********************  Bit Manipulation ********************/

/*** Some builtins (REQUIRES: (BMI, Bit Manipulation Instruction Set 1 >= year 2013)) ***/
#define clz32(x)	((u32) __builtin_clz(x))  	/* count leading zeroes, NOTE: if x == 0, undefined! */
#define clz64(x)	((u32) __builtin_clzl(x))	/* count leading zeroes long, NOTE: if x == 0, undefined! */
#define ctz32(x)	((u32) __builtin_ctz(x))  	/* count trailing zeroes, NOTE: if x == 0, undefined! */
#define ctz64(x)	((u32) __builtin_ctzl(x))	/* count trailing zeroes long, NOTE: if x == 0, undefined! */

/******************** kernal_tracer.c (ftrace + perf_event interface) ********************/

#include <unistd.h>

#include <linux/perf_event.h>

/*
 * from uapi/linux/perf_event:
 *	header.misc = 1 (PERF_EVENT_KERNEL...)
 *	header.type = PERF_RECORD_SAMPLE
 *
 *	sample_type == PERF_SAMPLE_TIME | PERF_SAMPLE_RAW
 */
struct kt_common
{
	u16	type;
	u8	flags;
	u8	preempt_count;
	i32	pid;
};

struct kt_sched_switch
{
	char	prev_comm[16];
	pid_t	prev_pid;
	i32	prev_prio;
	i64	prev_state;
	char	next_comm[16];
	pid_t	next_pid;
	i32	next_prio;
};

struct kt_sched_wakeup
{
	char	comm[16];
	pid_t	pid;
	int	prio;
	int	target_cpu;
};

struct kt_sched_waking
{
	char	comm[16];
	pid_t	pid;
	int	prio;
	int	target_cpu;
};

struct kt_sched_wait_task
{
	char	comm[16];
	pid_t	pid;
	int	prio;
};

struct kt_sched_iowait
{
	char	comm[16];
	pid_t	pid;
	u64	delay;		/* ns */
};

struct kt_sched_block
{
	char	comm[16];
	pid_t	pid;
	u64	delay;		/* ns */
};

struct kt_event
{
	struct kt_common common;
	union  
	{
		struct kt_sched_switch 	ss;
		struct kt_sched_iowait 	iowait;
		struct kt_sched_block  	block;
		struct kt_sched_wakeup	wakeup;
		struct kt_sched_waking	waking;
		struct kt_sched_wait_task	wait_task;
	};
	
	u8	pad[4];
};


struct kt_datapoint
{
	struct perf_event_header	header;
	u64				time;
	u32				raw_size;			
	u8				raw_data[];
};
#define KT_DATAPOINT_PACKED_SIZE	(sizeof(struct perf_event_header) + sizeof(u64) + sizeof(u32))

struct kt_ring_buffer
{
	int 				fd_switch;	/* perf_event_open file handle, group leader */	
	int				fd_wait_task;	/* redirected to use fd */
	int				fd_waking;	/* redirected to use fd */
	int				fd_wakeup;	/* redirected to use fd */
	struct perf_event_mmap_page *	metadata;

	const u8 *			base;		/* addr to start of ring buffer */
	u64 				offset;		/* current offset (within frame interval) */
	u64 				frame_start;	/* current frame's starting offset */
	u64 				frame_end;	/* current frame's ending offset */
};

/* read bytes into dst[bytes] */
void	kernel_tracer_read_bytes(void *dst, struct kt_ring_buffer *buf, const u64 bytes);
/* read bytes into dst[bytes] if we stay within buffer boundaries, otherwise memset to 0xff */
void 	kernel_tracer_try_read_bytes(void *dst, struct kt_ring_buffer *buf, const u64 bytes);

enum kt_timer
{
	KT_TIMER_RDTSC,
	KT_TIMER_SYSTEM,
};

struct kernel_tracer
{
	utf8 			ftrace_path;
	struct kt_ring_buffer *	buffers;
	u64 			page_size;
	u64 			page_count;
	u32 			buffer_count;
	enum kt_timer		timer;		/* specifies what timer */
	u64			(*tsc_from_kt_time)(const struct kt_ring_buffer *, const u64); /* converts kernel ring buffer timestamp to corresponding cpu tsc value */

	/* supported kernel tracepoint types. [ See include/trace/events/sched.h for definitions and comments ] */
	u64 			sched_switch_id;
	u64 			sched_wakeup_id;	/* Not guaranteed to be called during waking process, but when p->state == TASK_RUNNING */
	u64 			sched_waking_id;	/* PREFERRED: Guaranteed to be called during waking process */
	u64 			sched_wait_task_id;
	u64 			sched_stat_iowait_id;
	u64 			sched_stat_blocked_id;
};

/* initialize the kernel tracer (ftrace + perf_event)
 *
 * requires: root privileges 
 *
 * NOTE: Must be called before thread creation. What happens here is that we set perf_events (or something)
 * 	 which will be inherited to any (child processes?) created.
 */
struct kernel_tracer *	kernel_tracer_init(struct arena *mem);
/* Restore kernel tracer setting from before we called kernel_tracer_init. */
void 			kernel_tracer_shutdown(struct kernel_tracer *kt);
void 			kernel_tracer_enable_events(struct kernel_tracer *kt);
void 			kernel_tracer_disable_events(struct kernel_tracer *kt);
void 			kernel_tracer_debug_print(const struct kernel_tracer *kt);

void 			kt_datapoint_debug_print(const struct kt_datapoint *dp);
void 			kt_event_debug_print(const struct kernel_tracer *kt, const struct kt_event *ev);
void 			os_arch_init_func_ptrs(void);

#endif
