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
	u64 ns_start;
	f64 ns_resolution;	/* ns per tick */
};

struct rdtsc_timer g_precision_timer;
struct timer g_timer;

static void win_time_set_kt_transform_parameters(const u64 time_mult, const u64 time_zero, const u64 time_shift)
{
	kas_assert_string(0, "implement");
}

static u64 win_kt_from_tsc(const u64 tsc)
{
	kas_assert_string(0, "implement");
	return 0;
}

static u64 win_tsc_from_kt(const u64 kt_time)
{
	kas_assert_string(0, "implement");
	return 0;
}

static u64 win_ns_from_tsc(const u64 tsc)
{
	kas_assert_string(0, "implement");
	return 0;
}

static u64 win_tsc_from_ns(const u64 ns)
{
	kas_assert_string(0, "implement");
	return 0;
}

static u64 win_time_ns_from_tsc(const u64 tsc)
{
	kas_assert_string(0, "implement");
	return 0;
}

static u64 win_time_tsc_from_ns(const u64 ns)
{
	kas_assert_string(0, "implement");
	return 0;
}

static u64 win_time_ns_from_tsc_truth_source(const u64 tsc, const u64 ns_truth, const u64 cc_truth)
{
	kas_assert_string(0, "implement");
	return 0;
}

static u64 win_time_tsc_from_ns_truth_source(const u64 ns, const u64 ns_truth, const u64 cc_truth)
{
	kas_assert_string(0, "implement");
	return 0;
}

static u64 win_time_ns(void)
{
	LARGE_INTEGER ret_val;
	QueryPerformanceCounter(&ret_val);
	return (u64) (g_timer.ns_resolution * (ret_val.QuadPart - g_timer.ns_start));
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

static void tsc_estimate_skew(struct arena *persistent)
{
	kas_assert_string(0, "implement tsc_estimate_skew");
}

void time_init(struct arena *persistent)
{
	LARGE_INTEGER ret_val;
	QueryPerformanceCounter(&ret_val);

	g_timer.ns_start = ret_val.QuadPart;
	QueryPerformanceFrequency(&ret_val);
	u32 tmp;
	/* instruction fence */
	g_precision_timer.tsc_start = __rdtscp(&tmp);

	u64 freq = ret_val.QuadPart;
	g_timer.ns_resolution = (f64) NSEC_PER_SEC / freq;

	const u64 ms = 100;
	const u64 goal = g_timer.ns_start + freq / (1000/ms);
	for (;;)
	{
		QueryPerformanceCounter(&ret_val);
		if (goal <= (u64) ret_val.QuadPart)
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
 	tsc_from_kt = &win_tsc_from_kt;
	kt_from_tsc = &win_kt_from_tsc;
	time_set_kt_transform_parameters = &win_time_set_kt_transform_parameters;
}
