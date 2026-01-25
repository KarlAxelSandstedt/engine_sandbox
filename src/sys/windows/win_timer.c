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

#include <windows.h>

#include <intrin.h>
#include "win_local.h"

u64	(*time_ns_start)(void);					/* return origin of process time in sys time */
u64	(*time_s)(void);					/* seconds since start */
u64	(*time_ms)(void); 					/* milliseconds since start */
u64	(*time_us)(void); 					/* microseconds since start */
u64	(*time_ns)(void); 					/* nanoseconds since start */
u64 	(*time_ns_from_tsc)(const u64 tsc);			/* determine time elapsed from start in ns using hw tsc */
u64	(*time_tsc_from_ns)(const u64 ns);			/* determine time elapsed from start in hw tsc using ns */
u64	(*time_ns_from_tsc_truth_source)(const u64 tsc, const u64 ns_truth, const u64 cc_truth); /* determine time elapsed from timer initialisation start in ns using hw tsc, with additional truth pair (ns, tsc) in order to reduce error) */
u64	(*time_tsc_from_ns_truth_source)(const u64 ns, const u64 ns_truth, const u64 cc_truth);  /* determine time elapsed from timer initialisation start in hw tsc using ns  with additional truth pair (ns, tsc) in order to reduce error) */
u64 	(*ns_from_tsc)(const u64 tsc);				/* transform tsc to corresponding ns */
u64	(*tsc_from_ns)(const u64 ns);				/* transform ns to corresponding tsc */
u64	(*time_ns_per_tick)(void);
u64 	(*freq_rdtsc)(void);
f64 	(*time_seconds_from_rdtsc)(const u64 ticks);

struct rdtsc_timer 
{
	u64 tsc_start;
	u64 rdtsc_freq;
};

u64 *g_tsc_skew = NULL;
/*
 * coarse timer for general use
 */
struct timer
{
	u64 ns_start;
	u64 tsc_start;

	u64 ns_resolution;	/* ns per tick */
};

struct rdtsc_timer g_precision_timer;
struct timer g_timer;

static u64 win_ns_from_tsc(const u64 tsc)
{
	const u64 ns = NSEC_PER_SEC * time_seconds_from_rdtsc(tsc);
	return ns;
}

static u64 win_tsc_from_ns(const u64 ns)
{
	return ns * (f64) g_precision_timer.rdtsc_freq / NSEC_PER_SEC;
}

static u64 win_time_ns_from_tsc(const u64 tsc)
{
	ds_Assert(tsc >= g_timer.tsc_start);
	const u64 ns = ns_from_tsc(tsc - g_timer.tsc_start);
	return ns;
}

static u64 win_time_tsc_from_ns(const u64 ns)
{
	ds_Assert(ns >= g_timer.ns_start);
	const u64 cyc = tsc_from_ns(ns - g_timer.ns_start);
	return cyc;
}

static u64 win_time_ns_from_tsc_truth_source(const u64 tsc, const u64 ns_truth, const u64 cc_truth)
{
	if (tsc >= cc_truth)
	{
		const u64 ns = ns_from_tsc(tsc - cc_truth);
		return ns_truth + ns;
	}
	else
	{
		const u64 ns = ns_from_tsc(cc_truth - tsc);
		return ns_truth - ns;
	}
}

static u64 win_time_tsc_from_ns_truth_source(const u64 ns, const u64 ns_truth, const u64 cc_truth)
{
	if (ns >= ns_truth)
	{
		const u64 cc = tsc_from_ns(ns - ns_truth);
		return cc_truth + cc;
	}
	else
	{
		const u64 cc = tsc_from_ns(ns_truth - ns);
		return cc_truth - cc;
	}
}

static u64 win_time_ns(void)
{
	LARGE_INTEGER ret_val;
	QueryPerformanceCounter(&ret_val);
	return (u64) ret_val.QuadPart * g_timer.ns_resolution - g_timer.ns_start;
}

static u64 win_time_ns_start(void)
{
	return g_timer.ns_start;
}

static u64 win_time_s(void)
{
	return win_time_ns() / NSEC_PER_SEC;
}

static u64 win_time_ms(void)
{
	return win_time_ns() / NSEC_PER_MSEC;
}

static u64 win_time_us(void)
{
	return win_time_ns() / NSEC_PER_USEC;
}

static u64 win_time_ns_per_tick(void)
{
	return (u64) g_timer.ns_resolution;
}

static u64 win_freq_rdtsc(void)
{
	return g_precision_timer.rdtsc_freq;
}

static f64 win_time_seconds_from_rdtsc(const u64 ticks)
{
	return (f64) ticks / g_precision_timer.rdtsc_freq;
}

struct ping_pong_data
{
	u32	a_lock;
	u32	a_iteration_test;
	u32	Logical_core_count;
	u32	iterations;
	u64 *	tsc_reference;
	u64 *	tsc_iterator;
};

#define UNLOCKED_BY_REFERENCE 	1
#define UNLOCKED_BY_ITERATOR	2
DWORD WINAPI ping_pong_reference(void *data_void)
{
	struct ping_pong_data *data = data_void;

	const HANDLE thread = GetCurrentThread();

	GROUP_AFFINITY affinity = { 0 };
	affinity.Group = 0;
	affinity.Mask = 0;
	
	if (!SetThreadGroupAffinity(thread, &affinity, NULL))
	{
		LogString(T_SYSTEM, S_FATAL, "Failed to set thread affinity in tsc_estimate_skew, exiting.");	
		fatal_cleanup_and_exit(0);
	}

	u32 c;
	g_tsc_skew[0] = 0;
	for (u32 core = 1; core < data->Logical_core_count; ++core)
	{
		AtomicStoreRel32(&data->a_iteration_test, 1);

		for (u32 i = 0; i < data->iterations; ++i)
		{
			while (AtomicLoadAcq32(&data->a_lock) != UNLOCKED_BY_ITERATOR);
			data->tsc_reference[i] = rdtscp(&c);
			AtomicStoreRel32(&data->a_lock, UNLOCKED_BY_REFERENCE);
		}
		/* wait until last iteration of iterator is complete before calculating skew */
		while (AtomicLoadAcq32(&data->a_iteration_test) != 0);

		b64 skew = { .i = I64_MAX, };
		for (u32 i = 0; i < data->iterations; ++i)
		{
			const b64 it_skew = { .u = data->tsc_iterator[i] - data->tsc_reference[i] };
			if (it_skew.i < skew.i)
			{
				skew = it_skew;
			}
		}

		g_tsc_skew[core] = skew.u;
	}

	return 0;
}

DWORD WINAPI ping_pong_core_iterator(void *data_void)
{
	struct ping_pong_data *data = data_void;

	const HANDLE thread = GetCurrentThread();

	u32 c;
	for (u32 core = 1; core < data->Logical_core_count; ++core)
	{
		GROUP_AFFINITY affinity = { 0 };
		affinity.Group = core / 64;
		affinity.Mask = (u64) 1 << (core % 64);
		
		if (!SetThreadGroupAffinity(thread, &affinity, NULL))
		{
			LogString(T_SYSTEM, S_FATAL, "Failed to set thread affinity in tsc_estimate_skew, exiting.");	
			fatal_cleanup_and_exit(0);
		}

		while (AtomicLoadAcq32(&data->a_iteration_test) != 1);

		AtomicStoreRel32(&data->a_lock, UNLOCKED_BY_ITERATOR);

		for (u32 i = 0; i < data->iterations; ++i)
		{
			while (AtomicLoadAcq32(&data->a_lock) != UNLOCKED_BY_REFERENCE);
			data->tsc_iterator[i] = rdtscp(&c);
			AtomicStoreRel32(&data->a_lock, UNLOCKED_BY_ITERATOR);
		}

		AtomicStoreRel32(&data->a_lock, 0);
		AtomicStoreRel32(&data->a_iteration_test, 0);
	}

	return 0;
}

/*
Ping-Pong calibration of core skew:

Skew Core: (c)	   		      Reference Core: (0)
      	    |	   		  		       |
=================================================================== ITERATION N
       	    |					       |
     [ RELEASE LOCK ] -------------------------> [ GAIN LOCK ]
       	    |					       |
            |					       V
            |                                         TSC() ----> t0_0
       	    |					       |
            V					       V
      [ GAIN LOCK ] <-------------------------- [ RELEASE LOCK ]
       	    |					       |
            V					       |
           TSC() --------------------------------------+--------> tc_1
       	    |					       |
=================================================================== ITERATION N+1
      	    |					       |

It follows that tc_1 = t0_0 + time_execution_instructions + extra + skew.
By running many iterations, we hope that extra goes to 0; so we estimate
the skew by min(tc_1 - t0_0).
 */
static void tsc_estimate_skew(struct arena *persistent)
{	
	SYSTEM_INFO info;
	GetSystemInfo(&info);	

	struct ping_pong_data data =
	{
		.Logical_core_count = (u32) info.dwNumberOfProcessors,
		.iterations = 100000,
		.a_lock = 0,
		.a_iteration_test = 0,
	};

	g_tsc_skew = ArenaPushZero(persistent, data.Logical_core_count*sizeof(u64));
	ArenaPushRecord(persistent);
	data.tsc_reference = ArenaPush(persistent, data.iterations * sizeof(u64));
	data.tsc_iterator = ArenaPush(persistent, data.iterations * sizeof(u64));

  	LPSECURITY_ATTRIBUTES   lpThreadAttributes = NULL; 	/* default attributes */
  	DWORD                   dwCreationFlags = 0;		/* default creation flags */
	DWORD			dwStackSize = 0;
	HANDLE ref = CreateThread(lpThreadAttributes, dwStackSize, ping_pong_reference, (void *) &data, dwCreationFlags, NULL);
	HANDLE iter = CreateThread(lpThreadAttributes, dwStackSize, ping_pong_core_iterator, (void *) &data, dwCreationFlags, NULL);
	if (!ref || !iter)
	{
		Log_system_error(S_FATAL);
		fatal_cleanup_and_exit(0);
	}

	u32 err = 0;
	const u32 res1 = WaitForSingleObject(ref, INFINITE);
	const u32 res2 = WaitForSingleObject(iter, INFINITE);
	if (res1 != WAIT_OBJECT_0 || res2 != WAIT_OBJECT_0)
	{
		Log_system_error(S_FATAL);
		fatal_cleanup_and_exit(0);
	}

	CloseHandle(ref);
	CloseHandle(iter);

	ArenaPopRecord(persistent);
}

void time_init(struct arena *persistent)
{
	LARGE_INTEGER ret_val;
	QueryPerformanceCounter(&ret_val);
	u32 tmp;
	/* instruction fence */
	g_precision_timer.tsc_start = __rdtscp(&tmp);
	g_timer.ns_start = ret_val.QuadPart;
	QueryPerformanceFrequency(&ret_val);
	u64 freq = ret_val.QuadPart;

	g_timer.ns_resolution = NSEC_PER_SEC / freq;
	g_timer.ns_start *= g_timer.ns_resolution;
	g_timer.tsc_start = g_precision_timer.tsc_start;

	const u64 ms = 100;
	const u64 goal = g_timer.ns_start + NSEC_PER_SEC / (1000/ms);
	for (;;)
	{
		QueryPerformanceCounter(&ret_val);
		if (goal <= (u64) ret_val.QuadPart * g_timer.ns_resolution)
		{
			break;
		}
	}
	u64 end = __rdtsc();
	g_precision_timer.rdtsc_freq = (1000/ms) * (end-g_precision_timer.tsc_start);
	tsc_estimate_skew(persistent);

	time_ns_start = &win_time_ns_start;
	time_s = &win_time_s;
	time_ms = &win_time_ms;
	time_us = &win_time_us;
	time_ns = &win_time_ns;
	time_tsc_from_ns = &win_time_tsc_from_ns;
 	time_ns_from_tsc = &win_time_ns_from_tsc;
	time_ns_per_tick = &win_time_ns_per_tick;
	freq_rdtsc = &win_freq_rdtsc;
	time_seconds_from_rdtsc = win_time_seconds_from_rdtsc;
	time_ns_from_tsc_truth_source = &win_time_ns_from_tsc_truth_source;
	time_tsc_from_ns_truth_source = &win_time_tsc_from_ns_truth_source;
	ns_from_tsc = &win_ns_from_tsc;
	tsc_from_ns = &win_tsc_from_ns;
}
