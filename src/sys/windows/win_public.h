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
#include "ds_common.h"

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

typedef struct dsThread		dsThread;

#endif
