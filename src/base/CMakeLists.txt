cmake_minimum_required(VERSION 3.14.3)

add_library(DreamscapeBase 
	STATIC 
	"${DS_INCLUDE_PATH}/ds_allocator.h"
	"${DS_SRC_PATH}/base/ds_allocator.c"
)
add_library(Dreamscape::Base ALIAS DreamscapeBase)
target_link_libraries(DreamscapeBase PUBLIC DreamscapeDefine)
