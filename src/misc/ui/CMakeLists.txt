add_library(ui STATIC ui_core.c ui_build.c ui_local.h ui_public.h)
target_link_libraries(ui
	PUBLIC
		asset_system
		containers
		memory
		ds_math
		ds_string
		cmd
)
target_include_directories(ui INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
