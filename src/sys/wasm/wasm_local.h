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

#ifndef __WASM_LOCAL_H__
#define __WASM_LOCAL_H__

#define _GNU_SOURCE	/* Include all filesystem flags */
//#define _LARGEFILE64_SOURCE

#include <time.h>
#include <errno.h>
#include "kas_common.h"
#include "sys_common.h"
#include "wasm_public.h"
#include "log.h"

/******************** wasm_thread.c ********************/

#include <pthread.h>

//#include <setjmp.h>

struct kas_thread
{
	void		(*start)(kas_thread *);	/* beginning of execution for thread */
	void 		*args;			/* thread arguments */
	void 		*ret;			/* adress to returned value, if any */
	void		*stack;			/* adress to mprotected page at bottom of stack (top == stack + size + pagesize) */
	u64		ret_size;		/* size of returned value */
	u64		stack_size;		/* size of stack (not counting protected page at bottom) */
	//u64		tls_size;		/* size of tls */
	tid		tid;			/* (u32) unique wasm thread id for actual workers, 0xffffffff for main thread? */
	u32		index;			/* thread index, used for accessing thread data in arrays  */
	pthread_t 	pthread;
	//emscripten_wasm_worker_t wasm_worker;	/*thread internal */
	//jmp_buf	exit_jmp;
	//u32	a_has_exit_jumped;		/* set to 1 after worker thread has longjumped using exit_jmp */
};

#endif
