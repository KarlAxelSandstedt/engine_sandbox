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

#define _GNU_SOURCE
#include <sys/time.h>
#include <time.h>

#include "sys_public.h"
#include "wasm_local.h"

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

struct timer
{
	u64 ns_start;
	u64 ns_resolution;	/* ns per tick */
};

struct timer g_timer;

u64 wasm_time_ns_start(void)
{
	return g_timer.ns_start;
}

static u64 wasm_time_ns(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (u64) NSEC_PER_SEC * ts.tv_sec + ts.tv_nsec - g_timer.ns_start;
}

static u64 wasm_time_s(void)
{
	return wasm_time_ns() / NSEC_PER_SEC;
}

static u64 wasm_time_ms(void)
{
	return wasm_time_ns() / NSEC_PER_MSEC;
}

static u64 wasm_time_us(void)
{
	return wasm_time_ns() / NSEC_PER_USEC;
}

static u64 wasm_time_ns_per_tick(void)
{
	return g_timer.ns_resolution;
}

void time_init(struct arena *persistent)
{
	struct timespec ts;
	clock_getres(CLOCK_MONOTONIC, &ts);
	g_timer.ns_resolution = ts.tv_nsec;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	g_timer.ns_start = NSEC_PER_SEC * ts.tv_sec + ts.tv_nsec;

	time_ns_start = &wasm_time_ns_start;
	time_s = &wasm_time_s;
	time_ms = &wasm_time_ms;
	time_us = &wasm_time_us;
	time_ns = &wasm_time_ns;
	time_ns_per_tick = &wasm_time_ns_per_tick;

	time_ns_from_tsc = NULL;
	time_tsc_from_ns = NULL;
	time_ns_from_tsc_truth_source = NULL;
	time_tsc_from_ns_truth_source = NULL;
 	ns_from_tsc = NULL;
	tsc_from_ns = NULL;
	freq_rdtsc = NULL;
	time_seconds_from_rdtsc = NULL;
}
