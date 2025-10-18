cmake_minimum_required(VERSION 3.14.3)

add_library(memory STATIC allocator.h allocator.c)
target_link_libraries(memory PRIVATE system)
target_include_directories(memory INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
	target_link_libraries(memory PRIVATE onecore)
endif ()
