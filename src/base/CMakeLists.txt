cmake_minimum_required(VERSION 3.14.3)

add_library(DreamscapeBase 
	STATIC 
	"${DS_INCLUDE_PATH}/ds_atomic.h"
	"${DS_INCLUDE_PATH}/ds_allocator.h"
	"${DS_INCLUDE_PATH}/ds_log.h"
	"${DS_SRC_PATH}/base/ds_allocator.c"
	"${DS_SRC_PATH}/base/ds_log.c"
)
add_library(Dreamscape::Base ALIAS DreamscapeBase)
target_link_libraries(DreamscapeBase PUBLIC DreamscapeDefine)
