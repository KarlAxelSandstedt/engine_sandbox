cmake_minimum_required(VERSION 3.14.3)

add_library(collision STATIC
	collision.h
	collision.c
	bvh.c
)

target_link_libraries(collision PUBLIC
	memory
	containers
	ds_math
)

target_include_directories(collision PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${DYNAMICS_INCLUDE})
