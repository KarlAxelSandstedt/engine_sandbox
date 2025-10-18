cmake_minimum_required(VERSION 3.14.3)

add_library(asset_system STATIC 
	asset_public.h
	asset_local.h
	asset_ssff.c
	asset_font.c
	asset_database.c 
	asset_init.c
	)
target_include_directories(asset_system INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(asset_system 
	PRIVATE 
		system 
		stb_image
		freetype
	PUBLIC
		kas_common
		memory
		kas_string
	)
