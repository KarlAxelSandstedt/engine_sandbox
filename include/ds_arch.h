/*
==========================================================================
    Copyright (C) 2025, 2026 Axel Sandstedt 

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

#ifndef __DREAMSCAPE_ARCH_H__
#define __DREAMSCAPE_ARCH_H__

#ifdef __cplusplus
extern "C" { 
#endif

#include "ds_string.h"

#if __DS_PLATFORM__ == __DS_LINUX__ || __DS_PLATFORM__ == __DS_WEB__

#include <sys/types.h>
typedef pid_t	pid;

#elif __DS_PLATFORM__ == __DS_WIN64__ 

#include <windows.h>
typedef DWORD	pid;

#endif

enum arch_type
{
	ARCH_INTEL64,
	ARCH_AMD64,
};

struct ds_arch_config
{
	utf8 	vendor_string;
	utf8	processor_string;

	enum arch_type 	type;
	u32		logical_core_count;
	pid		pid;

	u64		pagesize;	/* bytes */
	u64		cacheline;	/* bytes */

	/* cpuid flags */
	u32		sse : 1;
	u32		sse2 : 1;
	u32		sse3 : 1;
	u32		ssse3 : 1;
	u32		sse4_1 : 1;
	u32		sse4_2 : 1;
	u32		avx : 1;
	u32		avx2 : 1;
	u32		bmi1 : 1;	/* bit manipulation instructions (ctz, ctzl, ...) */

	u32		Rdtsc : 1;	/* profiling timer support  */
	u32		Rdtscp : 1;	/* profiling timer support  */
	u32		tsc_invariant : 1;  /* tsc works as a wallclock timer, always ticking and at same frequency */
};

extern const struct ds_arch_config *g_arch_config;

/* return Logical core count  */
u32  	system_logical_core_count(void);
/* return system pagesize */
u64	system_pagesize(void);
/* return process identifier */
pid	system_pid(void);
/* cpu x86 querying */
void 	ds_cpuid(u32 *eax, u32 *ebx, u32 *ecx, u32 *edx, const u32 function);
void 	ds_cpuid_ex(u32 *eax, u32 *ebx, u32 *ecx, u32 *edx, const u32 function, const u32 subfunction);

/* sets up g_arch_config. returns 1 on intrinsics requirements fullfilled, 0 otherwise. */
u32 	ds_arch_config_init(struct arena *mem);

#ifdef __cplusplus
} 
#endif

#endif
