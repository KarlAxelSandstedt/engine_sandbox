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
#include <intrin.h>

void 		(*ds_cpuid)(u32 *eax, u32 *ebx, u32 *ecx, u32 *edx, const u32 function);
void 		(*ds_cpuid_ex)(u32 *eax, u32 *ebx, u32 *ecx, u32 *edx, const u32 function, const u32 subfunction);
u32  		(*system_logical_core_count)(void);
u64  		(*system_pagesize)(void);
pid		(*system_pid)(void);

static void win_kas_cpuid(u32 *eax, u32 *ebx, u32 *ecx, u32 *edx, const u32 function)
{
	int regs[4];		
	__cpuid(regs, function);
	*eax = (u32) regs[0];	
	*ebx = (u32) regs[1];	
	*ecx = (u32) regs[2];	
	*edx = (u32) regs[3];	
}

static void win_kas_cpuid_ex(u32 *eax, u32 *ebx, u32 *ecx, u32 *edx, const u32 function, const u32 subfunction)
{
	int regs[4];				
	__cpuidex(regs, function, subfunction);
	*eax = (u32) regs[0];			
	*ebx = (u32) regs[1];			
	*ecx = (u32) regs[2];			
	*edx = (u32) regs[3];
}

static u32 win_logical_core_count(void)
{
	SYSTEM_INFO info;
	GetSystemInfo(&info);
	return (u32) info.dwNumberOfProcessors;
}

static u64 win_system_pagesize(void)
{
	SYSTEM_INFO info;
	GetSystemInfo(&info);
	return info.dwPageSize;
}

static pid win_pid(void)
{
	return GetCurrentProcessId();
}

void os_arch_init_func_ptrs(void)
{
	ds_cpuid = &win_kas_cpuid;
	ds_cpuid_ex = &win_kas_cpuid_ex;
	system_logical_core_count = &win_logical_core_count;
	system_pagesize = &win_system_pagesize;
	system_pid = &win_pid;
}

/* returns reserved page aligned virtual memory on success, NULL on failure. */
void *virtual_memory_reserve(const u64 size)
{
	void *addr = VirtualAlloc(NULL, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
	if (addr == NULL)
	{
		Log_system_error(S_ERROR);	
	}

	return addr;
}

/* free reserved virtual memory */
void virtual_memory_release(void *addr, const u64 garbage)
{
	if (VirtualFree(addr, 0, MEM_RELEASE) == 0)
	{
		Log_system_error(S_ERROR);	
	}
}
