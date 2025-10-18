cmake_minimum_required(VERSION 3.14.3)

# files to link to later
add_library(parallel_containers STATIC 
	fifo_mpsc.c 
	fifo_spsc.c 
	fifo_spmc.c 
	ticket_factory.c
	fifo_mpsc.h 
	fifo_spsc.h
	fifo_spmc.h
	ticket_factory.h
)

target_link_libraries(parallel_containers PUBLIC kas_common memory system kas_math)
target_include_directories(parallel_containers INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
