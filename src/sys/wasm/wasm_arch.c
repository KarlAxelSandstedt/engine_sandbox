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

#include <unistd.h>
#include "wasm_local.h"

void 		(*kas_cpuid)(u32 *eax, u32 *ebx, u32 *ecx, u32 *edx, const u32 function);
void 		(*kas_cpuid_ex)(u32 *eax, u32 *ebx, u32 *ecx, u32 *edx, const u32 function, const u32 subfunction);
u32  		(*system_logical_core_count)(void);
u64  		(*system_pagesize)(void);

/*
 * returns number of logical (threads) available for the user
 */
static u32 wasm_logical_core_count(void)
{
	u32 count = (u32) emscripten_navigator_hardware_concurrency();
	/* returns navigator.hardwareConcurrency (logical threads aviable for the user agent), on 0, no support. */
	if (count == 0) 
	{ 
		LOG_STRING(T_SYSTEM, S_WARNING, 0, "navigator.hardwareConcurrency = 0, threading not supported\n");
		count = 1; 
	}
	LOG_MESSAGE(T_SYSTEM, S_NOTE, 0, "thread count : %u\n", count);
	return count;
}

static u64 wasm_pagesize(void)
{
	return (u64) getpagesize();
}

void os_arch_init_func_ptrs(void)
{
	kas_cpuid = NULL;
	kas_cpuid_ex = NULL;
	system_logical_core_count = &wasm_logical_core_count;
	system_pagesize = &wasm_pagesize;
}
