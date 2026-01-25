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

#ifndef _LINUX_LOCAL_H__
#define _LINUX_LOCAL_H__

#define _GNU_SOURCE	/* Include all filesystem flags */
//#define _LARGEFILE64_SOURCE

#include <time.h>
#include <errno.h>
#include "ds_common.h"
#include "sys_common.h"
#include "linux_public.h"
#include "Log.h"

/******************** linux_thread.c ********************/

#include <pthread.h>

/*
 *	Assuming we use CLONE_THREAD, we have the following:
 *	PPID - parent pid of master thread			   (Shared between all threads)
 *	TGID - thread group id, and the unique TID of the master thread (Shared between all threads due to CLONE_THREAD)
 *	TID  - unique thread identifier
 *
 *	We need CLONE_THREAD for kernel tracing, which means that we cannot use waitpid anymore on cloned threads;
 *	instead cpid is cleared to 0 on thread exit.
 *
 *	The PPID of a thread is retrieved by getppid();
 *	The TGID of a thread is retrieved by getpid();
 *	The TID of a thread is retrieved by gettid();
 */

struct ds_thread
{
	void		(*start)(ds_thread *);	/* beginning of execution for thread */
	void 		*args;			/* thread arguments */
	void 		*ret;			/* adress to returned value, if any */
	u64		ret_size;		/* size of returned value */
	u64		stack_size;		/* size of stack (not counting protected page at bottom) */
	pid_t		ppid;			/* native parent tid */
	pid_t		gtid;			/* native group tid */
	tid		tid;			/* native thread id (pid_t on linux) */
	u32		index;			/* thread index, used for accessing thread data in arrays  */
	pthread_t	pthread;		/* thread internal */
};

#endif
