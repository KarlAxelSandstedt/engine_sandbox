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

#include "linux_local.h"
#include "sys_public.h"

#include <linux/perf_event.h>
#include <linux/hw_breakpoint.h>
#include <sys/syscall.h>
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mount.h>
#include <sys/statvfs.h>
#include <linux/magic.h>
#include <mntent.h>
#include <string.h>

#define TRACE_BUFSIZE 1024

static void static_assert_kt_sched_switch_format(void)
{
	kas_static_assert(sizeof(struct kt_datapoint) == sizeof(struct perf_event_header) + sizeof(u64) + sizeof(u32)+ sizeof(u32), "Expected size of kt_event");
	kas_static_assert(sizeof(struct kt_event) == 72, "Expected size of kt_event");
	kas_static_assert(sizeof(struct kt_sched_switch) == 56, "Expected size of kt_sched_switch");
	kas_static_assert(sizeof(struct kt_common) == 8, "Expected size of kt_common");
	kas_static_assert((u64) &((struct kt_sched_switch *) 0)->prev_comm == 0, "");
	kas_static_assert((u64) &((struct kt_sched_switch *) 0)->prev_pid == 16, "");
	kas_static_assert((u64) &((struct kt_sched_switch *) 0)->prev_prio == 20, "");
	kas_static_assert((u64) &((struct kt_sched_switch *) 0)->prev_state == 24, "");
	kas_static_assert((u64) &((struct kt_sched_switch *) 0)->next_comm == 32, "");
	kas_static_assert((u64) &((struct kt_sched_switch *) 0)->next_pid == 48, "");
	kas_static_assert((u64) &((struct kt_sched_switch *) 0)->next_prio == 52, "");

	kas_static_assert((u64) &((struct kt_common *) 0)->type == 0, "");
	kas_static_assert((u64) &((struct kt_common *) 0)->flags == 2, "");
	kas_static_assert((u64) &((struct kt_common *) 0)->preempt_count == 3, "");
	kas_static_assert((u64) &((struct kt_common *) 0)->pid == 4, "");

	kas_static_assert((u64) &((struct kt_event *) 0)->common == 0, "");
	kas_static_assert((u64) &((struct kt_event *) 0)->ss == 8, "");

	kas_static_assert((u64) &((struct kt_datapoint *) 0)->header == 0, "");
	kas_static_assert((u64) &((struct kt_datapoint *) 0)->time == sizeof(struct perf_event_header), "");
	kas_static_assert((u64) &((struct kt_datapoint *) 0)->raw_size == 8 + sizeof(struct perf_event_header), "");
}

static void static_assert_kt_sched_wakeup_format(void)
{
	kas_static_assert(sizeof(struct kt_sched_wakeup) == 28, "Expected size of kt_sched_stat_wakeup");
	kas_static_assert((u64) &((struct kt_sched_wakeup *) 0)->comm == 0, "");
	kas_static_assert((u64) &((struct kt_sched_wakeup *) 0)->pid == 16, "");
	kas_static_assert((u64) &((struct kt_sched_wakeup *) 0)->prio == 20, "");
	kas_static_assert((u64) &((struct kt_sched_wakeup *) 0)->target_cpu == 24, "");
}

static void static_assert_kt_sched_waking_format(void)
{
	kas_static_assert(sizeof(struct kt_sched_waking) == 28, "Expected size of kt_sched_stat_waking");
	kas_static_assert((u64) &((struct kt_sched_waking *) 0)->comm == 0, "");
	kas_static_assert((u64) &((struct kt_sched_waking *) 0)->pid == 16, "");
	kas_static_assert((u64) &((struct kt_sched_waking *) 0)->prio == 20, "");
	kas_static_assert((u64) &((struct kt_sched_waking *) 0)->target_cpu == 24, "");
}

static void static_assert_kt_sched_wait_task_format(void)
{
	kas_static_assert(sizeof(struct kt_sched_wait_task) == 24, "Expected size of kt_sched_wait_task");
	kas_static_assert((u64) &((struct kt_sched_wait_task *) 0)->comm == 0, "");
	kas_static_assert((u64) &((struct kt_sched_wait_task *) 0)->pid == 16, "");
	kas_static_assert((u64) &((struct kt_sched_wait_task *) 0)->prio == 20, "");
}

static void static_assert_kt_sched_iowait_format(void)
{
	kas_static_assert(sizeof(struct kt_sched_iowait) == 32, "Expected size of kt_sched_stat_iowait");
	kas_static_assert((u64) &((struct kt_sched_iowait *) 0)->comm == 0, "");
	kas_static_assert((u64) &((struct kt_sched_iowait *) 0)->pid == 16, "");
	kas_static_assert((u64) &((struct kt_sched_iowait *) 0)->delay == 24, "");
}

static void static_assert_kt_sched_blocked_format(void)
{
	kas_static_assert(sizeof(struct kt_sched_block) == 32, "Expected size of kt_sched_stat_blocked");
	kas_static_assert((u64) &((struct kt_sched_block *) 0)->comm == 0, "");
	kas_static_assert((u64) &((struct kt_sched_block *) 0)->pid == 16, "");
	kas_static_assert((u64) &((struct kt_sched_block *) 0)->delay == 24, "");
}

static u64 kt_timer_kt_to_tsc(const struct kt_ring_buffer *buf, const u64 kt_time)
{
	const u64 time = kt_time - buf->metadata->time_zero;
	const u64 quot = time / buf->metadata->time_mult;
	const u64 rem  = time % buf->metadata->time_mult;
	return ((quot << buf->metadata->time_shift) + (rem << buf->metadata->time_shift) / buf->metadata->time_mult);
}

static u64 kt_timer_mono_raw_to_ns(const struct kt_ring_buffer *buf, const u64 kt_time)
{
	return time_ns_from_tsc(tsc_from_kt(kt_time));
}

static u64 kt_timer_tsc_to_ns(const struct kt_ring_buffer *buf, const u64 kt_time)
{
	return time_ns_from_tsc(kt_time);
}

void tracefs_write_file(const utf8 *path, const struct kas_buffer *buf)
{
	const i32 fd = open((char *) path->buf, O_WRONLY);
	if (fd == -1)
	{
		LOG_SYSTEM_ERROR(S_FATAL);
		fatal_cleanup_and_exit(0);
	}

	u8 *data = buf->data;
	size_t left = buf->size - buf->mem_left;
	ssize_t it_write;

	do
	{
		it_write = write(fd, data, left);
		if (it_write == -1)
		{
			LOG_SYSTEM_ERROR(S_ERROR);
			break;
		}

		data += it_write;
		left -= (size_t) it_write;
	} while (left > 0);

	if (close(fd) == -1)
	{
		LOG_SYSTEM_ERROR(S_WARNING);
	}
}

struct kas_buffer tracefs_read_file(struct arena *mem, const utf8 *path)
{
	const i32 fd = open((char *) path->buf, O_RDONLY);
	if (fd == -1)
	{
		LOG_SYSTEM_ERROR(S_FATAL);
		fatal_cleanup_and_exit(0);
	}

	struct kas_buffer buf = 
	{
		.data = arena_push_packed(mem, TRACE_BUFSIZE),
		.size = TRACE_BUFSIZE,
		.mem_left = TRACE_BUFSIZE,
	};

	/* we do not know file size, so we keep on reading until we read nothing. */
	u8 *dst = buf.data;
	i64 it_read;
	do 
	{
		it_read = read(fd, dst, buf.mem_left);
		if (it_read == -1)
		{
			LOG_SYSTEM_ERROR(S_FATAL);
			fatal_cleanup_and_exit(0);
		}

		dst += it_read;
		/* may not have enough memory, so we get more and loop */
		if ((i64) buf.mem_left == it_read)
		{
			if (!arena_push_packed(mem, buf.size))
			{
				LOG_SYSTEM_ERROR(S_FATAL);
				fatal_cleanup_and_exit(0);
			}
			buf.mem_left = buf.size;
			buf.size *= 2;
		}
		else
		{
			buf.mem_left -= it_read;
		}
	} while (it_read > 0);

	
	arena_pop_packed(mem, buf.mem_left);
	buf.size -= buf.mem_left;
	buf.mem_left = 0;

	if (close(fd) == -1)
	{
		LOG_SYSTEM_ERROR(S_WARNING);
	}

	return buf;
}

static utf8 tracefs_file_parse_string(struct arena *mem, const utf8 *path)
{
	struct kas_buffer buf = tracefs_read_file(mem, path);
	const u32 len = (u32) (buf.size - buf.mem_left);
	utf8 str =
	{
		.buf = arena_push(mem, len + 1),
		.size = len + 1,
		.len = len,
	};

	memcpy(str.buf, buf.data, len);
	str.buf[len] = '\0';

	return str;
}

static u64 tracefs_file_parse_u64(struct arena *tmp, const utf8 *path)
{
	arena_push_record(tmp);
	struct kas_buffer buf = tracefs_read_file(tmp, path);
	
	u64 id = 0;
	const u64 len = buf.size - buf.mem_left;
	for (u64 i = 0; i < len; ++i)
	{
		if (buf.data[i] == '\n')
		{
			break;
		}
		kas_assert_string('0' <= buf.data[i] && buf.data[i] <= '9', "expected event id to be string u64");
		id = 10*id + (u64) (buf.data[i] - '0');
	}

	arena_push_record(tmp);
	return id;
}

static u32 tracefs_is_mounted(const utf8 *path)
{
	u32 found = 0;
	FILE *stream = setmntent("/etc/mtab", "r");
	if (!stream)
	{
		log_string(T_SYSTEM, S_FATAL, "Failed to open /etc/mtab for reading, aborting");
		fatal_cleanup_and_exit(0);
	}

	const u64 maxsize = 256; /* covers trace_path, on overflow, we cmp to "" which is fine as well. */
	char buf[256];
	

	struct mntent *ent;
	while ( (ent = getmntent(stream)) != NULL )
	{
		utf8 ent_path = 
		{
			.buf = (u8 *) ent->mnt_dir,
		};
		ent_path.len = strlen(ent->mnt_dir);
		ent_path.size = ent_path.len + 1;

		if (utf8_equivalence(*path, ent_path))
		{
			found = 1;
			break;
		}
	}

	endmntent(stream);

	return found;
}

static void tracefs_mount(struct kernel_tracer *kt, const utf8 *trace_path)
{
	/* 
	 * default flags on my already mounted trace filesystem. 
	 * TODO: can we use other flags for better performance (i.e, do not update time always... etc...) ? 
	 */
	const unsigned long mount_flags = 
		  MS_NODEV 			/* Disallow access to device special files on this filesystem 	*/
		| MS_NOEXEC			/* Disallow execution of programs in filesystem 		*/
		| MS_NOSUID			/* The set-user-ID and set-group-ID bits are ignored by exec(3) for executable files on this filesystem */
		| MS_RELATIME;			/* Update atime relative to mtime/ctime */

	if (mount("nodev", (char *) trace_path->buf, "tracefs", mount_flags, NULL) == -1)
	{
		LOG_SYSTEM_ERROR(S_FATAL);
		log(T_SYSTEM, S_FATAL, 0, "Failed to mount ftrace filsystem at %k", &trace_path);
		fatal_cleanup_and_exit(0);
	}

	kas_assert(tracefs_is_mounted(trace_path));
}

void tracefs_get_status(struct kernel_tracer *kt, const utf8 *trace_path)
{
	/*
	 * (2) Get fs flags
	 */
	struct statvfs statvfsbuf;
	if (statvfs((char*) trace_path->buf, &statvfsbuf) == -1)
	{
		LOG_SYSTEM_ERROR(S_FATAL);
		fatal_cleanup_and_exit(0);
	}

	log(T_SYSTEM, S_NOTE, "tracefs flags:\n\tST_MANDLOCK (%u)\n\tST_NOATIME (%u)\n\tST_NODEV (%u)\n\tST_NODIRATIME (%u)\n\tST_NOEXEC (%u)\n\tST_NOSUID (%u)\n\tST_RDONLY (%u)\n\tST_RELATIME (%u)\n\tST_SYNCHRONOUS (%u)"
			, (statvfsbuf.f_flag & ST_MANDLOCK) ? 1 : 0
		      	, (statvfsbuf.f_flag & ST_NOATIME) ? 1 : 0
		       	, (statvfsbuf.f_flag & ST_NODEV) ? 1 : 0
		       	, (statvfsbuf.f_flag & ST_NODIRATIME) ? 1 : 0
		       	, (statvfsbuf.f_flag & ST_NOEXEC) ? 1 : 0
		       	, (statvfsbuf.f_flag & ST_NOSUID) ? 1 : 0
		       	, (statvfsbuf.f_flag & ST_RDONLY) ? 1 : 0
		       	, (statvfsbuf.f_flag & ST_RELATIME) ? 1 : 0
		       	, (statvfsbuf.f_flag & ST_SYNCHRONOUS) ? 1 : 0);
}

struct kernel_tracer *kernel_tracer_init(struct arena *mem)
{
	const utf8 tracefs_path = utf8_inline("/sys/kernel/tracing");
	const utf8 current_tracer_path = utf8_inline("/sys/kernel/tracing/current_tracer");
	const utf8 trace_options_path = utf8_inline("/sys/kernel/tracing/trace_options");
	const utf8 option_annotate_path = utf8_inline("/sys/kernel/tracing/options/annotate");
	const utf8 option_record_cmd_path = utf8_inline("/sys/kernel/tracing/options/record-cmd");
	const utf8 option_record_tgid_path = utf8_inline("/sys/kernel/tracing/options/record-tgid");
	const utf8 option_irq_info_path = utf8_inline("/sys/kernel/tracing/options/irq-info");
	const utf8 trace_clock_path = utf8_inline("/sys/kernel/tracing/trace_clock");
	const utf8 tracing_on_path = utf8_inline("/sys/kernel/tracing/tracing_on");
	const utf8 buffer_size_kb_path = utf8_inline("/sys/kernel/tracing/buffer_size_kb");

	const utf8 sched_switch_id_path = utf8_inline("/sys/kernel/tracing/events/sched/sched_switch/id");
	const utf8 sched_switch_enable_path = utf8_inline("/sys/kernel/tracing/events/sched/sched_switch/enable");
	const utf8 sched_wakeup_id_path = utf8_inline("/sys/kernel/tracing/events/sched/sched_wakeup/id");
	const utf8 sched_wakeup_enable_path = utf8_inline("/sys/kernel/tracing/events/sched/sched_wakeup/enable");
	const utf8 sched_waking_id_path = utf8_inline("/sys/kernel/tracing/events/sched/sched_waking/id");
	const utf8 sched_waking_enable_path = utf8_inline("/sys/kernel/tracing/events/sched/sched_waking/enable");
	const utf8 sched_wait_task_id_path = utf8_inline("/sys/kernel/tracing/events/sched/sched_wait_task/id");
	const utf8 sched_wait_task_enable_path = utf8_inline("/sys/kernel/tracing/events/sched/sched_wait_task/enable");
	const utf8 sched_stat_iowait_id_path = utf8_inline("/sys/kernel/tracing/events/sched/sched_stat_iowait/id");
	const utf8 sched_stat_iowait_enable_path = utf8_inline("/sys/kernel/tracing/events/sched/sched_stat_iowait/enable");
	const utf8 sched_stat_blocked_id_path = utf8_inline("/sys/kernel/tracing/events/sched/sched_stat_blocked/id");
	const utf8 sched_stat_blocked_enable_path = utf8_inline("/sys/kernel/tracing/events/sched/sched_stat_blocked/enable");

	struct kernel_tracer *kt = arena_push(mem, sizeof(struct kernel_tracer));

	if (!tracefs_is_mounted(&tracefs_path))	
	{
		log(T_SYSTEM, S_NOTE, "tracefs not mounted at %k, trying to mount filesystem", &tracefs_path);
		tracefs_mount(kt, &tracefs_path);

		kas_assert(tracefs_is_mounted(&tracefs_path));
	}
	log(T_SYSTEM, S_SUCCESS, "tracefs mounted at %k", &tracefs_path);

	tracefs_get_status(kt, &tracefs_path);

	/*
	 * options:
	 * 	record-cmd:	When any event or tracer is enabled, a hook is enabled
	 * 			in the sched_switch trace point to fill comm cache
	 * 			with mapped pids and comms. But this may cause some
	 * 			overhead, and if you only care about pids, and not the
	 * 			name of the task, disabling this option can lower the
	 * 			impact of tracing. See "saved_cmdlines".
	 *
	 * 	record-tgid:	When any event or tracer is enabled, a hook is enabled
	 *			in the sched_switch trace point to fill the cache of
	 *			mapped Thread Group IDs (TGID) mapping to pids. See
	 *			"saved_tgids".
	 *
	 * 	irq-info:	hows the interrupt, preempt count, need resched data.
	 *
	 * 	annotate:	annotate - It is sometimes confusing when the CPU buffers are full
  	 *			and one CPU buffer had a lot of events recently, thus
	 *			a shorter time frame, were another CPU may have only had
	 *			a few events, which lets it have older events. When
	 *			the trace is reported, it shows the oldest events first,
	 *			and it may look like only one CPU ran (the one with the
	 *			oldest events). When the annotate option is set, it will
	 *			display when a new CPU buffer started:
	 *
	 * x86-tsc: 		Make use of the tsc-counter (kernel uses tsc as basis to report timestamps in ns)
	 *
	 * buffer_size_kb:  	This sets or displays the number of kilobytes each CPU
	 *		    	buffer holds. By default, the trace buffers are the same size
	 *			for each CPU. The displayed number is the size of the
	 *			CPU buffer and not total size of all buffers
	 */
	const utf8 on_str = utf8_inline("1");
	const utf8 off_str = utf8_inline("0");
	const utf8 nop_str = utf8_inline("nop");
	const utf8 clock_str = utf8_inline("x86-tsc");
	const utf8 size_str = utf8_inline("4096");
	
	const struct kas_buffer on =  		{ .data = on_str.buf,  		.size = on_str.len, 	 .mem_left = 0 };
	const struct kas_buffer off = 		{ .data = off_str.buf, 		.size = off_str.len, 	 .mem_left = 0 };
	const struct kas_buffer nop = 		{ .data = nop_str.buf, 		.size = nop_str.len, 	 .mem_left = 0 };
	const struct kas_buffer clock = 	{ .data = clock_str.buf, 	.size = clock_str.len, 	 .mem_left = 0 };
	const struct kas_buffer size = 		{ .data = size_str.buf, 	.size = size_str.len, 	 .mem_left = 0 };

	tracefs_write_file(&tracing_on_path, &off);
	tracefs_write_file(&sched_switch_enable_path, &on);
	//tracefs_write_file(&sched_wakeup_enable_path, &on);
	tracefs_write_file(&sched_waking_enable_path, &on);
	//tracefs_write_file(&sched_wait_task_enable_path, &on);
	//tracefs_write_file(&sched_stat_iowait_enable_path, &on);
	//tracefs_write_file(&sched_stat_blocked_enable_path, &on);
	tracefs_write_file(&current_tracer_path, &nop);
	tracefs_write_file(&option_annotate_path, &off);
	tracefs_write_file(&option_record_cmd_path, &off);
	tracefs_write_file(&option_record_tgid_path, &off);
	tracefs_write_file(&option_irq_info_path, &off);
	tracefs_write_file(&trace_clock_path, &clock);
	tracefs_write_file(&buffer_size_kb_path, &size);
	tracefs_write_file(&tracing_on_path, &on);

	const utf8 tracer = tracefs_file_parse_string(mem, &current_tracer_path);
	const utf8 trace_options = tracefs_file_parse_string(mem, &trace_options_path);
	const utf8 trace_clock = tracefs_file_parse_string(mem, &trace_clock_path);
	const u64 buffer_size = tracefs_file_parse_u64(mem, &buffer_size_kb_path) * 1024;

	kt->sched_switch_id = tracefs_file_parse_u64(mem, &sched_switch_id_path);
        kt->sched_wakeup_id = tracefs_file_parse_u64(mem, &sched_wakeup_id_path);
        kt->sched_waking_id = tracefs_file_parse_u64(mem, &sched_waking_id_path);
        kt->sched_wait_task_id = tracefs_file_parse_u64(mem, &sched_wait_task_id_path);
        kt->sched_stat_iowait_id = tracefs_file_parse_u64(mem, &sched_stat_iowait_id_path);
        kt->sched_stat_blocked_id = tracefs_file_parse_u64(mem, &sched_stat_blocked_id_path);
	
	log(T_SYSTEM, S_NOTE, "tracing: (%lu)", tracefs_file_parse_u64(mem, &tracing_on_path));
	log(T_SYSTEM, S_NOTE, "sched_switch: (%lu)", kt->sched_switch_id);
	//log(T_SYSTEM, S_NOTE, "sched_wakeup: (%lu)", kt->sched_wakeup_id);
	log(T_SYSTEM, S_NOTE, "sched_waking: (%lu)", kt->sched_waking_id);
	//log(T_SYSTEM, S_NOTE, "sched_wait_task: (%lu)", kt->sched_wait_task_id);
	//log(T_SYSTEM, S_NOTE, "sched_stat_iowait: (%lu)", kt->sched_stat_iowait_id);
	//log(T_SYSTEM, S_NOTE, "sched_stat_blocked: (%lu)", kt->sched_stat_blocked_id);
	log(T_SYSTEM, S_NOTE, "current tracer: %k", &tracer);
	log(T_SYSTEM, S_NOTE, "trace clock: %k", &trace_clock);
	log(T_SYSTEM, S_NOTE, "ring buffer size: %luB", buffer_size);
	log(T_SYSTEM, S_NOTE, "trace options: \n%k", &trace_options);

	kt->page_size = getpagesize();
	kt->page_count = (buffer_size % kt->page_size)
	         ? buffer_size / kt->page_size + 1
	         : buffer_size / kt->page_size;
	kt->buffer_count = g_arch_config->logical_core_count;
	kt->buffers = arena_push(mem, kt->buffer_count * sizeof(struct kt_ring_buffer));
	kt->timer = (g_arch_config->rdtsc && g_arch_config->rdtscp && g_arch_config->tsc_invariant)
		? KT_TIMER_RDTSC
		: KT_TIMER_SYSTEM;
	/*
	 * We wish, for every cpu to caputre all context switch events  happening on itself. In the perf_event api,
	 * you can either specify to measure all processes happening on a cpu, or a single process on all cpus.
	 *
	 * Therefore we need go with the first choice, and inititate the capture on all cpus. From the manual:
	 *
	 * 	pid == -1 and cpu >= 0
         * 		This measures all processes/threads on the specified CPU.
         * 	   	This requires CAP_PERFMON (since Linux 5.8) or
         * 	   	CAP_SYS_ADMIN capability or a
         * 	   	/proc/sys/kernel/perf_event_paranoid value of less than 1.
	 */
	const pid_t pid = -1;
	const i32 group_fd = -1;	/* group leader, if we wish to add events to the group, use the file descriptor
					   of the group leader returned by the syscall.  */
	const u64 flags = 0;

	struct perf_event_attr attr = { 0 };
	attr.size = sizeof(struct perf_event_attr);

	if (kt->timer == KT_TIMER_SYSTEM)
	{
		attr.use_clockid = 1;			
		attr.clockid = CLOCK_MONOTONIC_RAW;
	}
	attr.sample_period = 1;					/* For every event we generate a datapoint. */
	attr.sample_type = PERF_SAMPLE_TIME | PERF_SAMPLE_RAW;	/* Records timestap + Raw data returned from tracepoint. */
	attr.inherit = 1;
	attr.type = PERF_TYPE_TRACEPOINT;
	log(T_SYSTEM, S_NOTE, "sched_switch_id: %lu", kt->sched_switch_id);
	//log(T_SYSTEM, S_NOTE, "sched_wakeup_id: %lu", kt->sched_wakeup_id);
	log(T_SYSTEM, S_NOTE, "sched_waking_id: %lu", kt->sched_waking_id);
	//log(T_SYSTEM, S_NOTE, "sched_wait_task_id: %lu", kt->sched_wait_task_id);
	log(T_SYSTEM, S_NOTE, "sched_switch on: %lu",  tracefs_file_parse_u64(mem, &sched_switch_enable_path));
	//log(T_SYSTEM, S_NOTE, "sched_wakeup on: %lu",  tracefs_file_parse_u64(mem, &sched_wakeup_enable_path));
	log(T_SYSTEM, S_NOTE, "sched_waking on: %lu",  tracefs_file_parse_u64(mem, &sched_waking_enable_path));
	//log(T_SYSTEM, S_NOTE, "sched_wait_task on: %lu", tracefs_file_parse_u64(mem, &sched_wait_task_enable_path));
	//log(T_SYSTEM, S_NOTE, "sched_stat_iowait_id: %lu", kt->sched_stat_iowait_id);
	//log(T_SYSTEM, S_NOTE, "sched_stat_blocked_id: %lu", kt->sched_stat_blocked_id);
	
	if (!is_power_of_two(kt->page_count - 1))
	{
		log_string(T_SYSTEM, S_FATAL, "kernel tracer buffer page count should be 2^n + 1. aborting");
		fatal_cleanup_and_exit(0);
	}

KT_BUFFERS_INIT:
	for (u32 cpu = 0; cpu < kt->buffer_count; ++cpu)
	{
		attr.disabled = 1;
		attr.config = kt->sched_switch_id;    	/* type is PERF_TYPE_TRACEPOINT, then we are measuring
        		      			   	   kernel tracepoints.  The value to use in config can be
        		      			   	   obtained from under debugfs tracing/events/[...]/id if
        		      			   	   ftrace is enabled in the kernel. */
		kt->buffers[cpu].fd_switch = syscall(SYS_perf_event_open, &attr, pid, (i32) cpu, group_fd, flags);
		if (kt->buffers[cpu].fd_switch == -1)
		{
			LOG_SYSTEM_ERROR(S_FATAL);
			fatal_cleanup_and_exit(0);
		}

		kt->buffers[cpu].metadata = mmap(NULL, kt->page_size*kt->page_count, PROT_READ | PROT_WRITE, MAP_SHARED, kt->buffers[cpu].fd_switch, 0);
		if (kt->buffers[cpu].metadata == MAP_FAILED)
		{
			LOG_SYSTEM_ERROR(S_FATAL);
			fatal_cleanup_and_exit(0);
		}

		//attr.disabled = 0;
		//attr.config = kt->sched_wakeup_id;
		//kt->buffers[cpu].fd_wakeup = syscall(SYS_perf_event_open, &attr, pid, (i32) cpu, kt->buffers[cpu].fd_switch, flags);
		//if (kt->buffers[cpu].fd_wakeup == -1 || ioctl(kt->buffers[cpu].fd_wakeup, PERF_EVENT_IOC_SET_OUTPUT, kt->buffers[cpu].fd_switch) == -1)
		//{
		//	LOG_SYSTEM_ERROR(S_FATAL);
		//	fatal_cleanup_and_exit(0);
		//}

		attr.disabled = 0;
		attr.config = kt->sched_waking_id;
		kt->buffers[cpu].fd_waking = syscall(SYS_perf_event_open, &attr, pid, (i32) cpu, kt->buffers[cpu].fd_switch, flags);
		if (kt->buffers[cpu].fd_waking == -1 || ioctl(kt->buffers[cpu].fd_waking, PERF_EVENT_IOC_SET_OUTPUT, kt->buffers[cpu].fd_switch) == -1)
		{
			LOG_SYSTEM_ERROR(S_FATAL);
			fatal_cleanup_and_exit(0);
		}

		//attr.config = kt->sched_wait_task_id;
		//kt->buffers[cpu].fd_wait_task = syscall(SYS_perf_event_open, &attr, pid, (i32) cpu, kt->buffers[cpu].fd_switch, flags);
		//if (kt->buffers[cpu].fd_wait_task == -1 || ioctl(kt->buffers[cpu].fd_wait_task, PERF_EVENT_IOC_SET_OUTPUT, kt->buffers[cpu].fd_switch) == -1)
		//{
		//	LOG_SYSTEM_ERROR(S_FATAL);
		//	fatal_cleanup_and_exit(0);
		//}

		//TODO if kt->timer != KT_TIMER_RDTSC We should only use lightweight profiler, since at that point, the kernel must not have managed to sync the tsc counters across the cores
		if (kt->timer == KT_TIMER_RDTSC && kt->buffers[cpu].metadata->cap_user_time_zero == 0)
		{
			log_string(T_SYSTEM, S_WARNING, "Failed to set kernel tracer to use tsc, trying CLOCK_MONONTIC_RAW.");
			for (u32 c = 0; c <= cpu; ++c)
			{
				if (munmap(kt->buffers[c].metadata, kt->page_size * kt->page_count) == -1)
				{
					LOG_SYSTEM_ERROR(S_WARNING);
				}

				close(kt->buffers[cpu].fd_wait_task);
				close(kt->buffers[cpu].fd_waking);
				//close(kt->buffers[cpu].fd_wakeup);
				close(kt->buffers[cpu].fd_switch);
			}

			attr.use_clockid = 1;			
			attr.clockid = CLOCK_MONOTONIC_RAW;
			kt->timer = KT_TIMER_SYSTEM;
			goto KT_BUFFERS_INIT;
		}

		if (kt->buffers[cpu].metadata->cap_user_time_zero)
		{
			const u64 time_mult = kt->buffers[cpu].metadata->time_mult;
			const u64 time_zero = kt->buffers[cpu].metadata->time_zero;
			const u64 time_shift = kt->buffers[cpu].metadata->time_shift;
			time_set_kt_transform_parameters(time_mult, time_zero, time_shift);
			kas_assert(kt->timer == KT_TIMER_RDTSC);
		}
		else
		{
			kas_assert_string(0, "TODO: when tsc <-> ns not available, we skip heavy weight profiling");
		}
		kt->buffers[cpu].base = ((u8 *) kt->buffers[cpu].metadata) + kt->buffers[cpu].metadata->data_offset;
	}

	//kt->kt_timer_to_ns = (kt->timer == KT_TIMER_RDTSC) ? kt_timer_tsc_to_ns : kt_timer_mono_raw_to_ns;
	/* TODO Kernel always provide a ns value, not tsc? */
	//kt->kt_timer_to_ns = kt_timer_mono_raw_to_ns;
	kt->tsc_from_kt_time = &kt_timer_kt_to_tsc;

	const utf8 tsc_str = utf8_inline("tsc");
	const utf8 mono_raw_str = utf8_inline("clock monotonic raw");
	const utf8 *timer_str = (kt->timer == KT_TIMER_RDTSC) ? &tsc_str : &mono_raw_str;
	log(T_SYSTEM, S_SUCCESS, "Kernel Tracer initiated, timer used is %k.", timer_str);

	kernel_tracer_enable_events(kt);

	return kt;
}

void kernel_tracer_debug_print(const struct kernel_tracer *kt)
{
	for (u32 cpu = 0; cpu < kt->buffer_count; ++cpu)
	{
		struct perf_event_mmap_page *meta = kt->buffers[cpu].metadata;
		fprintf(stderr, "(tail, head) : (%llu, %llu)\n", meta->data_tail, meta->data_head);
	}
}

void kernel_tracer_shutdown(struct kernel_tracer *kt)
{
	kernel_tracer_disable_events(kt);

	for (u32 cpu = 0; cpu < kt->buffer_count; ++cpu)
	{
		if (munmap(kt->buffers[cpu].metadata, kt->page_size * kt->page_count) == -1)
		{
			LOG_SYSTEM_ERROR(S_WARNING);
		}

		//close(kt->buffers[cpu].fd_wakeup);
		close(kt->buffers[cpu].fd_waking);
		close(kt->buffers[cpu].fd_wait_task);
		close(kt->buffers[cpu].fd_switch);
	}
}

void kernel_tracer_enable_events(struct kernel_tracer *kt)
{
	for (u32 i = 0; i < kt->buffer_count; ++i)
	{
		ioctl(kt->buffers[i].fd_switch, PERF_EVENT_IOC_ENABLE, 0);
	}
}

void kernel_tracer_disable_events(struct kernel_tracer *kt)
{
	for (u32 i = 0; i < kt->buffer_count; ++i)
	{
		ioctl(kt->buffers[i].fd_switch, PERF_EVENT_IOC_DISABLE, 0);
	}
}

static void kt_sched_switch_debug_print(const struct kt_event *ev)
{
	fprintf(stderr, "kernel event:						\
			\n{							\
			\n\t.common_type = %u	(sched_switch_id)		\
			\n\t.common_flags = %u					\
			\n\t.common_preempt_count = %u				\
			\n\t.common_pid = %i					\
			\n\t.prev_comm = %s					\
			\n\t.prev_pid = %i					\
			\n\t.prev_prio = %i					\
			\n\t.prev_state = %li					\
			\n\t.next_comm = %s					\
			\n\t.next_pid = %i					\
			\n\t.next_prio = %i					\
			\n}\n", 
			ev->common.type,
			ev->common.flags,
			ev->common.preempt_count,
			ev->common.pid,
			ev->ss.prev_comm,
			ev->ss.prev_pid,
			ev->ss.prev_prio,
			ev->ss.prev_state,
			ev->ss.next_comm,
			ev->ss.next_pid,
			ev->ss.next_prio
			);						
}

static void kt_sched_wakeup_debug_print(const struct kt_event *ev)
{
	fprintf(stderr, "kernel event:						\
			\n{							\
			\n\t.common_type = %u	(sched_wakeup)			\
			\n\t.common_flags = %u					\
			\n\t.common_preempt_count = %u				\
			\n\t.common_pid = %i					\
			\n\t.comm = %s						\
			\n\t.pid = %i						\
			\n\t.prio = %i						\
			\n\t.target_cpu = %i					\
			\n}\n", 
			ev->common.type,
			ev->common.flags,
			ev->common.preempt_count,
			ev->common.pid,
			ev->wakeup.comm,
			ev->wakeup.pid,
			ev->wakeup.prio,
			ev->wakeup.target_cpu
			);						
}

static void kt_sched_waking_debug_print(const struct kt_event *ev)
{
	fprintf(stderr, "kernel event:						\
			\n{							\
			\n\t.common_type = %u	(sched_waking)			\
			\n\t.common_flags = %u					\
			\n\t.common_preempt_count = %u				\
			\n\t.common_pid = %i					\
			\n\t.comm = %s						\
			\n\t.pid = %i						\
			\n\t.prio = %i						\
			\n\t.target_cpu = %i					\
			\n}\n", 
			ev->common.type,
			ev->common.flags,
			ev->common.preempt_count,
			ev->common.pid,
			ev->waking.comm,
			ev->waking.pid,
			ev->waking.prio,
			ev->waking.target_cpu
			);						
}

static void kt_sched_wait_task_debug_print(const struct kt_event *ev)
{
	fprintf(stderr, "kernel event:						\
			\n{							\
			\n\t.common_type = %u	(sched_wait_task)		\
			\n\t.common_flags = %u					\
			\n\t.common_preempt_count = %u				\
			\n\t.common_pid = %i					\
			\n\t.comm = %s						\
			\n\t.pid = %i						\
			\n\t.prio = %i						\
			\n}\n", 
			ev->common.type,
			ev->common.flags,
			ev->common.preempt_count,
			ev->common.pid,
			ev->waking.comm,
			ev->waking.pid,
			ev->waking.prio
			);						
}

static void kt_sched_iowait_debug_print(const struct kt_event *ev)
{
	fprintf(stderr, "kernel event:						\
			\n{							\
			\n\t.common_type = %u	(sched_iowait_id)		\
			\n\t.common_flags = %u					\
			\n\t.common_preempt_count = %u				\
			\n\t.common_pid = %i					\
			\n\t.comm = %s						\
			\n\t.pid = %i						\
			\n\t.delay[ns] = %lu					\
			\n}\n", 
			ev->common.type,
			ev->common.flags,
			ev->common.preempt_count,
			ev->common.pid,
			ev->iowait.comm,
			ev->iowait.pid,
			ev->iowait.delay
			);
}

static void kt_sched_block_debug_print(const struct kt_event *ev)
{
	fprintf(stderr, "kernel event:						\
			\n{							\
			\n\t.common_type = %u	(sched_blocked_id)		\
			\n\t.common_flags = %u					\
			\n\t.common_preempt_count = %u				\
			\n\t.common_pid = %i					\
			\n\t.comm = %s						\
			\n\t.pid = %i						\
			\n\t.delay[ns] = %lu					\
			\n}\n", 
			ev->common.type,
			ev->common.flags,
			ev->common.preempt_count,
			ev->common.pid,
			ev->block.comm,
			ev->block.pid,
			ev->block.delay
			);
}

void kt_datapoint_debug_print(const struct kt_datapoint *dp)
{
	fprintf(stderr, "dp(%u) at %p:						\
			\n{							\
			\n\t.header = { type = %u, misc = %u, size = %u } 	\
			\n\t.time = %lu						\
			\n\t.raw_size = %u					\
			\n}\n", 
			dp->header.size, dp,
			dp->header.type, dp->header.misc, dp->header.size,
			dp->time,
			dp->raw_size
			);

}

void kt_event_debug_print(const struct kernel_tracer *kt, const struct kt_event *ev)
{
	if 	(ev->common.type == kt->sched_switch_id) { kt_sched_switch_debug_print(ev); }
	else if (ev->common.type == kt->sched_wakeup_id) { kt_sched_wakeup_debug_print(ev); }
	else if (ev->common.type == kt->sched_waking_id) { kt_sched_waking_debug_print(ev); }
	else if (ev->common.type == kt->sched_wait_task_id) { kt_sched_wait_task_debug_print(ev); }
	else if (ev->common.type == kt->sched_stat_iowait_id) { kt_sched_iowait_debug_print(ev); }
	else if (ev->common.type == kt->sched_stat_blocked_id) { kt_sched_block_debug_print(ev); }
	else { kas_assert_string(0, "unsupported kernel event in debug print") };
}

void kernel_tracer_read_bytes(void *dst, struct kt_ring_buffer *buf, const u64 bytes)
{
	const u64 buf_pos = (buf->offset % buf->metadata->data_size);
	if (buf_pos + bytes <= buf->metadata->data_size)
	{
		memcpy(dst, buf->base + buf_pos, bytes);
	}
	else
	{
		const u64 initial = buf->metadata->data_size - buf_pos;
		memcpy(dst, buf->base + buf_pos, initial);
		memcpy(((u8 *) dst) + initial, buf->base, bytes - initial);
	}
	buf->offset += bytes;

	kas_assert(buf->offset <= buf->frame_end);
}

void kernel_tracer_try_read_bytes(void *dst, struct kt_ring_buffer *buf, const u64 bytes)
{
	if (buf->frame_end < buf->offset + bytes)
	{
		memset(dst, 0xff, bytes);
		return;
	}

	const u64 buf_pos = (buf->offset % buf->metadata->data_size);
	if (buf_pos + bytes <= buf->metadata->data_size)
	{
		memcpy(dst, buf->base + buf_pos, bytes);
	}
	else
	{
		const u64 initial = buf->metadata->data_size - buf_pos;
		memcpy(dst, buf->base + buf_pos, initial);
		memcpy(((u8 *) dst) + initial, buf->base, bytes - initial);
	}
	buf->offset += bytes;
}
