cmake_minimum_required(VERSION 3.14.3)

add_subdirectory(parallel)

# files to link to later
add_library(containers STATIC
	hash_map.c
	hash_map.h
	queue.c
	queue.h
	bit_vector.c
	bit_vector.h
	kas_vector.c
	kas_vector.h
	array_list.c
	array_list.h
	list.c
	list.h
	net_list.c
	net_list.h
	hierarchy_index.c
	hierarchy_index.h
	string_database.c
	string_database.h
)

target_link_libraries(containers
	PUBLIC
		kas_common
		memory
		system
		allocator_debug
		serialize
	PRIVATE
		kas_math
)

if (UNIX)
	target_link_libraries(containers 
		PRIVATE 
			rt # posix IPC
	)
endif ()

target_include_directories(containers INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
