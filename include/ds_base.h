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

#ifndef __DREAMSCAPE_BASE_H__
#define __DREAMSCAPE_BASE_H__

#include "ds_types.h"
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

#endif
