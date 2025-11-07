cmake_minimum_required(VERSION 3.14.3)

add_library(physics STATIC
	physics_pipeline.c
	contact_database.c
	contact_solver.c
	island.c
	dynamics.h
)

target_link_libraries(physics PUBLIC
	collision
	containers
	system
)

target_include_directories(physics PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
