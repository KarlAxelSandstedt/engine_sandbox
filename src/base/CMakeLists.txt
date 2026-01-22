cmake_minimum_required(VERSION 3.14.3)

add_library(DreamscapeBase 
	STATIC 
	"${DS_INCLUDE_PATH}/ds_base.h" 
	"${DS_SRC_PATH}/base/ds_allocator.h"
	"${DS_SRC_PATH}/base/ds_allocator.c"
	#"${DS_SRC_PATH}/base/ds_log.h"
	#"${DS_SRC_PATH}/base/ds_log.c"
)
add_library(Dreamscape::Base ALIAS DreamscapeBase)

target_compile_definitions(DreamscapeBase PUBLIC DS_VERSION_CSTR="${CPACK_PACKAGE_VERSION}")
target_include_directories(DreamscapeBase PUBLIC ${DS_INCLUDE_PATH})

if (DS_DEBUG)
	target_compile_definitions(DreamscapeBase PUBLIC DS_DEBUG DS_ASSERT)
endif()

if (DS_PROFILE)
	target_compile_definitions(DreamscapeBase PUBLIC DS_PROFILE)
	target_link_libraries(DreamscapeBase PUBLIC Tracy::TracyClient)
	target_include_directories(DreamscapeBase PUBLIC tracy/public)
endif ()

if (DS_LOG)
	target_compile_definitions(DreamscapeBase PUBLIC DS_LOG)
endif ()
