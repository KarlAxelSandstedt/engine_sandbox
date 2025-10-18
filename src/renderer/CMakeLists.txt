cmake_minimum_required(VERSION 3.14.3)

add_library(renderer STATIC
	r_public.h
	r_local.h
	r_init.c
	r_main.c
	r_mesh.c
	r_camera.c
	r_proxy3d.c
	r_core.c
	r_debug.c
	r_scene.c
	r_ui.c
	r_gl.c
)

target_link_libraries(renderer
	PUBLIC
		memory
		led
		physics
	PRIVATE
		system
		system_gl
		asset_system
		ui
)

target_include_directories(renderer INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
