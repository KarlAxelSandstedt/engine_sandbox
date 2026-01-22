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

#ifndef __WASM_PUBLIC_H__
#define __WASM_PUBLIC_H__

#include <stdlib.h>
#include <sys/stat.h>
#include <sys/mman.h>

#include "ds_common.h"
#include "allocator.h"
#include "ds_string.h"

/******************** wasm_error.c ********************/

#include <stdarg.h>
#include <signal.h>
#include <errno.h>
#include "log.h"

#define ERROR_BUFSIZE	512
#define LOG_SYSTEM_ERROR(severity)		_LOG_SYSTEM_ERROR_CODE(severity, errno, __func__, __FILE__, __LINE__)
#define LOG_SYSTEM_ERROR_CODE(severity, code)	_LOG_SYSTEM_ERROR_CODE(severity, code, __func__, __FILE__, __LINE__)
#define _LOG_SYSTEM_ERROR_CODE(severity, code, func, file, line)						\
{														\
	u8 _err_buf[ERROR_BUFSIZE];										\
	const utf8 _err_str = utf8_system_error_code_string_buffered(_err_buf, ERROR_BUFSIZE, code);		\
	log(T_SYSTEM, severity, "At %s:%u in function %s - %k", file, line, func, &_err_str);			\
}

/* thread safe system error message generation */
utf8	utf8_system_error_code_string_buffered(u8 *buf, const u32 bufsize, const u32 code);

void 	init_error_handling_func_ptrs(void);

/******************** wasm_arch.c ********************/

void os_arch_init_func_ptrs(void);

/******************** wasm_filesystem.c ********************/

typedef struct stat	file_status;
typedef int 		file_handle;

#define FILE_HANDLE_INVALID 	-1

#include <sys/mman.h>
#define FS_PROT_READ         PROT_READ
#define FS_PROT_WRITE        PROT_WRITE
#define FS_PROT_EXECUTE      PROT_EXEC
#define FS_PROT_NONE         PROT_NONE

#define FS_MAP_SHARED        MAP_SHARED
#define FS_MAP_PRIVATE       MAP_PRIVATE

void	filesystem_init_func_ptrs(void);

/********************  wasm_thread.c	********************/

typedef pid_t			pid;
typedef pid_t			tid;
typedef struct ds_thread 	ds_thread;

/********************  wasm_sync_primitives.c	********************/

#include <semaphore.h>
typedef sem_t semaphore;

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
#define atomic_compare_exchange_acq_32(dst_addr, cmp_addr, exch_val)		__atomic_compare_exchange_n(dst_addr, cmp_addr, exch_val, 0, ATOMIC_ACQUIRE, ATOMIC_ACQUIRE)
#define atomic_compare_exchange_rel_32(dst_addr, cmp_addr, exch_val)		__atomic_compare_exchange_n(dst_addr, cmp_addr, exch_val, 0, ATOMIC_RELEASE, ATOMIC_ACQUIRE)
#define atomic_compare_exchange_seq_cst_32(dst_addr, cmp_addr, exch_val)	__atomic_compare_exchange_n(dst_addr, cmp_addr, exch_val, 0, ATOMIC_SEQ_CST, ATOMIC_SEQ_CST)

#define atomic_compare_exchange_rlx_64(dst_addr, cmp_addr, exch_val)		__atomic_compare_exchange_n(dst_addr, cmp_addr, exch_val, 0, ATOMIC_RELAXED, ATOMIC_RELAXED)
#define atomic_compare_exchange_acq_64(dst_addr, cmp_addr, exch_val)		__atomic_compare_exchange_n(dst_addr, cmp_addr, exch_val, 0, ATOMIC_ACQUIRE, ATOMIC_ACQUIRE)
#define atomic_compare_exchange_rel_64(dst_addr, cmp_addr, exch_val)		__atomic_compare_exchange_n(dst_addr, cmp_addr, exch_val, 0, ATOMIC_RELEASE, ATOMIC_ACQUIRE)
#define atomic_compare_exchange_seq_cst_64(dst_addr, cmp_addr, exch_val)	__atomic_compare_exchange_n(dst_addr, cmp_addr, exch_val, 0, ATOMIC_SEQ_CST, ATOMIC_SEQ_CST)

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

/********************  Overflow Checking ********************/

#define	u64_add_return_overflow(dst_addr, src1, src2)		__builtin_add_overflow(src1, src2, dst_addr)
#define	u64_mul_return_overflow(dst_addr, src1, src2)		__builtin_mul_overflow(src1, src2, dst_addr)

/********************  Bit Manipulation ********************/

/*** Some builtins (REQUIRES: (BMI, Bit Manipulation Instruction Set 1 >= year 2013)) ***/
#define clz32(x)	((u32) __builtin_clz(x))  	/* count leading zeroes, NOTE: if x == 0, undefined! */
#define clz64(x)	((u32) __builtin_clzll(x))	/* count leading zeroes long, NOTE: if x == 0, undefined! */
#define ctz32(x)	((u32) __builtin_ctz(x))  	/* count trailing zeroes, NOTE: if x == 0, undefined! */
#define ctz64(x)	((u32) __builtin_ctzll(x))	/* count trailing zeroes long, NOTE: if x == 0, undefined! */


#endif
