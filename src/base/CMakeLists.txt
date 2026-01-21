cmake_minimum_required(VERSION 3.14.3)

add_library(DreamscapeBase INTERFACE "${INCLUDE_PATH}/ds_base.h")
add_library(Dreamscape::Base ALIAS DreamscapeBase)

target_compile_definitions(DreamscapeBase INTERFACE DS_VERSION_CSTR="${CPACK_PACKAGE_VERSION}")
target_include_directories(DreamscapeBase INTERFACE ${INCLUDE_PATH})

if (DS_DEBUG)
	target_compile_definitions(DreamscapeBase INTERFACE DS_DEBUG DS_ASSERT)
endif()
