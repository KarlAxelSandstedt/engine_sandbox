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

#ifndef __DREAMSCAPE_TIMER_H__
#define __DREAMSCAPE_TIMER_H__

#ifdef __cplusplus
extern "C" { 
#endif

/*
ds_Time
=======
The ds_Time** API returns time elapsed since the API's initialization. There is no guarantee that the timings
are synchronized between threads, so you should probably only call ds_Time* on the main thread, unless you are
constructing some timing durations locally on a given thread. 
*/

#if (__DS_PLATFORM__ == __DS_WEB__)

#elif (__DS_COMPILER__ == __DS_GCC__ || __DS_COMPILER__ == __DS_CLANG__)

	#include <x86intrin.h>
	#define	Rdtsc()			__rdtsc()
	/* RDTSC + Read OS dependent IA32_TSC_AUX. All previous instructions finsish (RW finish?) before Rdtscp is run. */
	#define	Rdtscp(core_addr)	__rdtscp(core_addr)

#elif (__DS_COMPILER__ == __DS_MSVC__)

	#include <intrin.h>
	#define	Rdtsc()			__rdtsc()
	/* RDTSC + Read OS dependent IA32_TSC_AUX. All previous instructions finsish (RW finish?) before Rdtscp is run. */
	#define	Rdtscp(core_addr)	__rdtscp(core_addr)

#endif

/* Initialize API */
void	ds_TimeApiInit(struct arena *persistent);
/* Return ns timing at the API's initialization */
u64	ds_TimeNsAtStart(void);				
/* Return seconds since initialization */
u64	ds_TimeS(void);					
/* Return ms since initialization */
u64	ds_TimeMs(void); 					
/* Return us since initialization */
u64	ds_TimeUs(void); 					
/* Return ns since initialization */
u64	ds_TimeNs(void); 					
/* Return ns timer resolution in ns */
u64	NsResolution(void);				

/* Return ns since initialization given cpu tsc value */
u64 	ds_TimeNsFromTsc(const u64 tsc);		
/* Return tsc value since initialization given PLATFORM ns value (not ds_TimeNs()) */
u64	ds_TimeTscFromNs(const u64 ns);			
/* transform tsc to corresponding ns */
u64 	NsFromTsc(const u64 tsc);			
/* transform ns to corresponding tsc */
u64	TscFromNs(const u64 ns);			
/* transform tsc to ns, with an additional truth pair (ns, tsc) in order to reduce error */
u64	NsFromTscTruthSource(const u64 tsc, const u64 ns_truth, const u64 cc_truth); 
/* transform ns to tsc, with an additional truth pair (ns, tsc) in order to reduce error */
u64	TscFromNsTruthSource(const u64 ns, const u64 ns_truth, const u64 cc_truth);  
/* Return estimated tsc ticks per second */
u64 	TscFrequency(void);				
/* Return estimated seconds given the tick count */
f64 	SFromTsc(const u64 ticks);			

/* g_tsc_skew[Logical_core_count]: estimated skew from core 0.
 * given a tsc value from core c, its corresponding tsc value on core 0 is t_0 = t_c + skew,
 * so in code we get 
 * 			tsc_c = Rdtscp(&core_c)
 * 			tsc_0 = core_c + g_tsc_skew[c];
 */
extern u64 *	g_tsc_skew;

#ifdef __cplusplus
} 
#endif

#endif
