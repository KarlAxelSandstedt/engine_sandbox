cmake_minimum_required(VERSION 3.14.3)

# files to link to later
add_library(kas_random STATIC
	kas_random.h
	rng.c
)

target_link_libraries(kas_random PUBLIC kas_common system)
target_include_directories(kas_random PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
