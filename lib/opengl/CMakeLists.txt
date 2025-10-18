cmake_minimum_required(VERSION 3.14.3)

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
	add_compile_options(-w)
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
	add_compile_options(/w)
endif()

add_library(kas_opengl INTERFACE)
target_include_directories(kas_opengl INTERFACE "${LIB_PATH}/opengl")

