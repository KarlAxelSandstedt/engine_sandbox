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


#endif
