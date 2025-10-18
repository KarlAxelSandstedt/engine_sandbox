cmake_minimum_required(VERSION 3.14.3)

add_library(physics STATIC
	physics_pipeline.c
	rigid_body.c
	contact_database.c
	contact_solver.c
	island.c
	physics_pipeline.h
	rigid_body.h
	contact_solver.h
	contact_database.h
	island.h
)

target_link_libraries(physics PUBLIC
	collision
	containers
	system
)

target_include_directories(physics PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
