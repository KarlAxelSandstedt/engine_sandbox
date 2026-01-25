/*
==========================================================================
    Copyright (C) 2025, 2026 Axel Sandstedt 

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

#include "ds_base.h"

#if __DS_PLATFORM__ == __DS_WEB__
#include <emscripten/console.h>
#endif

#if defined(DS_LOG)

/*
 * message_write:
 * 	if (!semaphore_trywait(msg_mem_left))	<--- if no mem, spin try to write to disk, until memory acquired
 * 	{
 *		message_write_batch_to_disk()
 * 	}
 *	semaphore_wait(msg_mem_left)
 * 	--------------------------------------- <-- we know there exist msg slot for thread, and that slot
 * 					            has had its memory pushed to disk. TODO: inspect assembly
 * 					            to ensure correct memory barriers, should be good enough
 * 					            with semaphore
 * 	index = atomic_relaxed_fetch_add(msg_count) % msg_max_count;
 * 	timestamp = timer_miliseconds()
 * 	format_msg_to_memory()
 *	atomic_release(msg_finished, 1);	<--- msg_finished == 1 <=> all memory has been written 
 *
 * message_write_batch_to_disk:
 * 	if (atomic_cmp_exchange_relaxed(disk_io_in_progress, 0, 1))
 * 	{
 * 		consume messages until we reach one that is not ready
 * 		while (atomic_cmp_exchange_acquire(next_msg->msg_finished, 1, 0) == 1)
 * 		{
 * 			write shit to disk
 * 		}
 * 		atomic_store_relaxed(disk_io_in_progress, 0);
 * 		semaphore_release(consume_count)
 * 	}
 */

#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include "ticket_factory.h"

static utf8 systems[T_COUNT];
static utf8 severities[S_COUNT];

struct log_message
{
	u64 	time;			/* ms */
	u32 	system;
	u32 	severity;
	u32 	thread_id;
	u32 	a_completed;		/* atomic to signal message has been completed */
	u32 	len;			/* length of message (without null termination) */
	u64	size_req;
	u8 	buf[LOG_MAX_MESSAGE_SIZE];	
	u32 	a_in_use_and_completed;	/* ready to be sent of to disk and reused */
};

struct log 
{
	struct log_message *	msg;
	struct ticket_factory *	tf;    			/* index factory */
	u32 			a_writing_to_disk;
	u32 			a_shutting_down;	/* when set, any further calls to message_write will immediately return */
	u32 			has_file;		/* If not, simply skip file IO */ 
	struct file 		file;
};

static struct log g_log;

void log_init(struct arena *mem, const char *filepath)
{
	systems[T_SYSTEM] = utf8_inline("System");
	systems[T_RENDERER] = utf8_inline("Renderer");
	systems[T_PHYSICS] = utf8_inline("Physics");
	systems[T_ASSET] = utf8_inline("Asset");
	systems[T_UTILITY] = utf8_inline("Utility");
	systems[T_PROFILER] = utf8_inline("Profiler");
	systems[T_ASSERT] = utf8_inline("Assert");
	systems[T_GAME] = utf8_inline("Game");
	systems[T_UI] = utf8_inline("Ui");
	systems[T_LED] = utf8_inline("Led");

	severities[S_SUCCESS] = utf8_inline("success");
	severities[S_NOTE] = utf8_inline("note");
	severities[S_WARNING] = utf8_inline("warning");
	severities[S_ERROR] = utf8_inline("error");
	severities[S_FATAL] = utf8_inline("fatal");

	g_log.msg = ArenaPush(mem, LOG_MAX_MESSAGES * sizeof(struct log_message));
	g_log.tf = ticket_factory_init(mem, LOG_MAX_MESSAGES);
	g_log.file = file_null();
	file_try_create_at_cwd(mem, &g_log.file, filepath, FILE_TRUNCATE);
	g_log.has_file = (g_log.file.handle != FILE_HANDLE_INVALID)
			? 1
			: 0;
	AtomicStoreRel32(&g_log.a_writing_to_disk, 0);
}

static void log_try_write_to_disk(void)
{
	u32 desired = 0;
	/* If we fail, try to write messages to disk and re-publish them  */
	if (AtomicCompareExchangeAcq32(&g_log.a_writing_to_disk, &desired, 1))
	{
		u32 count = 0;
		/* we are the single owner of a_serve: sync with any previous disk-writer */
		u32 serving = AtomicLoadAcq32(&g_log.tf->a_serve) % LOG_MAX_MESSAGES;
		u32 completed = 1;
		while (AtomicCompareExchangeAcq32(&g_log.msg[serving].a_in_use_and_completed, &completed, 0))
		{
			struct log_message *msg = g_log.msg + serving;

			if (g_log.has_file && msg->len)
			{
				file_write_append(&g_log.file, msg->buf, msg->size_req);
			}
			serving = (serving + 1) % LOG_MAX_MESSAGES;
			count += 1;
		}

		ticket_factory_return_tickets(g_log.tf, count);
		AtomicStoreRel32(&g_log.a_writing_to_disk, 0);
	}
}

static void internal_write_to_disk(void)
{
	/* spin until all tickets have finished and the messages have been written to disk */
	while (AtomicLoadAcq32(&g_log.tf->a_serve) != AtomicLoadAcq32(&g_log.tf->a_next))
	{
		log_try_write_to_disk();
	}
}

void log_shutdown()
{
	log_string(T_SYSTEM, S_NOTE, "Log system initiated shutdown");

	AtomicStoreRel32(&g_log.tf->a_open, 0);
	if (g_log.has_file)
	{
		internal_write_to_disk();
		file_sync(&g_log.file);
		file_close(&g_log.file);
	}
	ticket_factory_destroy(g_log.tf);
}

void log_write_message(const enum system_id system, const enum severity_id severity, const char *format, ... )
{
	//TODO should perf log this 
	
	const u32 thread_id = ds_thread_self_tid();
	/* spin until a new msg slot is up for grabs for us to publish */
	u32 ticket;
	u32 ret;
	while (!(ret = ticket_factory_try_get_ticket(&ticket, g_log.tf)))
	{
		/* If we fail to get a ticket after the shutdown process has started, 
		 * we should probably just abort to keep it simple for now. We should
		 * instead ensure that all threads have terminated before we even call
		 * log_shutdown();
		 */
		log_try_write_to_disk();
	}

	if (ret == TICKET_FACTORY_CLOSED) { return; }


	struct log_message *msg = g_log.msg + (ticket % LOG_MAX_MESSAGES);

	/* format message */
	const u64 ms = time_ms();
	msg->time = ms; 
	msg->system = system;
	msg->severity = severity;
	msg->thread_id = thread_id;

	/* TODO: hacky, but good enough for now... */
	u8 buf[LOG_MAX_MESSAGE_SIZE]  = { 0 };
	u64 req_size;
	va_list args;
	va_start(args, format);
	utf8 formatted = utf8_format_buffered_variadic(&req_size, buf, LOG_MAX_MESSAGE_SIZE, format, args);
	va_end(args);

#if __DS_PLATFORM__ == __DS_WEB__
	utf8 str = utf8_format_buffered(msg->buf, LOG_MAX_MESSAGE_SIZE-1, "[%lu.%lu%lu%lus] %k %k - Thread %u: %k",
#else
	utf8 str = utf8_format_buffered(msg->buf, LOG_MAX_MESSAGE_SIZE-1, "[%lu.%lu%lu%lus] %k %k - Thread %u: %k\n",
#endif
		ms / 1000,
		(ms / 100) % 10,
		(ms / 10) % 10,
		ms % 10,
		systems + system,
		severities + severity,
		thread_id,
		&formatted);
	msg->size_req = utf8_size_required(str);
	msg->len = str.len;
	msg->buf[msg->size_req] = '\0';

	if (str.len == 0)
	{
		msg->len = 0;
		AtomicStoreRel32(&msg->a_in_use_and_completed, 1);
		return;
	}
#if __DS_PLATFORM__ == __DS_WEB__
	emscripten_out((char *) msg->buf);
#else
	fprintf(stdout, "%s", msg->buf);
#endif
	/* sync-point, msg ready for writing */
	AtomicStoreRel32(&msg->a_in_use_and_completed, 1);
}

#endif
