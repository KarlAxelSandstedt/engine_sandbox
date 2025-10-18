cmake_minimum_required(VERSION 3.14.3)

# files to link to later
add_library(kas_math STATIC
	matrix.c
	vector.c
	kas_math.c
	float32.c
	geometry.c
	quaternion.c
	transform.c
	matrix.h
	vector.h
	kas_math.h
	float32.h
	geometry.h
	quaternion.h
	transform.h
)

target_link_libraries(kas_math PUBLIC kas_random kas_common)
if (UNIX) 
	target_link_libraries(kas_math PUBLIC m)
endif ()


target_include_directories(kas_math PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
