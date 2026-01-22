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

#ifndef __TEST_LOCAL_H__
#define __TEST_LOCAL_H__

#include "test_public.h"

#include "kas_common.h"
#include "kas_random.h"
#include "sys_public.h"
#include "memory.h"

/********************************** Performance Testing ************************************/

enum repetition_tester_type
{
	RT_TEST_PERFORMANCE,		/* single threaded perfomance 		*/
	RT_TEST_PARALLEL_PERFORMANCE,	/* multithreaded threaded perfomance 	*/
	RT_TEST_COUNT
};

enum repetition_tester_state
{
	RT_UNINITIALIZED = 0,
	RT_TESTING,
	RT_COMPLETED,
	RT_ERROR,
};

struct repetition_tester
{
	u64 time;
	u64 bytes;
	u64 tsc_in_current_test;
	u64 bytes_in_current_test;
	enum repetition_tester_state state;
	u32 enter_count;
	u32 exit_count;
	u32 print : 1;

	u64 bytes_to_process;
	u64 tsc_retry_max;	/* maximum tsc since last new best iteration before we end the test */
	u64 tsc_freq;
	u64 tsc_start;

	u64 test_count;
	u64 total_time;
	u64 tsc_iteration_max;
	u64 tsc_iteration_min;

	u64 cycles_in_current_test;
	u64 cycles_min_time;
	u64 cycles_max_time;
	u64 cycles;
	
	u64 page_faults_in_current_test;
	u64 page_faults_min_time;
	u64 page_faults_max_time; /* page fault count on max count */
	u64 page_faults;

	u64 branch_misses_in_current_test;
	u64 branch_misses_min_time;
	u64 branch_misses_max_time;
	u64 branch_misses;

	u64 frontend_stalled_cycles_in_current_test;
	u64 frontend_stalled_cycles_min_time;
	u64 frontend_stalled_cycles_max_time;
	u64 frontend_stalled_cycles;

	u64 backend_stalled_cycles_in_current_test;
	u64 backend_stalled_cycles_min_time;
	u64 backend_stalled_cycles_max_time;
	u64 backend_stalled_cycles;
	
#ifdef __linux__

#define NUM_EVENTS 5
#define PAGE_FAULT_SAMPLING_PERIOD 1		/* counter period for new update */
#define BRANCH_MISSES_SAMPLING_PERIOD 1000
	u64 event_count;
	u64 pf_id; /* page faults, event group leader */
	u64 bm_id; /* branch_misses */
	u64 fnt_id; /* frontend stall cycles */
	u64 bck_id; /* backend stall cycles */
	u64 cyc_id;  /* non-cpu-frequency-scaled cycles */
	i32 pf_fd; 
	i32 bm_fd;  
	i32 fnt_fd;
	i32 bck_fd;
	i32 cyc_fd;
#endif
};

i32 rt_is_testing(struct repetition_tester *tester);
void rt_wave(struct repetition_tester *tester, const u64 bytes_to_process, const u64 tsc_freq, const u64 tsc_retry_max, const u32 print);
void rt_begin_time(struct repetition_tester *tester);
void rt_end_time(struct repetition_tester *tester);
void rt_print_statistics(const struct repetition_tester *tester, FILE *file);


extern struct performance_suite *hash_performance_suite;
extern struct performance_suite *rng_performance_suite;
extern struct performance_suite *serialize_performance_suite;
extern struct performance_suite *allocator_performance_suite;

struct serial_test
{
	const char *		id;
	const u64		size;
	void *			(*test_init)(void);
	void 			(*test_reset)(void *);
	void 			(*test_free)(void *);
	void 			(*test)(void *);
};

struct parallel_test
{
	const char *	id;
	const u64	size;
	void *		(*test_init)(void);
	void 		(*test_reset)(void *);
	void 		(*test_free)(void *);
	TASK		test;
};

struct performance_test_caller_input
{
	u32 *	a_barrier;
	void *	args;
	TASK	test;
};

struct performance_suite
{
	const char *			id;
	const struct serial_test	*serial_test;
	const const u32 		serial_test_count;
	const struct parallel_test	*parallel_test;
	const const u32 		parallel_test_count;
};


/********************************** Correctness Testing  ************************************/


extern struct suite *array_list_suite;
extern struct suite *hierarchy_index_suite;
extern struct suite *math_suite;
extern struct suite *kas_string_suite;
extern struct suite *serialize_suite;

struct test_output
{
	const char *id;
	const char *file;
	u64 line;
	u64 success;
};

struct test_environment
{
	struct arena *mem_1;
	struct arena *mem_2;
	struct arena *mem_3;
	struct arena *mem_4;
	struct arena *mem_5;
	struct arena *mem_6;
	const u64 seed;
};

struct repetition_test
{
	struct test_output (*test)(void);
	const u32 count;
};

struct suite
{
	char *id;
	struct test_output (**unit_test)(struct test_environment *);
	const u64 unit_test_count;
	const struct repetition_test *repetition_test;
	const u64 repetition_test_count;
};

#ifdef KAS_DEBUG
#define TEST_FAILURE { output.success = 0; output.file = __FILE__; output.line = __LINE__; Breakpoint(1); return output; }
#else
#define TEST_FAILURE { output.success = 0; output.file = __FILE__; output.line = __LINE__; return output; }
#endif 
#define PRINT_FAILURE(t1, t2, a, b, print) { fprintf(stderr, t1); print(stderr, a); fprintf(stderr, t2); print(stderr, b); }
#define PRINT_SINGLE(t, a, print) { fprintf(stderr, t); print(a); }

#define TEST_EQUAL(exp, act) if ((exp) != (act)) { TEST_FAILURE } else { output.success = 1; }
#define TEST_NOT_EQUAL(exp, act) if ((exp) == (act)) { TEST_FAILURE } else { output.success = 1; }
#define TEST_EQUAL_PRINT(exp, act, print) if ((exp) != (act)) { PRINT_FAILURE("EXPECTED:\t", "ACTUAL:\t", (exp), (act), (print)) TEST_FAILURE } else { output.success = 1; }
#define TEST_NOT_EQUAL_PRINT(exp, act, print) if ((exp) == (act)) { PRINT_FAILURE("NOT EXPECTED\t", "ACTUAL:\t", (exp), (act), (print)) TEST_FAILURE } else { output.success = 1; }

#define TEST_ZERO(exp) if (exp) { TEST_FAILURE } else { output.success = 1; }
#define TEST_NOT_ZERO(exp) if (!(exp)) { TEST_FAILURE } else { output.success = 1; }
#define TEST_ZERO_PRINT(exp, print) if (exp) { PRINT_SINGLE("EXPECTED ZERO:\t", exp, print) TEST_FAILURE } else { output.success = 1; }
#define TEST_NOT_ZERO_PRINT(exp, print) if (!(exp)) { PRINT_SINGLE("EXPECTED NOT ZERO:\t", exp, print) TEST_FAILURE } else { output.success = 1; }

#define TEST_FALSE(exp) if (exp) { TEST_FAILURE } else { output.success = 1; }
#define TEST_TRUE(exp) if (!(exp)) { TEST_FAILURE } else { output.success = 1; }

#endif
