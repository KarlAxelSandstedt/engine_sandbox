cmake_minimum_required(VERSION 3.14.3)

add_library(linux_interface STATIC linux_local.h linux_public.h linux_thread.c linux_sync_primitives.c linux_timer.c linux_error.c linux_arch.c linux_filesystem.c)
target_include_directories(linux_interface INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(linux_interface PUBLIC containers memory pthread)
