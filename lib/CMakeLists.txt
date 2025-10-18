cmake_minimum_required(VERSION 3.14.3)

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
	add_compile_options(-w)
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
endif()

add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/SDL")
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/freetype")
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/dtoa")
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/opengl")

add_library(stb_image INTERFACE)
target_include_directories(stb_image INTERFACE "${LIB_PATH}/stb_image")

