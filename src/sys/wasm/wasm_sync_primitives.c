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

#include <semaphore.h>
#include <errno.h>
#include "wasm_local.h"
#include "sys_public.h"

#define SEM_NOT_SHARED 0

typedef sem_t semaphore;

void semaphore_init(semaphore *sem, const u32 val)
{
	if (sem_init(sem, SEM_NOT_SHARED, val) == -1)
	{
		LOG_SYSTEM_ERROR(S_FATAL);
		assert(0);
	}
}

void semaphore_destroy(semaphore *sem)
{
	if (sem_destroy(sem) == -1)
	{
		LOG_SYSTEM_ERROR(S_FATAL);
		assert(0);
	}	
}

void semaphore_post(semaphore *sem)
{
	if (sem_post(sem) == -1)
	{
		LOG_SYSTEM_ERROR(S_FATAL);
		assert(0);
	}
}

u32 semaphore_wait(semaphore *sem)
{
	u32 success = 1;
	if (sem_wait(sem) == -1)
	{
		if (errno == EINVAL)
		{
			LOG_SYSTEM_ERROR(S_FATAL);
			assert(0);
		}	

		success = 0;
	}

	return success;
}

u32 semaphore_trywait(semaphore *sem)
{
	u32 success = 1;
	if (sem_trywait(sem) == -1)
	{
		if (errno == EINVAL)
		{
			LOG_SYSTEM_ERROR(S_FATAL);
			assert(0);
		}	

		success = 0;
	}

	return success;
}

//typedef emscripten_semaphore_t semaphore;
//
//void semaphore_init(semaphore *sem, const u32 val)
//{
//  	emscripten_semaphore_init(sem, (int) val);
//	if (!sem)
//	{
//		log_string(T_SYSTEM, S_FATAL, "Failed to initialize emscripten_semaphore");
//		fatal_cleanup_and_exit(0);
//	}
//}
//
//void semaphore_destroy(semaphore *sem)
//{
//	log_string(T_SYSTEM, S_WARNING, "emscripten has no semaphore destroy?");
//}
//
//void semaphore_post(semaphore *sem)
//{
//	const u32 prev_val = emscripten_semaphore_release(sem, 1);
//}
//
//u32 semaphore_wait(semaphore *sem)
//{
//	u32 success;
//	if (emscripten_semaphore_waitinf_acquire(sem, 1) < 0)
//	{
//		success = 0;
//	}
//	else
//	{
//		success = 1;
//	}
//
//	return success;
//}
//
//u32 semaphore_trywait(semaphore *sem)
//{
//	u32 success;
//	if (emscripten_semaphore_try_acquire(sem, 1) == -1)
//	{
//		success = 0;
//	}
//	else
//	{
//		success = 1;
//	}
//
//	return success;
//}
