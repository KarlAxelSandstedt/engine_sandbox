/*
==========================================================================
    Copyright (C) 2026 Axel Sandstedt 

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

#ifndef __DREAMSCAPE_ERROR_H__
#define __DREAMSCAPE_ERROR_H__

#ifdef __cplusplus
extern "C" { 
#endif

#include <errno.h>

#include "ds_string.h"

/* Generate stacktrace and shutdown log gracefully, then exits */
void 	FatalCleanupAndExit(void);

/* thread safe last system error or code message generation */
utf8	Utf8SystemErrorCodeStringBuffered(u8 *buf, const u32 bufsize, const u32 code);


#ifdef __cplusplus
} 
#endif

#endif
