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

#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "wasm_public.h"
#include "wasm_local.h"

void (*fatal_cleanup_and_exit)(const u32 thread);

u32 a_fatal_cleanup_initiated;
static void wasm_fatal_cleanup_and_exit(const u32 thread)
{
	u32 desired = 0;
	// TODO Somehow stop/force all threads to not continue / shut down program until cleanup is complete 
	
	if (atomic_compare_exchange_acq_32(&a_fatal_cleanup_initiated, &desired, 1))
	{
		LOG_SHUTDOWN();
	
		//TODO kernel_tracer_shutdown(kt);
		exit(1);
	}

	//TODO spin threads that di not acquire lock or exit or something...
}

void init_error_handling_func_ptrs(void)
{
	atomic_store_rel_32(&a_fatal_cleanup_initiated, 0);
	fatal_cleanup_and_exit = &wasm_fatal_cleanup_and_exit;
}

utf8 utf8_system_error_code_string_buffered(u8 *buf, const u32 bufsize, const u32 code)
{
	struct utf8 err_str = utf8_empty();

	const u32 status = strerror_r(code, (char *) buf, bufsize);
	if (status == 0)
	{
		err_str.buf = buf;
		err_str.len = strnlen((char *) err_str.buf, bufsize);
		err_str.size = bufsize;
		err_str.req_size = utf8_required_size(err_str);
	}

	if (err_str.buf == NULL)
	{
		fprintf(stderr, "Error in %s\n", __func__);
	}


	return err_str;
}
