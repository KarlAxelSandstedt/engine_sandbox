cmake_minimum_required(VERSION 3.14.3)

add_library(wasm_interface STATIC 
	wasm_local.h 
	wasm_public.h 
	wasm_thread.c 
	wasm_sync_primitives.c 
	wasm_timer.c 
	wasm_error.c 
	wasm_filesystem.c 
	wasm_arch.c
)
target_include_directories(wasm_interface INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(wasm_interface PUBLIC containers memory)
