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

#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "wasm_public.h"
#include "wasm_local.h"

#include "sys_public.h"

void (*fatal_cleanup_and_exit)(const u32 thread);

u32 a_fatal_cleanup_initiated;
static void wasm_fatal_cleanup_and_exit(const u32 thread)
{
	u32 desired = 0;
	// TODO Somehow stop/force all threads to not continue / shut down program until cleanup is complete 
	
	if (AtomicCompareExchangeAcq32(&a_fatal_cleanup_initiated, &desired, 1))
	{
		log_shutdown();
		exit(1);
	}

	//TODO spin threads that di not acquire lock or exit or something...
}

void init_error_handling_func_ptrs(void)
{
	AtomicStoreRel32(&a_fatal_cleanup_initiated, 0);
	fatal_cleanup_and_exit = &wasm_fatal_cleanup_and_exit;
}

utf8 utf8_system_error_code_string_buffered(u8 *buf, const u32 bufsize, const u32 code)
{
	ds_Assert(bufsize > 0);
	utf8 err_str = 
	{
		.len = 0,
		.buf = buf,
		.size = bufsize,
	};

	const u32 status = strerror_r(code, (char *) buf, bufsize);
	if (status != 0)
	{
		if (status == EINVAL)
		{
			LOG_SYSTEM_ERROR_CODE(S_ERROR, status);
		}
		else if (status == ERANGE)
		{
			ds_Assert(0 && "increase system error string buffer size!");
		}

		return utf8_empty();
	}

	err_str.len = strnlen((char *) err_str.buf, ERROR_BUFSIZE);
	if (err_str.len == ERROR_BUFSIZE)
	{
		log(T_SYSTEM, S_ERROR, "strnlen failed to determine string length in %s, most likely due to no null-termination? Fix.", __func__);
		return utf8_empty();
	}

	return err_str;
}
