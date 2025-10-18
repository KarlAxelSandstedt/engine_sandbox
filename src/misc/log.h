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

#ifndef __KAS_LOG_H__
#define __KAS_LOG_H__

#include "kas_common.h"

#define LOG_MAX_MESSAGES		512
#define LOG_MAX_MESSAGE_SIZE 		512

#include <stdarg.h>
#include "allocator.h"
#include "kas_string.h"

void 	log_init(struct arena *mem, const char *filepath);
void 	log_shutdown();


/**
 * log_write_message() - Generate a formatted string from the input string and the following arguments and write to the logger.
 */
void 	log_write_message(const enum system_id system, const enum severity_id severity, const char *format, ... );

#ifdef KAS_LOG

#define log_string(system, severity, msg, ...)		log_write_message(system, severity, msg)
#define log(system, severity, msg, ...)			log_write_message(system, severity, msg, __VA_ARGS__)

#else

#define log_string(system, severity, msg, ...)
#define log(system, severity, msg, ...)

#endif

#endif
