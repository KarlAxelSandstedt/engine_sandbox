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
#include <sys/sysinfo.h>
#include <cpuid.h>
#include "linux_local.h"
#include "sys_public.h"

void 		(*kas_cpuid)(u32 *eax, u32 *ebx, u32 *ecx, u32 *edx, const u32 function);
void 		(*kas_cpuid_ex)(u32 *eax, u32 *ebx, u32 *ecx, u32 *edx, const u32 function, const u32 subfunction);
u32  		(*system_logical_core_count)(void);
u64  		(*system_pagesize)(void);
pid		(*system_pid)(void);

static void linux_kas_cpuid(u32 *eax, u32 *ebx, u32 *ecx, u32 *edx, const u32 function)
{
	const int supported = __get_cpuid(function, eax, ebx, ecx, edx);
	if (!supported)
	{
		*eax = 0;
		*ebx = 0;
		*ecx = 0;
		*edx = 0;
	}
}

static void linux_kas_cpuid_ex(u32 *eax, u32 *ebx, u32 *ecx, u32 *edx, const u32 function, const u32 subfunction)
{
	const int supported = __get_cpuid_count(function, subfunction, eax, ebx, ecx, edx);
	if (!supported)
	{
		*eax = 0;
		*ebx = 0;
		*ecx = 0;
		*edx = 0;
	}
}

static u32 linux_logical_core_count(void)
{
	errno = 0;
	long count = sysconf(_SC_NPROCESSORS_ONLN);
	if (errno != 0)
	{
		LOG_SYSTEM_ERROR(S_ERROR);	
		log_string(T_SYSTEM, S_WARNING, "Failed to retrieve number of logical cores, defaulting to 2");
		count = 2;
	}

	return (u32) count;
}

static u64 linux_pagesize(void)
{
	return (u64) getpagesize();
}

pid linux_pid(void)
{
	return getpid();
}

void *virtual_memory_reserve(const u64 size)
{
	void *addr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (addr == MAP_FAILED)
	{
		LOG_SYSTEM_ERROR(S_ERROR);	
		addr = NULL;
	}
	return addr;
}

void virtual_memory_release(void *addr, const u64 size)
{
	if (munmap(addr, size) == -1)
	{
		LOG_SYSTEM_ERROR(S_ERROR);	
	}
}

void os_arch_init_func_ptrs(void)
{
	kas_cpuid = &linux_kas_cpuid;
	kas_cpuid_ex = &linux_kas_cpuid_ex;
	system_logical_core_count = &linux_logical_core_count;
	system_pagesize = &linux_pagesize;
	system_pid = &linux_pid;
}
