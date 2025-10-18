cmake_minimum_required(VERSION 3.14.3)

add_library(kas_common INTERFACE)
target_include_directories(kas_common
	INTERFACE
		${CMAKE_CURRENT_SOURCE_DIR}
)
