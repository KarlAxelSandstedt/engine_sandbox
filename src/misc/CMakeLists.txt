cmake_minimum_required(VERSION 3.14.3)

add_library(cmd 
	STATIC 
		cmd.h 
		cmd.c
	)
target_link_libraries(cmd PUBLIC ds_common containers ds_string PRIVATE system dtoa)
target_include_directories(cmd INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

add_library(ds_string ds_string.c ds_string.h)
target_link_libraries(ds_string PRIVATE ds_math dtoa PUBLIC ds_common containers asset_system)
target_include_directories(ds_string INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

add_library(log log.c log.h)
target_link_libraries(log PUBLIC ds_common ds_string PRIVATE parallel_containers system)
target_include_directories(log INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

add_library(serialize STATIC serialize.c serialize.h)
target_include_directories(serialize INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(serialize PUBLIC ds_common ds_string)
