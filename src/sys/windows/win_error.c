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

#include "win_local.h"
#include "ds_string.h"

#include <minidumpapiset.h>

void (*fatal_cleanup_and_exit)(const u32 thread);

u32 a_fatal_cleanup_initiated;
static void win_fatal_cleanup_and_exit(const u32 thread)
{
	u32 desired = 0;
	// TODO Somehow stop/force all threads to not continue / shut down program until cleanup is complete 
	
	if (atomic_compare_exchange_acq_32(&a_fatal_cleanup_initiated, &desired, 1))
	{
    		SYSTEMTIME local_time;
    		GetLocalTime(&local_time);

		struct arena tmp = arena_alloc_1MB();
		const utf8 utf8_filename = utf8_format(&tmp, "%s_%s_latest.dmp"
				, DS_EXECUTABLE_CSTR
				, DS_VERSION_CSTR);
		//const utf8 utf8_filename = utf8_format(&tmp, "%s_%s_%u%u%u_%u%u%u.dmp", 
    		//           "engine_sandbox",
		//	   "0_1", 
    		//           local_time.wYear,
		//	   local_time.wMonth,
		//	   local_time.wDay, 
    		//           local_time.wHour, 
		//	   local_time.wMinute,
		//	   local_time.wSecond);
    		const char *filename = cstr_utf8(&tmp, utf8_filename);
		struct file dump = file_null();
		if (file_try_create_at_cwd(&tmp, &dump, filename, FILE_TRUNCATE) == FS_SUCCESS)
		{
			if (!MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), dump.handle, MiniDumpWithFullMemory, NULL, NULL, NULL))
			{
				log_system_error(S_ERROR);
			}
			
			file_close(&dump);
		}
		arena_free_1MB(&tmp);

		log_shutdown();
		exit(0);
	}

	//TODO spin threads that di not acquire lock or exit or something...
}

void init_error_handling_func_ptrs(void)
{
	atomic_store_rel_32(&a_fatal_cleanup_initiated, 0);
	fatal_cleanup_and_exit = &win_fatal_cleanup_and_exit;
}


utf8 utf8_system_error_buffered(u8 *buf, const u32 bufsize)
{
	utf8 err_str =
	{
		.buf = buf,
		.size = bufsize,
		.len = 0,
	};

	const DWORD code = GetLastError();
	err_str.len  = FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM |FORMAT_MESSAGE_IGNORE_INSERTS,
					 NULL,
					 code,
					 0,
					 buf,
					 (DWORD) bufsize,
					 NULL);

	return (err_str.len > 0) ? err_str : utf8_empty();
}

utf8 utf8_nt_status_buffered(u8 *buf, const u32 bufsize, const NTSTATUS status)
{
	utf8 str = utf8_empty();

	HMODULE nt_handle;
	GetModuleHandleEx(0, "ntdll.dll", &nt_handle);

	const DWORD len = FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_FROM_HMODULE
			, nt_handle
			, status
			, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT)
			, (LPSTR) buf
			, bufsize / sizeof(TCHAR)
			, NULL);
	if (len)
	{
		str.buf = buf;
		str.len = len;
		str.size = bufsize;
	}

	FreeLibrary(nt_handle);

	return str;
}
