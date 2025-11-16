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

#ifndef __WIN_LOCAL_H__
#define __WIN_LOCAL_H__

#include <windows.h>
#include "win_public.h"
#include "sys_public.h"
#include "log.h"

/******************** win_thread.c ********************/

struct kas_thread
{
	void		(*start)(kas_thread *);	/* beginning of execution for thread */
	void 		*args;			/* thread arguments */
	void 		*ret;			/* adress to returned value, if any */
	u64		ret_size;		/* size of returned value */
	u64		stack_size;		/* size of stack (not counting protected page at bottom) */
	u32		index;			/* thread index, used for accessing thread data in arrays  */
	DWORD		tid;			/* native thread id (pid_t on linux) */
	HANDLE		native;			/* native thread handle */
};


/************************* win_error.c *************************/

#define log_nt_status(severity, status)	_log_nt_status(severity, status, __func__, __FILE__, __LINE__)
#define _log_nt_status(severity, status, func, file, line)					\
{												\
	u8 _err_buf[ERROR_BUFSIZE];								\
	const utf8 _err_str = utf8_nt_status_buffered(_err_buf, ERROR_BUFSIZE, status);		\
	log(T_SYSTEM, severity, "At %s:%u in function %s - %k\n", file, line, func, &_err_str);	\
}

/* thread safe NTSTATUS error message generation */
utf8	utf8_nt_status_buffered(u8 *buf, const u32 bufsize, const NTSTATUS status);


#endif
