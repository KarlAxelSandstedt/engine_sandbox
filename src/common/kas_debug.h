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

#ifndef __KAS_DEBUG_H__
#define __KAS_DEBUG_H__

/* Should be included at top of file */

/*** synchronization debug definitions ***/
//#define FORCE_SEQ_CST

#include <stdio.h>
#include <assert.h>

#define KAS_PHYSICS_DEBUG	/* Physics debug events, physics debug rendering	*/

#ifdef KAS_DEBUG

#define KAS_ASSERT_DEBUG 	/* Asserts on 						*/

#endif

/*** enables opengl debugging ***/

//TODO  How does debug callbacks work multi-context, context local func_ptrs?
//#define KAS_GL_DEBUG		/* graphics debugging */

#endif
