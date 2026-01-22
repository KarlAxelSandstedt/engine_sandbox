cmake_minimum_required(VERSION 3.14.3)

add_library(led STATIC 
	led_public.h
	led_local.h
	led_init.c
	led_main.c
	led_utility.c
	led_visual.c
	led_ui.c
	led_core.c
	csg.c
	csg.h
	)

target_link_libraries(led
	PUBLIC
		ds_common
		memory
		system
		renderer
		asset_system
)
target_include_directories(led INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

