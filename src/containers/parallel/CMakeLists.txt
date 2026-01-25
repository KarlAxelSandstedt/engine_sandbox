cmake_minimum_required(VERSION 3.14.3)

# files to link to later
add_library(parallel_containers STATIC 
	fifo_spmc.c 
	fifo_spmc.h
)

target_link_libraries(parallel_containers PUBLIC ds_common memory system ds_math)
target_include_directories(parallel_containers INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
