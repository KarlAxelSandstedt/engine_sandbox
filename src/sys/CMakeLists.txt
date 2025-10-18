cmake_minimum_required(VERSION 3.14.3)

add_library(system_common INTERFACE)
target_include_directories(system_common INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(system_common INTERFACE kas_common containers kas_string)

add_library(system_gl INTERFACE)
target_include_directories(system_gl INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(system_gl 
	INTERFACE 
		kas_opengl
)

add_library(system_graphics
	STATIC
		sys_local.h
		sys_public.h
		sys_graphics.c
	)

target_include_directories(system_graphics INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(system_graphics
	PUBLIC
		system
	PRIVATE
		renderer
		sdl3_wrapper
	)

add_library(system 
	STATIC
		sys_local.h
		sys_public.h
		sys_input.c
		sys_event.c
		sys_init.c
		sys_arch.c
		sys_filesystem.c
		task.c
	)

target_include_directories(system INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(system
	PUBLIC
		kas_common	
		system_common
		containers
		memory
		kas_math
		ui
		cmd
		parallel_containers
	PRIVATE
		sdl3_wrapper
		dtoa	
)

if (DEFINED EMSCRIPTEN)
	target_link_libraries(system PUBLIC wasm_interface)
elseif (CMAKE_SYSTEM_NAME STREQUAL "Linux")
	target_link_libraries(system PUBLIC linux_interface)
elseif (CMAKE_SYSTEM_NAME STREQUAL "Windows")
	target_link_libraries(system PUBLIC windows_interface)
endif ()
