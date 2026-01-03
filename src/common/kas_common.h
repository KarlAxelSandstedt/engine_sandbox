/*
==========================================================================
    Copyright (C) 2025,2026 Axel Sandstedt 

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

#ifndef __KAS_COMMON_H__
#define __KAS_COMMON_H__

#ifdef KAS_PROFILE
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

/*** logger definitions ***/
#define KAS_LOG		/* LOGGER ON */

/*** assert library definitions ***/

/*** synchronization definitions ***/

/*** system definitions ***/
#define __SDL3__	0
#define __X11__		1
#define __WAYLAND__	2

#define __WIN64__	3
#define __LINUX__	4

#define __GCC__ 	5
#define __MSVC__ 	6

/* OS api */
#if defined(__linux__)
	#define __OS__ 	__LINUX__
#elif defined(__EMSCRIPTEN__)
	#define __OS__ 	__WEB__
#elif defined(_WIN64)
	#define __OS__ 	__WIN64__
#endif

#define __GAPI__ __SDL3__

#if defined(__EMSCRIPTEN__)
	#define __COMPILER__ 		__EMSCRIPTEN__ 
	#define kas_thread_local	__thread
	#ifndef LITTLE_ENDIAN
		#define LITTLE_ENDIAN
	#endif
#elif defined(__GNUC__)
	#define __COMPILER__ 		__GCC__
	#define kas_thread_local	__thread
	#if (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN)
		#define LITTLE_ENDIAN
	#elif (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN)
		#define BIG_ENDIAN
	#endif
#elif defined(_MSC_VER)
	#define __COMPILER__ 		__MSVC__
	#define kas_thread_local	__declspec(thread)
	#define LITTLE_ENDIAN
#endif

#include "kas_types.h"
#include "kas_debug.h"

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

/* timer stuff */
#define NSEC_PER_SEC 	1000000000
#define NSEC_PER_MSEC 	1000000
#define NSEC_PER_USEC 	1000

#define GROWABLE	1
#define NOT_GROWABLE	0

#endif

