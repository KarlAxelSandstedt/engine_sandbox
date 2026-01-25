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

#ifndef __DREAMSCAPE_LOG_H__
#define __DREAMSCAPE_LOG_H__

#ifdef __cplusplus
extern "C" { 
#endif

#include <stdarg.h>

#include "ds_allocator.h"

#define LOG_MAX_MESSAGES		512
#define LOG_MAX_MESSAGE_SIZE 		512

void 	LogInit(struct arena *mem, const char *filepath);
void 	LogShutdown();


/**
 * LogWriteMessage() - Generate a formatted string from the input string and the following arguments and write to the Logger.
 */
void 	LogWriteMessage(const enum system_id system, const enum severity_id severity, const char *format, ... );

#ifdef DS_LOG

#define LogString(system, severity, msg, ...)		LogWriteMessage(system, severity, msg)
#define Log(system, severity, msg, ...)			LogWriteMessage(system, severity, msg, __VA_ARGS__)

#else

#define LogString(system, severity, msg, ...)
#define Log(system, severity, msg, ...)

#endif

#ifdef __cplusplus
} 
#endif

#endif
