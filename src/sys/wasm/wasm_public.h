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

#ifndef __WASM_PUBLIC_H__
#define __WASM_PUBLIC_H__

#include <stdlib.h>
#include <sys/stat.h>

#include "kas_common.h"
#include "memory.h"
#include "kas_string.h"

/******************** wasm memory utils  ********************/

#	define stack_alloc(bytes)	alloca(bytes)		
#	define INLINE		inline
#	define ALIGN(m)	__attribute__((aligned (m)))	
#	if   _POSIX_C_SOURCE >= 200112L || _XOPEN_SOURCE >= 600 
#		define ALIGN_ALLOC(ptr_addr, size, alignment)	posix_memalign((void **)(ptr_addr),(alignment),(size))
#	endif

/******************** wasm_arch.c ********************/

void os_arch_init_func_ptrs(void);

/******************** wasm_filesystem.c ********************/

typedef struct stat file_status;
typedef int file_handle;

/******************** wasm_error.c ********************/

#include <stdarg.h>
#include <signal.h>
#include "kas_logger.h"

#define kas_static_assert(assertion, str)	_Static_assert(assertion, str)

#ifdef KAS_ASSERT_DEBUG
#define kas_assert(assertion)			_kas_assert(assertion, __FILE__, __LINE__, __func__)
#define kas_assert_string(assertion, cstr)	_kas_assert_string(assertion, __FILE__, __LINE__, __func__, cstr) 
#define kas_assert_message(assertion, msg, ...)	_kas_assert_message(assertion, __FILE__, __LINE__, __func__, msg, __VA_ARGS__)

#define _kas_assert(assertion, file, line, func)								  \
	if (assertion) { }											  \
	else													  \
	{													  \
		LOG_MESSAGE(T_ASSERT, S_FATAL, 0, "assertion failed at %s:%u in function %s\n", file, line, func);\
		raise(SIGTRAP);											  \
	}

#define _kas_assert_string(assertion, file, line, func, cstr)							    		\
	if (assertion) { }			     									        \
	else													  		\
	{													    		\
		LOG_MESSAGE(T_ASSERT, S_FATAL, 0, "assertion failed at %s:%u in function %s - %s\n", file, line, func, cstr); 	\
		raise(SIGTRAP);											    		\
	}

#define _kas_assert_message(assertion, file, line, func, msg, ...)						    		\
	if (assertion) { }												    	\
	else															\
	{													    		\
		u8 __msg_buf[LOG_MAX_MESSAGE_SIZE];									    	\
		const kas_string __fmsg = kas_string_format_buffered(__msg_buf, LOG_MAX_MESSAGE_SIZE, msg, __VA_ARGS__);	\
		LOG_MESSAGE(T_ASSERT, S_FATAL, 0, "assertion failed at %s:%u in function %s - %k\n", file, line, func, &__fmsg);\
		raise(SIGTRAP);											    		\
	}

#else
#define kas_assert(assertion)
#define kas_assert_string(assertion, str)
#define kas_assert_message(assertion, msg, ...)
#define _kas_assert(assertion, file, line, func)
#define _kas_assert_string(assertion, file, line, func, str)
#define _kas_assert_message(assertion, file, line, func, str, ...)
#endif


void init_error_handling_func_ptrs(void);

/********************  wasm_thread.c	********************/

typedef struct mutex
{
	i32 tmp;
} mutex;

typedef struct mutex_condition
{
	i32 tmp;
} mutex_condition;

typedef u32			tid;
typedef struct kas_thread 	kas_thread;

/********************  wasm_sync_primitives.c	********************/

#include <emscripten/wasm_worker.h>
typedef emscripten_semaphore_t semaphore;

#ifdef FORCE_SEQ_CST
#define ATOMIC_RELAXED	__ATOMIC_SEQ_CST
#define ATOMIC_ACQUIRE	__ATOMIC_SEQ_CST
#define ATOMIC_RELEASE	__ATOMIC_SEQ_CST
#define ATOMIC_SEQ_CST	__ATOMIC_SEQ_CST
#else
#define ATOMIC_RELAXED	__ATOMIC_RELAXED
#define ATOMIC_ACQUIRE	__ATOMIC_ACQUIRE
#define ATOMIC_RELEASE	__ATOMIC_RELEASE
#define ATOMIC_SEQ_CST	__ATOMIC_SEQ_CST
#endif

#define atomic_fetch_add_rlx_32(fetch_addr, val)	__atomic_fetch_add(fetch_addr, val, ATOMIC_RELAXED)
#define atomic_fetch_add_acq_32(fetch_addr, val)	__atomic_fetch_add(fetch_addr, val, ATOMIC_ACQUIRE)
#define atomic_fetch_add_rel_32(fetch_addr, val)	__atomic_fetch_add(fetch_addr, val, ATOMIC_RELAXED)
#define atomic_fetch_add_seq_cst_32(fetch_addr, val)	__atomic_fetch_add(fetch_addr, val, ATOMIC_SEQ_CST)

#define atomic_fetch_add_rlx_64(fetch_addr, val)	__atomic_fetch_add(fetch_addr, val, ATOMIC_RELAXED)
#define atomic_fetch_add_acq_64(fetch_addr, val)	__atomic_fetch_add(fetch_addr, val, ATOMIC_ACQUIRE)
#define atomic_fetch_add_rel_64(fetch_addr, val)	__atomic_fetch_add(fetch_addr, val, ATOMIC_RELAXED)
#define atomic_fetch_add_seq_cst_64(fetch_addr, val)	__atomic_fetch_add(fetch_addr, val, ATOMIC_SEQ_CST)

#define atomic_fetch_sub_rlx_32(fetch_addr, val)	__atomic_fetch_sub(fetch_addr, val, ATOMIC_RELAXED)
#define atomic_fetch_sub_acq_32(fetch_addr, val)	__atomic_fetch_sub(fetch_addr, val, ATOMIC_ACQUIRE)
#define atomic_fetch_sub_rel_32(fetch_addr, val)	__atomic_fetch_sub(fetch_addr, val, ATOMIC_RELAXED)
#define atomic_fetch_sub_seq_cst_32(fetch_addr, val)	__atomic_fetch_sub(fetch_addr, val, ATOMIC_SEQ_CST)
                                                                       
#define atomic_fetch_sub_rlx_64(fetch_addr, val)	__atomic_fetch_sub(fetch_addr, val, ATOMIC_RELAXED)
#define atomic_fetch_sub_acq_64(fetch_addr, val)	__atomic_fetch_sub(fetch_addr, val, ATOMIC_ACQUIRE)
#define atomic_fetch_sub_rel_64(fetch_addr, val)	__atomic_fetch_sub(fetch_addr, val, ATOMIC_RELAXED)
#define atomic_fetch_sub_seq_cst_64(fetch_addr, val)	__atomic_fetch_sub(fetch_addr, val, ATOMIC_SEQ_CST)

#define atomic_compare_exchange_rlx_32(dst_addr, cmp_addr, exch_val)		__atomic_compare_exchange_n(dst_addr, cmp_addr, exch_val, 0, ATOMIC_RELAXED, ATOMIC_RELAXED)
#define atomic_compare_exchange_acq_32(dst_addr, cmp_addr, exch_val)		__atomic_compare_exchange_n(dst_addr, cmp_addr, exch_val, 0, ATOMIC_ACQUIRE, ATOMIC_RELAXED)
#define atomic_compare_exchange_rel_32(dst_addr, cmp_addr, exch_val)		__atomic_compare_exchange_n(dst_addr, cmp_addr, exch_val, 0, ATOMIC_RELEASE, ATOMIC_RELAXED)
#define atomic_compare_exchange_seq_cst_32(dst_addr, cmp_addr, exch_val)	__atomic_compare_exchange_n(dst_addr, cmp_addr, exch_val, 0, ATOMIC_SEQ_CST, ATOMIC_RELAXED)

#define atomic_compare_exchange_rlx_64(dst_addr, cmp_addr, exch_val)		__atomic_compare_exchange_n(dst_addr, cmp_addr, exch_val, 0, ATOMIC_RELAXED, ATOMIC_RELAXED)
#define atomic_compare_exchange_acq_64(dst_addr, cmp_addr, exch_val)		__atomic_compare_exchange_n(dst_addr, cmp_addr, exch_val, 0, ATOMIC_ACQUIRE, ATOMIC_RELAXED)
#define atomic_compare_exchange_rel_64(dst_addr, cmp_addr, exch_val)		__atomic_compare_exchange_n(dst_addr, cmp_addr, exch_val, 0, ATOMIC_RELEASE, ATOMIC_RELAXED)
#define atomic_compare_exchange_seq_cst_64(dst_addr, cmp_addr, exch_val)	__atomic_compare_exchange_n(dst_addr, cmp_addr, exch_val, 0, ATOMIC_SEQ_CST, ATOMIC_RELAXED)

#define atomic_store_rlx_32(dst_addr, val)		__atomic_store_n(dst_addr, val, ATOMIC_RELAXED)
#define atomic_store_rel_32(dst_addr, val)		__atomic_store_n(dst_addr, val, ATOMIC_RELEASE)
#define atomic_store_seq_cst_32(dst_addr, val)		__atomic_store_n(dst_addr, val, ATOMIC_SEQ_CST)
               
#define atomic_store_rlx_64(dst_addr, val)		__atomic_store_n(dst_addr, val, ATOMIC_RELAXED)
#define atomic_store_rel_64(dst_addr, val)		__atomic_store_n(dst_addr, val, ATOMIC_RELEASE)
#define atomic_store_seq_cst_64(dst_addr, val)		__atomic_store_n(dst_addr, val, ATOMIC_SEQ_CST)

#define atomic_add_fetch_rlx_32(fetch_addr, val)	__atomic_add_fetch(fetch_addr, val, ATOMIC_RELAXED) 
#define atomic_add_fetch_acq_32(fetch_addr, val)	__atomic_add_fetch(fetch_addr, val, ATOMIC_ACQUIRE) 
#define atomic_add_fetch_rel_32(fetch_addr, val)	__atomic_add_fetch(fetch_addr, val, ATOMIC_RELEASE) 
#define atomic_add_fetch_seq_cst_32(fetch_addr, val)	__atomic_add_fetch(fetch_addr, val, ATOMIC_SEQ_CST)
      
#define atomic_add_fetch_rlx_64(fetch_addr, val)	__atomic_add_fetch(fetch_addr, val, ATOMIC_RELAXED) 
#define atomic_add_fetch_acq_64(fetch_addr, val)	__atomic_add_fetch(fetch_addr, val, ATOMIC_ACQUIRE) 
#define atomic_add_fetch_rel_64(fetch_addr, val)	__atomic_add_fetch(fetch_addr, val, ATOMIC_RELEASE) 
#define atomic_add_fetch_seq_cst_64(fetch_addr, val)	__atomic_add_fetch(fetch_addr, val, ATOMIC_SEQ_CST)
     
#define atomic_sub_fetch_rlx_32(fetch_addr, val)	__atomic_sub_fetch(fetch_addr, val, ATOMIC_RELAXED) 
#define atomic_sub_fetch_acq_32(fetch_addr, val)	__atomic_sub_fetch(fetch_addr, val, ATOMIC_ACQUIRE) 
#define atomic_sub_fetch_rel_32(fetch_addr, val)	__atomic_sub_fetch(fetch_addr, val, ATOMIC_RELEASE) 
#define atomic_sub_fetch_seq_cst_32(fetch_addr, val)	__atomic_sub_fetch(fetch_addr, val, ATOMIC_SEQ_CST)
    
#define atomic_sub_fetch_rlx_64(fetch_addr, val)	__atomic_sub_fetch(fetch_addr, val, ATOMIC_RELAXED) 
#define atomic_sub_fetch_acq_64(fetch_addr, val)	__atomic_sub_fetch(fetch_addr, val, ATOMIC_ACQUIRE) 
#define atomic_sub_fetch_rel_64(fetch_addr, val)	__atomic_sub_fetch(fetch_addr, val, ATOMIC_RELEASE) 
#define atomic_sub_fetch_seq_cst_64(fetch_addr, val)	__atomic_sub_fetch(fetch_addr, val, ATOMIC_SEQ_CST)

#define atomic_load_rlx_32(fetch_addr)			__atomic_load_n(fetch_addr, ATOMIC_RELAXED)
#define atomic_load_acq_32(fetch_addr)			__atomic_load_n(fetch_addr, ATOMIC_ACQUIRE)
#define atomic_load_seq_cst_32(fetch_addr)		__atomic_load_n(fetch_addr, ATOMIC_SEQ_CST)

#define atomic_load_rlx_64(fetch_addr)			__atomic_load_n(fetch_addr, ATOMIC_RELAXED)
#define atomic_load_acq_64(fetch_addr)			__atomic_load_n(fetch_addr, ATOMIC_ACQUIRE)
#define atomic_load_seq_cst_64(fetch_addr)		__atomic_load_n(fetch_addr, ATOMIC_SEQ_CST)

#define atomic_load_to_addr_rlx_32(fetch_addr, dst_addr)	__atomic_load(fetch_addr, dst_addr, ATOMIC_RELAXED)
#define atomic_load_to_addr_acq_32(fetch_addr, dst_addr)	__atomic_load(fetch_addr, dst_addr, ATOMIC_ACQUIRE)
#define atomic_load_to_addr_seq_cst_32(fetch_addr, dst_addr)	__atomic_load(fetch_addr, dst_addr, ATOMIC_SEQ_CST)

#define atomic_load_to_addr_rlx_64(fetch_addr, dst_addr)	__atomic_load(fetch_addr, dst_addr, ATOMIC_RELAXED)
#define atomic_load_to_addr_acq_64(fetch_addr, dst_addr)	__atomic_load(fetch_addr, dst_addr, ATOMIC_ACQUIRE)
#define atomic_load_to_addr_seq_cst_64(fetch_addr, dst_addr)	__atomic_load(fetch_addr, dst_addr, ATOMIC_SEQ_CST)

#define atomic_store_from_addr_rlx_32(dst_addr, src_addr)	__atomic_store(dst_addr, src_addr, ATOMIC_RELAXED)
#define atomic_store_from_addr_rel_32(dst_addr, src_addr)	__atomic_store(dst_addr, src_addr, ATOMIC_ACQUIRE)
#define atomic_store_from_addr_seq_cst_32(dst_addr, src_addr)	__atomic_store(dst_addr, src_addr, ATOMIC_SEQ_CST)
                                                                         
#define atomic_store_from_addr_rlx_64(dst_addr, src_addr)	__atomic_store(dst_addr, src_addr, ATOMIC_RELAXED)
#define atomic_store_from_addr_rel_64(dst_addr, src_addr)	__atomic_store(dst_addr, src_addr, ATOMIC_ACQUIRE)
#define atomic_store_from_addr_seq_cst_64(dst_addr, src_addr)	__atomic_store(dst_addr, src_addr, ATOMIC_SEQ_CST)

#endif
