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

#ifndef __WIN_PUBLIC_H__
#define __WIN_PUBLIC_H__

#include <windows.h>
#include "kas_common.h"

/************************* windows memory utils *************************/

#define INLINE		__forceinline
#define ALIGN(m)	__declspec(align(m))

#define	memory_alloc_aligned(ptr_addr, size, alignment) (*(ptr_addr) = _aligned_malloc((size),(alignment)))

/************************* win_error.c *************************/

#include <intrin.h>
#include <stdarg.h>
#include "log.h"

#ifdef _M_X64
#define breakpoint(condition) if (!(condition)) { } else { __debugbreak(); }
#endif 

#ifdef KAS_ASSERT_DEBUG
#define kas_assert(assertion)			_kas_assert(assertion, __FILE__, __LINE__, __func__)
#define kas_assert_string(assertion, cstr)	_kas_assert_string(assertion, __FILE__, __LINE__, __func__, cstr) 
#define kas_assert_message(assertion, msg, ...)	_kas_assert_message(assertion, __FILE__, __LINE__, __func__, msg, __VA_ARGS__)
#define kas_static_assert(assertion, str)	static_assert(assertion, str)

#define _kas_assert(assertion, file, line, func)							\
	if (assertion) { }										\
	else												\
	{												\
		log(T_ASSERT, S_FATAL, "assertion failed at %s:%u in function %s", file, line, func);	\
		__debugbreak();										\
 		fatal_cleanup_and_exit(kas_thread_self_tid());						\
	}

#define _kas_assert_string(assertion, file, line, func, cstr)							\
	if (assertion) { }			     								\
	else													\
	{													\
		log(T_ASSERT, S_FATAL, "assertion failed at %s:%u in function %s - %s", file, line, func, cstr);\
		__debugbreak();											\
 		fatal_cleanup_and_exit(kas_thread_self_tid());						\
	}

#define _kas_assert_message(assertion, file, line, func, msg, ...)						    \
	if (assertion) { }											    \
	else													    \
	{													    \
		u8 __msg_buf[LOG_MAX_MESSAGE_SIZE];								    \
		const utf8 __fmsg = utf8_format_buffered(__msg_buf, LOG_MAX_MESSAGE_SIZE, msg, __VA_ARGS__);	    \
		log(T_ASSERT, S_FATAL, "assertion failed at %s:%u in function %s - %k", file, line, func, &__fmsg); \
		__debugbreak();											    \
 		fatal_cleanup_and_exit(kas_thread_self_tid());						\
	}
#else
#define kas_static_assert(assertion, str)
#define kas_assert(assertion)
#define kas_assert_string(assertion, str)
#define kas_assert_message(assertion, msg, ...)
#define _kas_assert(assertion, file, line, func)
#define _kas_assert_string(assertion, file, line, func, str)
#define _kas_assert_message(assertion, file, line, func, str, ...)
#endif

#define ERROR_BUFSIZE	512				
#define log_system_error(severity)	_log_system_error(severity, __func__, __FILE__, __LINE__)
#define _log_system_error(severity, func, file, line)						\
{												\
	u8 _err_buf[ERROR_BUFSIZE];								\
	const utf8 _err_str = utf8_system_error_buffered(_err_buf, ERROR_BUFSIZE);		\
	log(T_SYSTEM, severity, "At %s:%u in function %s - %k\n", file, line, func, &_err_str);	\
}

/* thread safe last system error message generation */
utf8	utf8_system_error_buffered(u8 *buf, const u32 bufsize);


void 	init_error_handling_func_ptrs(void);

/************************* win_filesystem.c *************************/

typedef WIN32_FILE_ATTRIBUTE_DATA	file_status;
typedef HANDLE 				file_handle;

#define FILE_HANDLE_INVALID	INVALID_HANDLE_VALUE

#define FS_PROT_READ         	FILE_MAP_READ
#define FS_PROT_WRITE		FILE_MAP_WRITE
#define FS_PROT_EXECUTE      	FILE_MAP_EXECUTE
#define FS_PROT_NONE         	0

#define FS_MAP_SHARED        	0
#define FS_MAP_PRIVATE       	0

void 	filesystem_init_func_ptrs(void);

/********************  win_thread.c	********************/

typedef DWORD				tid;
typedef struct kas_thread		kas_thread;

/************************* win_sync_primitives.c *************************/

#include <intrin.h>
#include <immintrin.h>
typedef HANDLE semaphore;

#ifdef FORCE_SEQ_CST
	/* returns signed integers */
	#define atomic_fetch_add_rlx_32(fetch_addr, val)	_InterlockedExchangeAdd((long volatile *) (fetch_addr), (long) (val))
	#define atomic_fetch_add_acq_32(fetch_addr, val)	_InterlockedExchangeAdd((long volatile *) (fetch_addr), (long) (val))
	#define atomic_fetch_add_rel_32(fetch_addr, val)	_InterlockedExchangeAdd((long volatile *) (fetch_addr), (long) (val))
	#define atomic_fetch_add_seq_cst_32(fetch_addr, val)	_InterlockedExchangeAdd((long volatile *) (fetch_addr), (long) (val))
	
	#define atomic_fetch_add_rlx_64(fetch_addr, val)	_InterlockedExchangeAdd64((__int64 volatile *) (fetch_addr), (__int64) (val))
	#define atomic_fetch_add_acq_64(fetch_addr, val)	_InterlockedExchangeAdd64((__int64 volatile *) (fetch_addr), (__int64) (val))
	#define atomic_fetch_add_rel_64(fetch_addr, val)	_InterlockedExchangeAdd64((__int64 volatile *) (fetch_addr), (__int64) (val))
	#define atomic_fetch_add_seq_cst_64(fetch_addr, val)	_InterlockedExchangeAdd64((__int64 volatile *) (fetch_addr), (__int64) (val))

	#define atomic_fetch_sub_rlx_32(fetch_addr, val)	_InterlockedExchangeAdd((long volatile *) (fetch_addr), -(long) (val))
	#define atomic_fetch_sub_acq_32(fetch_addr, val)	_InterlockedExchangeAdd((long volatile *) (fetch_addr), -(long) (val))
	#define atomic_fetch_sub_rel_32(fetch_addr, val)	_InterlockedExchangeAdd((long volatile *) (fetch_addr), -(long) (val))
	#define atomic_fetch_sub_seq_cst_32(fetch_addr, val)	_InterlockedExchangeAdd((long volatile *) (fetch_addr), -(long) (val))
	
	#define atomic_fetch_sub_rlx_64(fetch_addr, val)	_InterlockedExchangeAdd64((__int64 volatile *) (fetch_addr), -(__int64) (val))
	#define atomic_fetch_sub_acq_64(fetch_addr, val)	_InterlockedExchangeAdd64((__int64 volatile *) (fetch_addr), -(__int64) (val))
	#define atomic_fetch_sub_rel_64(fetch_addr, val)	_InterlockedExchangeAdd64((__int64 volatile *) (fetch_addr), -(__int64) (val))
	#define atomic_fetch_sub_seq_cst_64(fetch_addr, val)	_InterlockedExchangeAdd64((__int64 volatile *) (fetch_addr), -(__int64) (val))

	/* if (dst == cmp) set dst to exch_val, return true if exchanged, otherwise false */
	#define atomic_compare_exchange_rlx_32(dst_addr, cmp_addr, exch_val)		(*(cmp_addr) == _InterlockedCompareExchange((long volatile *) (dst_addr), (long) (exch_val), (long) (*cmp_addr)))
	#define atomic_compare_exchange_acq_32(dst_addr, cmp_addr, exch_val)		(*(cmp_addr) == _InterlockedCompareExchange((long volatile *) (dst_addr), (long) (exch_val), (long) (*cmp_addr)))
	#define atomic_compare_exchange_rel_32(dst_addr, cmp_addr, exch_val)		(*(cmp_addr) == _InterlockedCompareExchange((long volatile *) (dst_addr), (long) (exch_val), (long) (*cmp_addr)))
	#define atomic_compare_exchange_seq_cst_32(dst_addr, exch_val, cmp_val)		(*(cmp_addr) == _InterlockedCompareExchange((long volatile *) (dst_addr), (long) (exch_val), (long) (*cmp_addr)))

	#define atomic_compare_exchange_rlx_64(dst_addr, cmp_addr, exch_val)		(*(cmp_addr) == _InterlockedCompareExchange64((__int64 volatile *) (dst_addr), (__int64) (exch_val), (__int64) (*cmp_addr)))
	#define atomic_compare_exchange_acq_64(dst_addr, cmp_addr, exch_val)		(*(cmp_addr) == _InterlockedCompareExchange64((__int64 volatile *) (dst_addr), (__int64) (exch_val), (__int64) (*cmp_addr)))
	#define atomic_compare_exchange_rel_64(dst_addr, cmp_addr, exch_val)		(*(cmp_addr) == _InterlockedCompareExchange64((__int64 volatile *) (dst_addr), (__int64) (exch_val), (__int64) (*cmp_addr)))
	#define atomic_compare_exchange_seq_cst_64(dst_addr, exch_val, cmp_val)		(*(cmp_addr) == _InterlockedCompareExchange64((__int64 volatile *) (dst_addr), (__int64) (exch_val), (__int64) (*cmp_addr)))

	#define atomic_store_rlx_32(dst_addr, val)		_InterlockedExchange((long volatile *) (dst_addr), long (val))
	#define atomic_store_rel_32(dst_addr, val)		_InterlockedExchange((long volatile *) (dst_addr), long (val))
	#define atomic_store_seq_cst_32(dst_addr, val)		_InterlockedExchange((long volatile *) (dst_addr), long (val))
	               
	#define atomic_store_rlx_64(dst_addr, val)		_InterlockedExchange64_HLERelease((__int64 volatile *) (dst_addr), (__int64) (val))
	#define atomic_store_rel_64(dst_addr, val)		_InterlockedExchange64_HLERelease((__int64 volatile *) (dst_addr), (__int64) (val))
	#define atomic_store_seq_cst_64(dst_addr, val)		_InterlockedExchange64((__int64 volatile *) (dst_addr), (__int64) (val))

#else
	/* returns signed integers  */
	#define atomic_fetch_add_rlx_32(fetch_addr, val)	_InterlockedExchangeAdd_HLEAcquire((long volatile *) (fetch_addr), (long) (val))
	#define atomic_fetch_add_acq_32(fetch_addr, val)	_InterlockedExchangeAdd_HLEAcquire((long volatile *) (fetch_addr), (long) (val))
	#define atomic_fetch_add_rel_32(fetch_addr, val)	_InterlockedExchangeAdd_HLERelease((long volatile *) (fetch_addr), (long) (val))
	#define atomic_fetch_add_seq_cst_32(fetch_addr, val)	_InterlockedExchangeAdd((long volatile *) (fetch_addr), (long) (val))
	
	#define atomic_fetch_add_rlx_64(fetch_addr, val)	_InterlockedExchangeAdd64_HLEAcquire((__int64 volatile *) (fetch_addr), (__int64) (val))
	#define atomic_fetch_add_acq_64(fetch_addr, val)	_InterlockedExchangeAdd64_HLEAcquire((__int64 volatile *) (fetch_addr), (__int64) (val))
	#define atomic_fetch_add_rel_64(fetch_addr, val)	_InterlockedExchangeAdd64_HLERelease((__int64 volatile *) (fetch_addr), (__int64) (val))
	#define atomic_fetch_add_seq_cst_64(fetch_addr, val)	_InterlockedExchangeAdd64((__int64 volatile *) (fetch_addr), (__int64) (val))

	#define atomic_fetch_sub_rlx_32(fetch_addr, val)	_InterlockedExchangeAdd_HLEAcquire((long volatile *) (fetch_addr), -(long) (val))
	#define atomic_fetch_sub_acq_32(fetch_addr, val)	_InterlockedExchangeAdd_HLEAcquire((long volatile *) (fetch_addr), -(long) (val))
	#define atomic_fetch_sub_rel_32(fetch_addr, val)	_InterlockedExchangeAdd_HLERelease((long volatile *) (fetch_addr), -(long) (val))
	#define atomic_fetch_sub_seq_cst_32(fetch_addr, val)	_InterlockedExchangeAdd((long volatile *) (fetch_addr), -(long) (val))
	
	#define atomic_fetch_sub_rlx_64(fetch_addr, val)	_InterlockedExchangeAdd64_HLEAcquire((__int64 volatile *) (fetch_addr), -(__int64) (val))
	#define atomic_fetch_sub_acq_64(fetch_addr, val)	_InterlockedExchangeAdd64_HLEAcquire((__int64 volatile *) (fetch_addr), -(__int64) (val))
	#define atomic_fetch_sub_rel_64(fetch_addr, val)	_InterlockedExchangeAdd64_HLERelease((__int64 volatile *) (fetch_addr), -(__int64) (val))
	#define atomic_fetch_sub_seq_cst_64(fetch_addr, val)	_InterlockedExchangeAdd64((__int64 volatile *) (fetch_addr), -(__int64) (val))
	
	/* if (dst == cmp) set dst to exch_val */
	#define atomic_compare_exchange_rlx_32(dst_addr, cmp_addr, exch_val)		(*(cmp_addr) == _InterlockedCompareExchange_HLEAcquire((long volatile *) (dst_addr), (long) (exch_val), (long) (*cmp_addr)))
	#define atomic_compare_exchange_acq_32(dst_addr, cmp_addr, exch_val)		(*(cmp_addr) == _InterlockedCompareExchange_HLEAcquire((long volatile *) (dst_addr), (long) (exch_val), (long) (*cmp_addr)))
	#define atomic_compare_exchange_rel_32(dst_addr, cmp_addr, exch_val)		(*(cmp_addr) == _InterlockedCompareExchange_HLERelease((long volatile *) (dst_addr), (long) (exch_val), (long) (*cmp_addr)))
	#define atomic_compare_exchange_seq_cst_32(dst_addr, cmp_addr, exch_val)	(*(cmp_addr) == _InterlockedCompareExchange((long volatile *) (dst_addr), (long) (exch_val), (long) (*cmp_addr)))
                                                                                                        
	#define atomic_compare_exchange_rlx_64(dst_addr, cmp_addr, exch_val)		(*(cmp_addr) == _InterlockedCompareExchange64_HLEAcquire((__int64 volatile *) (dst_addr), (__int64) (exch_val), (__int64) (*cmp_addr)))
	#define atomic_compare_exchange_acq_64(dst_addr, cmp_addr, exch_val)		(*(cmp_addr) == _InterlockedCompareExchange64_HLEAcquire((__int64 volatile *) (dst_addr), (__int64) (exch_val), (__int64) (*cmp_addr)))
	#define atomic_compare_exchange_rel_64(dst_addr, cmp_addr, exch_val)		(*(cmp_addr) == _InterlockedCompareExchange64_HLERelease((__int64 volatile *) (dst_addr), (__int64) (exch_val), (__int64) (*cmp_addr)))
	#define atomic_compare_exchange_seq_cst_64(dst_addr, cmp_addr, exch_val)	(*(cmp_addr) == _InterlockedCompareExchange64((__int64 volatile *) (dst_addr), (__int64) (exch_val), (__int64) (*cmp_addr)))

	#define atomic_store_rlx_32(dst_addr, val)		_InterlockedExchange_HLERelease((long volatile *) (dst_addr), (long) (val))
	#define atomic_store_rel_32(dst_addr, val)		_InterlockedExchange_HLERelease((long volatile *) (dst_addr), (long) (val))
	#define atomic_store_seq_cst_32(dst_addr, val)		_InterlockedExchange(dst_addr, val)
	               
	#define atomic_store_rlx_64(dst_addr, val)		_InterlockedExchange64_HLERelease((__int64 volatile *) (dst_addr), (__int64) (val))
	#define atomic_store_rel_64(dst_addr, val)		_InterlockedExchange64_HLERelease((__int64 volatile *) (dst_addr), (__int64) (val))
	#define atomic_store_seq_cst_64(dst_addr, val)		_InterlockedExchange64((__int64 volatile *) (dst_addr), (__int64) (val))
#endif                                             

#define atomic_add_fetch_rlx_32(fetch_addr, val)	(atomic_fetch_add_rlx_32(fetch_addr, val) + ((long) val))
#define atomic_add_fetch_acq_32(fetch_addr, val)	(atomic_fetch_add_acq_32(fetch_addr, val) + ((long) val))
#define atomic_add_fetch_rel_32(fetch_addr, val)	(atomic_fetch_add_rel_32(fetch_addr, val) + ((long) val))
#define atomic_add_fetch_seq_cst_32(fetch_addr, val)	(atomic_fetch_add_seq_cst_32(fetch_addr, val) + ((long) val))
                                                                                                         
#define atomic_add_fetch_rlx_64(fetch_addr, val)	(atomic_fetch_add_rlx_64(fetch_addr, val) + ((__int64) val))
#define atomic_add_fetch_acq_64(fetch_addr, val)	(atomic_fetch_add_acq_64(fetch_addr, val) + ((__int64) val))
#define atomic_add_fetch_rel_64(fetch_addr, val)	(atomic_fetch_add_rel_64(fetch_addr, val) + ((__int64) val))
#define atomic_add_fetch_seq_cst_64(fetch_addr, val)	(atomic_fetch_add_seq_cst_64(fetch_addr, val) + ((__int64) val))

#define atomic_sub_fetch_rlx_32(fetch_addr, val)	(atomic_fetch_sub_rlx_32(fetch_addr, val) - ((long) val))
#define atomic_sub_fetch_acq_32(fetch_addr, val)	(atomic_fetch_sub_acq_32(fetch_addr, val) - ((long) val))
#define atomic_sub_fetch_rel_32(fetch_addr, val)	(atomic_fetch_sub_rel_32(fetch_addr, val) - ((long) val))
#define atomic_sub_fetch_seq_cst_32(fetch_addr, val)	(atomic_fetch_sub_seq_cst_32(fetch_addr, val) - ((long) val))
                                                                                                      
#define atomic_sub_fetch_rlx_64(fetch_addr, val)	(atomic_fetch_sub_rlx_64(fetch_addr, val) - ((__int64) val))
#define atomic_sub_fetch_acq_64(fetch_addr, val)	(atomic_fetch_sub_acq_64(fetch_addr, val) - ((__int64) val))
#define atomic_sub_fetch_rel_64(fetch_addr, val)	(atomic_fetch_sub_rel_64(fetch_addr, val) - ((__int64) val) )
#define atomic_sub_fetch_seq_cst_64(fetch_addr, val)	(atomic_fetch_sub_seq_cst_64(fetch_addr, val) - ((__int64) val))

#define atomic_load_rlx_32(fetch_addr)			atomic_add_fetch_rlx_32(fetch_addr, 0)
#define atomic_load_acq_32(fetch_addr)			atomic_add_fetch_acq_32(fetch_addr, 0)
#define atomic_load_seq_cst_32(fetch_addr)		atomic_add_fetch_seq_cst_32(fetch_addr, 0)

#define atomic_load_rlx_64(fetch_addr)			atomic_add_fetch_rlx_64(fetch_addr, 0)
#define atomic_load_acq_64(fetch_addr)			atomic_add_fetch_acq_64(fetch_addr, 0)
#define atomic_load_seq_cst_64(fetch_addr)		atomic_add_fetch_seq_cst_64(fetch_addr, 0)

#define atomic_load_to_addr_rlx_32(fetch_addr, dst_addr)	(*(dst_addr) = atomic_add_fetch_rlx_32(fetch_addr, 0))
#define atomic_load_to_addr_acq_32(fetch_addr, dst_addr)	(*(dst_addr) = atomic_add_fetch_acq_32(fetch_addr, 0))
#define atomic_load_to_addr_seq_cst_32(fetch_addr, dst_addr)	(*(dst_addr) = atomic_add_fetch_seq_cst_32(fetch_addr, 0))

#define atomic_load_to_addr_rlx_64(fetch_addr, dst_addr)	(*(dst_addr) = atomic_add_fetch_rlx_64(fetch_addr, 0))
#define atomic_load_to_addr_acq_64(fetch_addr, dst_addr)	(*(dst_addr) = atomic_add_fetch_acq_64(fetch_addr, 0))
#define atomic_load_to_addr_seq_cst_64(fetch_addr, dst_addr)	(*(dst_addr) = atomic_add_fetch_seq_cst_64(fetch_addr, 0))

#define atomic_store_from_addr_rlx_32(dst_addr, src_addr)	atomic_store_rlx_32(dst_addr, *(src_addr))
#define atomic_store_from_addr_rel_32(dst_addr, src_addr)	atomic_store_rel_32(dst_addr, *(src_addr))
#define atomic_store_from_addr_seq_cst_32(dst_addr, src_addr)	atomic_store_seq_cst_32(dst_addr, *(src_addr))
               
#define atomic_store_from_addr_rlx_64(dst_addr, src_addr)	atomic_store_rlx_64(dst_addr, *(src_addr))
#define atomic_store_from_addr_rel_64(dst_addr, src_addr)	atomic_store_rel_64(dst_addr, *(src_addr))
#define atomic_store_from_addr_seq_cst_64(dst_addr, src_addr)	atomic_store_seq_cst_64(dst_addr, *(src_addr))

/******************************************** Overflow Checking ********************************************/

#define u64_add_return_overflow(dst_addr, src1, src2)	((u64) _addcarry_u64(0, src1, src2, dst_addr))

__forceinline u64 u64_mul_return_overflow(u64 *dst, const u64 src1, const u64 src2)
{
	u64 high;
	*dst = _umul128(src1, src2, &high);	
	return high;
}

/******************************************** Bit Manipulation *********************************************/

#include <immintrin.h>

/*** Some builtins (REQUIRES: (BMI, Bit Manipulation Instruction Set 1 >= year 2013)) ***/
#define clz32(x)	((u32) _lzcnt_u32(x))	/* count leading zeroes, NOTE: if x == 0, undefined! */
#define clz64(x)	((u32) _lzcnt_u64(x))	/* count leading zeroes long, NOTE: if x == 0, undefined! */
#define ctz32(x)	((u32) _tzcnt_u32(x))	/* count trailing zeroes, NOTE: if x == 0, undefined! */
#define ctz64(x)	((u32) _tzcnt_u64(x))	/* count trailing zeroes long, NOTE: if x == 0, undefined! */

/************************* win_arch.c *************************/

void os_arch_init_func_ptrs(void);

#endif
