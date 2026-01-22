cmake_minimum_required(VERSION 3.14.3)

add_library(allocator_debug 
	STATIC 
		allocator_debug.c 
		allocator_debug.h)
target_link_libraries(allocator_debug PUBLIC ds_common containers PRIVATE system)
target_include_directories(allocator_debug INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
