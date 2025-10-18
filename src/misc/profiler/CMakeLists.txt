cmake_minimum_required(VERSION 3.14.3)

add_library(kas_profiler STATIC kaspf_reader.c kas_profiler.c kas_profiler.h)
target_link_libraries(kas_profiler PUBLIC kas_common PRIVATE system)
target_include_directories(kas_profiler INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
