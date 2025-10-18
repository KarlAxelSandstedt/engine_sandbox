cmake_minimum_required(VERSION 3.14.3)

add_library(kas_test 
	test_local.h
	test_public.h
	test_repetition.c
	test_main.c 
	test_container.c
	test_math.c
	test_string.c
	test_serialize.c
	test_allocator.c
	test_rng.c)

target_link_libraries(kas_test PRIVATE 
	system
	containers
	kas_math
	kas_string
	serialize
	dtoa
	) 

target_include_directories(kas_test INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
