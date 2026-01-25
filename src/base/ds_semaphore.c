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

#include "ds_semaphore.h"

#if __DS_PLATFORM__ == __DS_LINUX__ || __DS_PLATFORM__ == __DS_WEB__

#include <errno.h>
#include <stdio.h>

#define SEM_SHARED 1
#define SEM_NOT_SHARED 0

void SemaphoreInit(semaphore *sem, const u32 val)
{
#if __DS_PLATFORM__ == __DS_WEB__
	/* emscripten recommends "inter-process sharing" for thread safety? */
	if (sem_init(sem, SEM_SHARED, val) == -1)
#else
	if (sem_init(sem, SEM_NOT_SHARED, val) == -1)
#endif
	{
		//TODO
		//LOG_SYSTEM_ERROR(S_FATAL);
		//fatal_cleanup_and_exit(0);
	}
}

void SemaphoreDestroy(semaphore *sem)
{
	if (sem_destroy(sem) == -1)
	{
		//TODO
		//LOG_SYSTEM_ERROR(S_FATAL);
		//fatal_cleanup_and_exit(0);
	}	
}

void SemaphorePost(semaphore *sem)
{
	if (sem_post(sem) == -1)
	{
		//TODO
		//LOG_SYSTEM_ERROR(S_FATAL);
		//fatal_cleanup_and_exit(0);
	}
}

u32 SemaphoreWait(semaphore *sem)
{
	u32 success = 1;
	if (sem_wait(sem) == -1)
	{
		if (errno == EINVAL)
		{
			//TODO
			//LOG_SYSTEM_ERROR(S_FATAL);
			//fatal_cleanup_and_exit(0);
		}	

		success = 0;
	}

	return success;
}

u32 SemaphoreTryWait(semaphore *sem)
{
	u32 success = 1;
	if (sem_trywait(sem) == -1)
	{
		if (errno == EINVAL)
		{
			//TODO
			//LOG_SYSTEM_ERROR(S_FATAL);
			//fatal_cleanup_and_exit(0);
		}	

		success = 0;
	}

	return success;
}

#elif _DS_PLATFORM__ == __DS_WIN64__

#define SEM_MAX U16_MAX

void SemaphoreInit(semaphore *sem, const u32 val)	
{
	LPSECURITY_ATTRIBUTES lpSemaphoreAttributes = NULL; /* default */
	*sem = CreateSemaphoreA(lpSemaphoreAttributes, val, SEM_MAX, NULL);
	if (*sem == NULL)
	{
		//TODO
		//Log_system_error(S_FATAL);
		//fatal_cleanup_and_exit(0);
	}
}

void SemaphoreDestroy(semaphore *sem)
{
	/* generally unsafe on its on; undefined behaviour if threads are waiting on the semaphore
	 * while we are closing it? */
	if (!CloseHandle(*sem))
	{
		//TODO
		//Log_system_error(S_FATAL);
		//fatal_cleanup_and_exit(0);
	}
}

void SemaphorePost(semaphore *sem)
{
	u32 sem_val;
	if (!ReleaseSemaphore(*sem, 1, &sem_val))
	{
		//TODO
		//Log_system_error(S_FATAL);
		//fatal_cleanup_and_exit(0);
	}
}

u32 SemaphoreWait(semaphore *sem)
{
	DWORD ret = WaitForSingleObjectEx(*sem, INFINITE, FALSE);
	if (ret != WAIT_OBJECT_0)
	{
		//TODO
		//Log_system_error(S_FATAL);
		//fatal_cleanup_and_exit(0);
	}

	return 1;
}

u32 SemaphoreTryWait(semaphore *sem)
{
	/* return 1 on successful lock aquisition, 0 otherwise. */
	u32 success = 1;
	DWORD ret = WaitForSingleObjectEx(*sem, 0, FALSE);
	if (ret != WAIT_OBJECT_0)
	{
		if (ret == WAIT_TIMEOUT)
		{
			success = 0;
		}
		else
		{
			//TODO
			//Log_system_error(S_FATAL);
			//fatal_cleanup_and_exit(0);
		}
	}

	return success;
}

#endif
