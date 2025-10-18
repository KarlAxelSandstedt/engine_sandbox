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

#ifndef __WASM_LOCAL_H__
#define __WASM_LOCAL_H__

#define _GNU_SOURCE	/* Include all filesystem flags */
//#define _LARGEFILE64_SOURCE

#include <time.h>
#include <errno.h>
#include "kas_common.h"
#include "sys_common.h"
#include "wasm_public.h"
#include "kas_logger.h"

/******************** wasm_thread.c ********************/

#include <setjmp.h>

//TODO force cacheline aligned
//TODO force page alignment
struct kas_thread
{
	void	(*start)(kas_thread *);	/* beginning of execution for thread */
	void 	*args;		/* thread arguments */
	void 	*ret;		/* adress to returned value, if any */
	void	*stack;		/* adress to mprotected page at bottom of stack (top == stack + size + pagesize) */
	u64	ret_size;	/* size of returned value */
	u64	stack_size;	/* size of stack (not counting protected page at bottom) */
	u64	tls_size;

	emscripten_wasm_worker_t wasm_worker;
	tid	tid;		/* (u32) unique wasm thread id for actual workers, 0xffffffff for main thread ? */
	jmp_buf	exit_jmp;
	u32	a_has_exit_jumped;	/* set to 1 after worker thread has longjumped using exit_jmp */
};

/******************** wasm_filesystem.c ********************/

void 		filesystem_init_func_ptrs(void);

/******************** wasm_error.c ********************/

#define ERROR_BUFSIZE	512
#define LOG_SYSTEM_ERROR(severity, thread)		_LOG_SYSTEM_ERROR_CODE(severity, thread, errno, __func__, __FILE__, __LINE__)
#define LOG_SYSTEM_ERROR_CODE(severity, thread, code)	_LOG_SYSTEM_ERROR_CODE(severity, thread, code, __func__, __FILE__, __LINE__)
#define _LOG_SYSTEM_ERROR_CODE(severity, thread, code, func, file, line)					\
{														\
	u8 _err_buf[ERROR_BUFSIZE];										\
	const kas_string _err_str = kas_string_system_error_code_string_buffered(_err_buf, ERROR_BUFSIZE, code, thread);	\
	LOG_MESSAGE(T_SYSTEM, severity, thread, "At %s:%u in function %s - %k\n", file, line, func, &_err_str);	\
}

/* thread safe system error message generation */
kas_string	kas_string_system_error_code_string_buffered(u8 *buf, const u32 bufsize, const u32 code, const u32 thread);

#endif
