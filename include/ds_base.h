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

#ifndef __DREAMSCAPE_BASE_H__
#define __DREAMSCAPE_BASE_H__

#ifdef __cplusplus
extern "C" { 
#endif

#include "ds_define.h"
#include "ds_types.h"
#include "ds_atomic.h"
#include "ds_allocator.h"

#ifdef DS_PROFILE
	#include "tracy/TracyC.h"
	#define PROF_FRAME_MARK		TracyCFrameMark
	#define	PROF_ZONE		TracyCZone(ctx, 1)
	#define PROF_ZONE_NAMED(str)	TracyCZoneN(ctx, str, 1)
	#define PROF_ZONE_END		TracyCZoneEnd(ctx)
	#define PROF_THREAD_NAMED(str)	TracyCSetThreadName(str)
#else
	#define PROF_FRAME_MARK		
	#define	PROF_ZONE		
	#define PROF_ZONE_NAMED(str)
	#define PROF_ZONE_END	
	#define PROF_THREAD_NAMED(str)
#endif

#ifdef DS_ASSERT_DEBUG
	#define ds_Assert(assertion)			_ds_assert(assertion, __FILE__, __LINE__, __func__)
	#define ds_AssertString(assertion, cstr)	_ds_assert_string(assertion, __FILE__, __LINE__, __func__, cstr) 
	#define ds_AssertMessage(assertion, msg, ...)	_ds_assert_message(assertion, __FILE__, __LINE__, __func__, msg, __VA_ARGS__)
	
	#define _ds_assert(assertion, file, line, func)								\
		if (assertion) { }										\
		else												\
		{												\
			log(T_ASSERT, S_FATAL, "assertion failed at %s:%u in function %s", file, line, func);	\
			Breakpoint(1);										\
 			fatal_cleanup_and_exit(ds_thread_self_tid());						\
		}
	
	#define _ds_assert_string(assertion, file, line, func, cstr)						\
		if (assertion) { }			     							\
		else												\
		{												\
			log(T_ASSERT, S_FATAL, "assertion failed at %s:%u in function %s - %s", file, line, func, cstr); \
			Breakpoint(1);										\
 			fatal_cleanup_and_exit(ds_thread_self_tid());						\
		}
	
	#define _ds_assert_message(assertion, file, line, func, msg, ...)					\
		if (assertion) { }										\
		else												\
		{												\
			u8 __msg_buf[LOG_MAX_MESSAGE_SIZE];							\
			const utf8 __fmsg = utf8_format_buffered(__msg_buf, LOG_MAX_MESSAGE_SIZE, msg, __VA_ARGS__); \
			log(T_ASSERT, S_FATAL, "assertion failed at %s:%u in function %s - %k", file, line, func, &__fmsg); \
			Breakpoint(1);										\
 			fatal_cleanup_and_exit(ds_thread_self_tid());						\
		}
#else

#define ds_Assert(assertion)
#define ds_AssertString(assertion, str)
#define ds_AssertMessage(assertion, msg, ...)

#endif

//TODO move to timers
/* timer stuff */
#define NSEC_PER_SEC 	1000000000
#define NSEC_PER_MSEC 	1000000
#define NSEC_PER_USEC 	1000

#define GROWABLE	1
#define NOT_GROWABLE	0

/* system identifiers for logger, profiler ... */
enum system_id
{
	T_SYSTEM,
	T_RENDERER,
	T_PHYSICS,
	T_CSG,
	T_ASSET,
	T_UTILITY,
	T_PROFILER,
	T_ASSERT,
	T_GAME,
	T_UI,
	T_LED,
	T_COUNT
};

/* system identifiers for logger, profiler ... */
enum severity_id
{
	S_SUCCESS,
	S_NOTE,
	S_WARNING,
	S_ERROR,
	S_FATAL,
	S_COUNT
};

#ifdef __cplusplus
} 
#endif

#endif
