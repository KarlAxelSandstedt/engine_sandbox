cmake_minimum_required(VERSION 3.14.3)

add_library(dtoa STATIC dtoa.h dtoa.c)
target_include_directories(dtoa INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

if (DEFINED EMSCRIPTEN)
	target_link_libraries(dtoa PUBLIC kas_common PRIVATE Threads::Threads)
elseif (CMAKE_SYSTEM_NAME STREQUAL "Linux")
	target_link_libraries(dtoa PUBLIC kas_common PRIVATE Threads::Threads)
elseif (CMAKE_SYSTEM_NAME STREQUAL "Windows")
	target_link_libraries(dtoa PUBLIC kas_common)
endif ()
