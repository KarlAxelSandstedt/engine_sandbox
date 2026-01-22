cmake_minimum_required(VERSION 3.14.3)

# files to link to later
add_library(ds_random STATIC
	ds_random.h
	rng.c
)

target_link_libraries(ds_random PUBLIC ds_common system)
target_include_directories(ds_random PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
