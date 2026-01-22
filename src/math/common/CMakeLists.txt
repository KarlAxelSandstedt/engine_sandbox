cmake_minimum_required(VERSION 3.14.3)

# files to link to later
add_library(ds_math STATIC
	matrix.c
	vector.c
	ds_math.c
	float32.c
	geometry.c
	quaternion.c
	transform.c
	matrix.h
	vector.h
	ds_math.h
	float32.h
	geometry.h
	quaternion.h
	transform.h
)

target_link_libraries(ds_math PUBLIC ds_random ds_common)
if (UNIX) 
	target_link_libraries(ds_math PUBLIC m)
endif ()


target_include_directories(ds_math PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
