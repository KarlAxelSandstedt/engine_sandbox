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

#include <errno.h>
#include <stdio.h>
#include "wasm_local.h"

typedef emscripten_semaphore_t semaphore;

void semaphore_init(semaphore *sem, const u32 val)
{
  	emscripten_semaphore_init(sem, (int) val);
	if (!sem)
	{
		LOG_STRING(T_SYSTEM, S_FATAL, 0, "Failed to initialize emscripten_semaphore\n");
		fatal_cleanup_and_exit(0);
	}
}

void semaphore_destroy(semaphore *sem)
{
	LOG_STRING(T_SYSTEM, S_WARNING, 0, "emscripten has no semaphore destroy?\n");
}

void semaphore_post(semaphore *sem)
{
	const u32 prev_val = emscripten_semaphore_release(sem, 1);
}

u32 semaphore_wait(semaphore *sem)
{
	u32 success;
	if (emscripten_semaphore_waitinf_acquire(sem, 1) < 0)
	{
		success = 0;
	}
	else
	{
		success = 1;
	}

	return success;
}

u32 semaphore_trywait(semaphore *sem)
{
	u32 success;
	if (emscripten_semaphore_try_acquire(sem, 1) == -1)
	{
		success = 0;
	}
	else
	{
		success = 1;
	}

	return success;
}
