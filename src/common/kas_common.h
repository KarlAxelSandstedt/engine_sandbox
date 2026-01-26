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

#ifndef __DS_COMMON_H__
#define __DS_COMMON_H__

#ifdef DS_PROFILE
	#include "tracy/TracyC.h"
	#define ProfFrameMark		TracyCFrameMark
	#define	ProfZone		TracyCZone(ctx, 1)
	#define ProfZoneNamed(str)	TracyCZoneN(ctx, str, 1)
	#define ProfZoneEnd		TracyCZoneEnd(ctx)
	#define ProfThreadNamed(str)	TracyCSetThreadName(str)
#else
	#define ProfFrameMark		
	#define	ProfZone		
	#define ProfZoneNamed(str)
	#define ProfZoneEnd	
	#define ProfThreadNamed(str)
#endif

/*** Logger definitions ***/
#define DS_LOG		/* LOGGER ON */

/*** assert library definitions ***/

/*** synchronization definitions ***/

/*** system definitions ***/
#define __DS_SDL3__	0
#define __DS_X11__		1
#define __DS_WAYLAND__	2

#define __DS_WIN64__	3
#define __DS_LINUX__	4
#define __DS_WEB__		5

#define __DS_GCC__ 	6
#define __DS_MSVC__ 	7

/* OS api */
#if defined(__linux__)
	#define __DS_PLATFORM__ 	__DS_LINUX__
#elif defined(__EMSCRIPTEN__)
	#define __DS_PLATFORM__ 	__DS_WEB__
#elif defined(_WIN64)
	#define __DS_PLATFORM__ 	__DS_WIN64__
#endif

#define __GAPI__ __DS_SDL3__

#if defined(__EMSCRIPTEN__)
	#define __COMPILER__ 		__EMSCRIPTEN__ 
	#define dsThreadLocal	__thread
	#ifndef DS_LITTLE_ENDIAN
		#define DS_LITTLE_ENDIAN
	#endif
#elif defined(__GNUC__)
	#define __COMPILER__ 		__DS_GCC__
	#define dsThreadLocal	__thread
	#if (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN)
		#define DS_LITTLE_ENDIAN
	#elif (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN)
		#define DS_BIG_ENDIAN
	#endif
#elif defined(_MSC_VER)
	#define __COMPILER__ 		__DS_MSVC__
	#define dsThreadLocal	__declspec(thread)
	#define DS_LITTLE_ENDIAN
#endif

#include "ds_types.h"
#include "ds_debug.h"

/* system identifiers for Logger, profiler ... */
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

/* system identifiers for Logger, profiler ... */
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

