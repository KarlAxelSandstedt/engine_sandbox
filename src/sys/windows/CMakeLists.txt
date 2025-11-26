cmake_minimum_required(VERSION 3.14.3)

add_library(windows_interface STATIC
	win_public.h
	win_local.h
	win_arch.c
	win_timer.c
	win_error.c
	win_filesystem.c
	win_thread.c
	win_sync_primitives.c
	kernel_tracer.c
	)

	target_link_libraries(windows_interface PUBLIC containers memory PRIVATE ntdll shlwapi Pathcch DbgHelp)
target_include_directories(windows_interface INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
