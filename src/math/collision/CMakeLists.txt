cmake_minimum_required(VERSION 3.14.3)

add_library(collision STATIC
	csg.h
	csg.c
	collision.h
	collision.c
	dbvt.h
	dbvt.c
	collision_debug.c
	shape.c
)

target_link_libraries(collision PUBLIC
	memory
	containers
	kas_math
)

target_include_directories(collision PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${DYNAMICS_INCLUDE})
