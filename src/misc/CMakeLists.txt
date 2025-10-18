cmake_minimum_required(VERSION 3.14.3)

add_library(cmd 
	STATIC 
		cmd.h 
		cmd.c
	)
target_link_libraries(cmd PUBLIC kas_common containers kas_string PRIVATE system dtoa)
target_include_directories(cmd INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

add_library(kas_string kas_string.c kas_string.h)
target_link_libraries(kas_string PRIVATE kas_math dtoa PUBLIC kas_common containers asset_system)
target_include_directories(kas_string INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

add_library(log log.c log.h)
target_link_libraries(log PUBLIC kas_common kas_string PRIVATE parallel_containers system)
target_include_directories(log INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

add_library(serialize STATIC serialize.c serialize.h)
target_include_directories(serialize INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(serialize PUBLIC kas_common kas_string)
