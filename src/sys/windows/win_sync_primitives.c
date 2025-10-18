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

#include "win_local.h"

#define SEM_MAX U16_MAX

void semaphore_init(semaphore *sem, const u32 val)	
{
	LPSECURITY_ATTRIBUTES lpSemaphoreAttributes = NULL; /* default */
	*sem = CreateSemaphoreA(lpSemaphoreAttributes, val, SEM_MAX, NULL);
	if (*sem == NULL)
	{
		log_system_error(S_FATAL);
		fatal_cleanup_and_exit(0);
	}
}

void semaphore_destroy(semaphore *sem)
{
	/* generally unsafe on its on; undefined behaviour if threads are waiting on the semaphore
	 * while we are closing it? */
	if (!CloseHandle(*sem))
	{
		log_system_error(S_FATAL);
		fatal_cleanup_and_exit(0);
	}
}

void semaphore_post(semaphore *sem)
{
	u32 sem_val;
	if (!ReleaseSemaphore(*sem, 1, &sem_val))
	{
		log_system_error(S_FATAL);
		fatal_cleanup_and_exit(0);
	}
}

u32 semaphore_wait(semaphore *sem)
{
	DWORD ret = WaitForSingleObjectEx(*sem, INFINITE, FALSE);
	if (ret != WAIT_OBJECT_0)
	{
		log_system_error(S_FATAL);
		fatal_cleanup_and_exit(0);
	}

	return 1;
}

u32 semaphore_trywait(semaphore *sem)
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
			log_system_error(S_FATAL);
			fatal_cleanup_and_exit(0);
		}
	}

	return success;
}
