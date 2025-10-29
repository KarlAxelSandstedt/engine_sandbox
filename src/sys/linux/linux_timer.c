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

#define _GNU_SOURCE
#include "sys_common.h"
#include "linux_local.h"
#include <sys/time.h>
#include <time.h>
#include <x86intrin.h>
#include <pthread.h>

#include "sys_public.h"

u64 *	g_tsc_skew;

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
u64	(*tsc_from_ns)(const u64 kt);				/* transform ns to corresponding tsc */
u64 	(*tsc_from_kt)(const u64 tsc);				/* transform kernel trace value to corresponding tsc */
u64	(*kt_from_tsc)(const u64 kt);				/* transform tsc to corresponding kernel trace value */
u64	(*time_ns_per_tick)(void);
void 	(*time_set_kt_transform_parameters)(const u64 time_mult, const u64 time_zero, const u64 time_shift);
u64 	(*freq_rdtsc)(void);
f64 	(*time_seconds_from_rdtsc)(const u64 ticks);


struct rdtsc_timer 
{
	u64 tsc_start;
	u64 rdtsc_freq;
};

/*
 * coarse timer for general use
 */
struct timer
{
	/* see ns <-> tsc from man perf_event_open */
	u64 time_mult;
	u64 time_zero;
	u64 time_shift;

	u64 ns_start;
	u64 tsc_start;
	u64 kt_start;

	u64 ns_resolution;	/* ns per tick */
};

struct rdtsc_timer g_precision_timer;
struct timer g_timer;

void linux_time_set_kt_transform_parameters(const u64 time_mult, const u64 time_zero, const u64 time_shift)
{
	g_timer.time_mult = time_mult;
	g_timer.time_zero = time_zero;
	g_timer.time_shift = time_shift;
	g_timer.kt_start = kt_from_tsc(g_timer.tsc_start);
}

u64 linux_kt_from_tsc(const u64 tsc)
{
	const u64 quot = tsc >> g_timer.time_shift;
	const u64 rem  = tsc & (((u64)1 << g_timer.time_shift) - 1);
	const u64 kt_time = g_timer.time_zero + quot * g_timer.time_mult + ((rem * g_timer.time_mult) >> g_timer.time_shift);
	return kt_time;
}

u64 linux_tsc_from_kt(const u64 kt_time)
{
	const u64 time = kt_time - g_timer.time_zero;
	const u64 quot = time / g_timer.time_mult;
	const u64 rem = time % g_timer.time_mult;
	const u64 tsc = (quot << g_timer.time_shift) + (rem << g_timer.time_shift) / g_timer.time_mult;

	return tsc;
}

u64 linux_ns_from_tsc(const u64 tsc)
{
	const u64 ns = NSEC_PER_SEC * time_seconds_from_rdtsc(tsc);
	return ns;
}

u64 linux_tsc_from_ns(const u64 ns)
{
	return ns * (f64) g_precision_timer.rdtsc_freq / NSEC_PER_SEC;
}

u64 linux_time_ns_from_tsc(const u64 tsc)
{
	kas_assert(tsc >= g_timer.tsc_start);
	const u64 ns = ns_from_tsc(tsc - g_timer.tsc_start);
	return ns;
}

u64 linux_time_tsc_from_ns(const u64 ns)
{
	kas_assert(ns >= g_timer.ns_start);
	const u64 cyc = tsc_from_ns(ns - g_timer.ns_start);
	return cyc;
}

u64 linux_time_ns_from_tsc_truth_source(const u64 tsc, const u64 ns_truth, const u64 cc_truth)
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

u64 linux_time_tsc_from_ns_truth_source(const u64 ns, const u64 ns_truth, const u64 cc_truth)
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

u64 linux_time_ns_start(void)
{
	return g_timer.ns_start;
}

u64 linux_time_ns_from_os_source(const u64 ns_os_time)
{
	kas_assert(ns_os_time >= g_timer.ns_start);
	return ns_os_time - g_timer.ns_start;
}

static u64 linux_time_ns(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
	return (u64) NSEC_PER_SEC * ts.tv_sec + ts.tv_nsec - g_timer.ns_start;
}

static u64 linux_time_s(void)
{
	return linux_time_ns() / NSEC_PER_SEC;
}

static u64 linux_time_ms(void)
{
	return linux_time_ns() / NSEC_PER_MSEC;
}

static u64 linux_time_us(void)
{
	return linux_time_ns() / NSEC_PER_USEC;
}

static u64 linux_time_ns_per_tick(void)
{
	return g_timer.ns_resolution;
}

static u64 linux_freq_rdtsc(void)
{
	return g_precision_timer.rdtsc_freq;
}

f64 linux_time_seconds_from_rdtsc(const u64 ticks)
{
	return (f64) ticks / g_precision_timer.rdtsc_freq;
}

struct ping_pong_data
{
	u32	a_lock;
	u32	a_iteration_test;
	u32	logical_core_count;
	u32	iterations;
	u64 *	tsc_reference;
	u64 *	tsc_iterator;
};

#define UNLOCKED_BY_REFERENCE 	1
#define UNLOCKED_BY_ITERATOR	2
void *ping_pong_reference(void *data_void)
{
	struct ping_pong_data *data = data_void;

	cpu_set_t cpuset;
	CPU_ZERO(&cpuset);
	CPU_SET(0, &cpuset);	

	/* thread is allowed to run on intersection of cpu_set_t and actual system cpus. */
	if (pthread_setaffinity_np(pthread_self(), sizeof(cpuset), &cpuset) != 0)
	{
		log_string(T_SYSTEM, S_FATAL, "Failed to set thread affinity in tsc_estimate_skew, exiting.");	
		fatal_cleanup_and_exit(gettid());
	}

	u32 c;
	g_tsc_skew[0] = 0;
	for (u32 core = 1; core < data->logical_core_count; ++core)
	{
		atomic_store_rel_32(&data->a_iteration_test, 1);

		for (u32 i = 0; i < data->iterations; ++i)
		{
			while (atomic_load_acq_32(&data->a_lock) != UNLOCKED_BY_ITERATOR);
			data->tsc_reference[i] = rdtscp(&c);
			atomic_store_rel_32(&data->a_lock, UNLOCKED_BY_REFERENCE);
		}
		/* wait until last iteration of iterator is complete before calculating skew */
		while (atomic_load_acq_32(&data->a_iteration_test) != 0);

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

	return NULL;
}

void *ping_pong_core_iterator(void *data_void)
{
	struct ping_pong_data *data = data_void;

	u32 c;
	for (u32 core = 1; core < data->logical_core_count; ++core)
	{
		cpu_set_t cpuset;
		CPU_ZERO(&cpuset);
		CPU_SET(core, &cpuset);	

		/* thread is allowed to run on intersection of cpu_set_t and actual system cpus. */
		if (pthread_setaffinity_np(pthread_self(), sizeof(cpuset), &cpuset) != 0)
		{
			log_string(T_SYSTEM, S_FATAL, "Failed to set thread affinity in tsc_estimate_skew, exiting.");	
			fatal_cleanup_and_exit(gettid());
		
		}

		while (atomic_load_acq_32(&data->a_iteration_test) != 1);

		atomic_store_rel_32(&data->a_lock, UNLOCKED_BY_ITERATOR);

		for (u32 i = 0; i < data->iterations; ++i)
		{
			while (atomic_load_acq_32(&data->a_lock) != UNLOCKED_BY_REFERENCE);
			data->tsc_iterator[i] = rdtscp(&c);
			atomic_store_rel_32(&data->a_lock, UNLOCKED_BY_ITERATOR);
		}

		atomic_store_rel_32(&data->a_lock, 0);
		atomic_store_rel_32(&data->a_iteration_test, 0);
	}

	return NULL;
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

	struct ping_pong_data data =
	{
		.logical_core_count = (u32) sysconf(_SC_NPROCESSORS_ONLN),
		.iterations = 100000,
		.a_lock = 0,
		.a_iteration_test = 0,
	};

	g_tsc_skew = arena_push_zero(persistent, data.logical_core_count*sizeof(u64));
	arena_push_record(persistent);
	data.tsc_reference = arena_push(persistent, data.iterations * sizeof(u64));
	data.tsc_iterator = arena_push(persistent, data.iterations * sizeof(u64));

	pthread_attr_t attr;
	if (pthread_attr_init(&attr) != 0)
	{
		LOG_SYSTEM_ERROR(S_FATAL);	
		fatal_cleanup_and_exit(gettid());
	}

	pthread_t thread1, thread2;
	if (pthread_create(&thread1, &attr, ping_pong_reference, (void *) &data) != 0
	    || pthread_create(&thread2, &attr, ping_pong_core_iterator, (void *) &data) != 0)

	{
		LOG_SYSTEM_ERROR(S_FATAL);	
		fatal_cleanup_and_exit(gettid());
	}

	if (pthread_attr_destroy(&attr) != 0)
	{
		LOG_SYSTEM_ERROR(S_FATAL);	
		fatal_cleanup_and_exit(gettid());
	}

	void *garbage;
	pthread_join(thread1, &garbage);
	pthread_join(thread2, &garbage);
	arena_pop_record(persistent);
}

void time_init(struct arena *persistent)
{
	struct timespec ts, tmp;
	clock_getres(CLOCK_MONOTONIC_RAW, &ts);
	g_timer.ns_resolution = ts.tv_nsec;
	assert(ts.tv_sec == 0);

	clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
	u32 tmp2;
	/* instruction fence */
	g_precision_timer.tsc_start = __rdtscp(&tmp2);
	g_timer.ns_start = NSEC_PER_SEC * ts.tv_sec + ts.tv_nsec;
	g_timer.tsc_start = g_precision_timer.tsc_start;

	clock_gettime(CLOCK_MONOTONIC_RAW, &tmp);
	const u64 ms = 100;
	const u64 goal = (tmp.tv_sec) * NSEC_PER_SEC + tmp.tv_nsec + NSEC_PER_SEC / (1000/ms);
	while ((u64) (tmp.tv_sec * NSEC_PER_SEC + tmp.tv_nsec) < goal)
	{
		clock_gettime(CLOCK_MONOTONIC_RAW, &tmp);
	}
	u64 end = __rdtsc();
	g_precision_timer.rdtsc_freq = (1000/ms) * (end - g_precision_timer.tsc_start);

	tsc_estimate_skew(persistent);

	time_ns_start = &linux_time_ns_start;
	time_s = &linux_time_s;
	time_ms = &linux_time_ms;
	time_us = &linux_time_us;
	time_ns = &linux_time_ns;
	time_ns_per_tick = &linux_time_ns_per_tick;
	time_set_kt_transform_parameters = &linux_time_set_kt_transform_parameters;
	time_ns_from_tsc = &linux_time_ns_from_tsc;
	time_tsc_from_ns = &linux_time_tsc_from_ns;
	time_ns_from_tsc_truth_source = &linux_time_ns_from_tsc_truth_source;
	time_tsc_from_ns_truth_source = &linux_time_tsc_from_ns_truth_source;
 	ns_from_tsc = &linux_ns_from_tsc;
	tsc_from_ns = &linux_tsc_from_ns;
	tsc_from_kt = &linux_tsc_from_kt;
	kt_from_tsc = &linux_kt_from_tsc;

	freq_rdtsc = &linux_freq_rdtsc;
	time_seconds_from_rdtsc = &linux_time_seconds_from_rdtsc;
}
