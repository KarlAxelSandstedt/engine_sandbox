cmake_minimum_required(VERSION 3.14.3)

find_program(NINJA_EXECUTABLE ninja)
if (NINJA_EXECUTABLE)
	set(CMAKE_GENERATOR "Ninja")
endif()

project(engine_sandbox)
set(CPACK_PACKAGE_MAJOR "0")
set(CPACK_PACKAGE_MINOR "1")
set(CPACK_PACKAGE_PATCH "0")
set(CPACK_PACKAGE_VERSION "${CPACK_PACKAGE_MAJOR}.${CPACK_PACKAGE_MINOR}.${CPACK_PACKAGE_PATCH}")

#TODO Turn off when making release builds
add_compile_definitions(KAS_DEV)
add_compile_definitions(KAS_EXECUTABLE_CSTR="${PROJECT_NAME}")
add_compile_definitions(KAS_VERSION_CSTR="${CPACK_PACKAGE_VERSION}")

set(SRC_PATH "${CMAKE_CURRENT_SOURCE_DIR}/src")
set(TEST_PATH "${CMAKE_CURRENT_SOURCE_DIR}/test")
set(LIB_PATH "${CMAKE_CURRENT_SOURCE_DIR}/lib")

if (DEFINED EMSCRIPTEN)
	add_executable(${PROJECT_NAME} ${SRC_PATH}/sys/wasm/main.c)
	set(CMAKE_EXECUTABLE_SUFFIX ".html")
	add_link_options(--emrun)
else ()
	if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
		add_executable(${PROJECT_NAME} ${SRC_PATH}/sys/linux/main.c)
	elseif (CMAKE_SYSTEM_NAME STREQUAL "Windows")
		add_executable(${PROJECT_NAME} WIN32 ${SRC_PATH}/sys/windows/WinMain.c)
	endif ()
	
	##### Sanitizing Options #####
	#option(thread_sanitizer "thread_sanitizer" OFF)
	#option(address_sanitizer "address_sanitizer" OFF)
	
	if (thread_sanitizer)
		set(apply_optimization_options ON)
		set(CMAKE_C_FLAGS  "${CMAKE_C_FLAGS} -fsanitize=thread -fno-omit-frame-pointer")
		link_libraries(tsan)
		#add_compile_options(-fsanitize=thread -fno-omit-frame-pointer)
		add_compile_definitions(KAS_TSAN)
	endif ()
	
	message(CHECK_START "Address Sanitizer")
	if (address_sanitizer)
		set(apply_optimization_options ON)
		set(CMAKE_C_FLAGS  "${CMAKE_C_FLAGS} -fsanitize=address -fno-omit-frame-pointer")
		link_libraries(asan)
		#add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
		add_compile_definitions(KAS_ASAN)
	endif ()
	
	if (undefined_behaviour_sanitizer)
		set(apply_optimization_options ON)
		set(CMAKE_C_FLAGS  "${CMAKE_C_FLAGS} -fsanitize=undefined")
		link_libraries(ubsan)
		#add_compile_options(-fsanitize=undefined)
		add_compile_definitions(KAS_UBSAN)
	endif ()

	unset(thread_sanitizer CACHE)
	unset(address_sanitizer CACHE)
	unset(undefined_behaviour_sanitizer CACHE)
endif()

if (kas_debug)
	add_compile_definitions(KAS_DEBUG)
else ()
	#target_compile_definitions(SDL3-shared CMAKE_BUILD_TYPE=Release)
endif ()

if (kas_test_correctness)
	add_compile_definitions(KAS_TEST_CORRECTNESS)
	add_subdirectory("${TEST_PATH}")
	target_link_libraries(${PROJECT_NAME} PRIVATE kas_test)
elseif (kas_test_performance)
	add_compile_definitions(KAS_TEST_PERFORMANCE)
	add_subdirectory("${TEST_PATH}")
	target_link_libraries(${PROJECT_NAME} PRIVATE kas_test)
endif ()

message(CHECK_START "Compiler")
if (DEFINED EMSCRIPTEN)
	message(CHECK_PASS "Emscripten")
	# enable simd => web assembly vector instructions
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mavx -msimd128")
	# enable wasm workers
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -sWASM_WORKERS=1")

	if (kas_debug)
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O0 -W -Wall -gsource-map -Wno-switch -Wno-unused-parameter -Wno-unused-variable -Wno-unused-function")
		# high overhead; check every memory reference?
		add_link_options(STACK_OVERFLOW_CHECK=2)
		add_link_options(ASSERTIONS=2)
	else ()
		# low overhead; end-of-frame checking?
		add_link_options(STACK_OVERFLOW_CHECK=1)
	endif ()

	if (apply_optimization_options)
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3")
	endif ()	

	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} --embed-file=../assets@../assets")
	# enable memory growth, no over commitment of memory when process requires more memory
  	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -sALLOW_MEMORY_GROWTH=1 -sMEMORY_GROWTH_GEOMETRIC_STEP=0")

elseif (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
	message(CHECK_PASS "MSVC")

	# avoids x87 fp-instructions entirely (extended precision bad for consistency)
	add_compile_options(/arch:SSE2)

	#parallel compilation
	add_compile_options(/MP)

	if (kas_debug)
		add_compile_options(/Od /Zi)
	endif ()

	if (apply_optimization_options)
		add_compile_options(/O2)
	endif ()
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
	message(CHECK_PASS "GCC")

	# avoids x87 fp-instructions entirely (extended precision bad for consistency)
	add_compile_options(-mfpmath=sse)

  	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -rdynamic")

	if (kas_debug)
		add_compile_options(-W -Wall -gdwarf -Wno-switch -Wno-unused-parameter -Wno-unused-variable -Wno-unused-function)
	endif ()

	if (apply_optimization_options)
		add_compile_options(-O3)
	endif ()
else ()
	message(CHECK_FAIL "Unrecognized Compiler")
endif ()

unset(kas_debug CACHE)
unset(kas_test_correctness CACHE)
unset(kas_test_performance CACHE)
unset(apply_optimization_options CACHE)

find_package(OpenGL REQUIRED)
find_package(Threads REQUIRED)

add_subdirectory("${LIB_PATH}")
add_subdirectory("${SRC_PATH}/misc")
add_subdirectory("${SRC_PATH}/misc/ui")
add_subdirectory("${SRC_PATH}/misc/profiler")
add_subdirectory("${SRC_PATH}/misc/debug")
add_subdirectory("${SRC_PATH}/common")
add_subdirectory("${SRC_PATH}/math")
add_subdirectory("${SRC_PATH}/containers")
add_subdirectory("${SRC_PATH}/memory")
add_subdirectory("${SRC_PATH}/renderer")
add_subdirectory("${SRC_PATH}/sys")
add_subdirectory("${SRC_PATH}/sys/sdl3_wrapper")
add_subdirectory("${SRC_PATH}/led")
add_subdirectory("${SRC_PATH}/asset_system")


link_libraries(kas_common)
link_libraries(log)
link_libraries(kas_profiler)

if (DEFINED EMSCRIPTEN)
	add_subdirectory(${SRC_PATH}/sys/wasm)
	#target_include_directories(${PROJECT_NAME}
	#	PRIVATE
	#		"${LIB_PATH}"
	#		"${SRC_PATH}/misc"
	#		"${SRC_PATH}/misc/profiler"
	#		"${SRC_PATH}/common"
	#		"${SRC_PATH}/math"
	#		"${SRC_PATH}/math/common"
	#		"${SRC_PATH}/containers"
	#		"${SRC_PATH}/containers/parallel"
	#		"${SRC_PATH}/memory"
	#		"${SRC_PATH}/renderer"
	#		"${SRC_PATH}/sys"
	#		"${SRC_PATH}/sys/sdl3_wrapper"
	#		"${SRC_PATH}/game"
	#		"${SRC_PATH}/asset_system"
	#		"${SRC_PATH}/sys"
	#		"${SRC_PATH}/sys/wasm"
	#)

	target_link_libraries(${PROJECT_NAME}
		PRIVATE
			wasm_interface
			memory
			system
			renderer
			asset_system
	)
elseif (CMAKE_SYSTEM_NAME STREQUAL "Linux")
	add_subdirectory(${SRC_PATH}/sys/linux)
	target_link_libraries(${PROJECT_NAME}
		PRIVATE
			linux_interface
			system_graphics
			renderer
			asset_system
			led
	)
elseif (CMAKE_SYSTEM_NAME STREQUAL "Windows")
	add_subdirectory(${SRC_PATH}/sys/windows)
	target_link_libraries(${PROJECT_NAME}
		PRIVATE
			windows_interface
			system_graphics
			renderer
			asset_system
			led
			bcrypt
		)

		# DLLs must be in same folder as exe? Not as on linux where we have rpath and such...
		add_custom_command(
			TARGET	${PROJECT_NAME} POST_BUILD
			COMMAND ${CMAKE_COMMAND} -E copy_if_different
				$<TARGET_FILE:SDL3::SDL3-shared>
				$<TARGET_FILE_DIR:${PROJECT_NAME}>
			VERBATIM
		)
endif ()

#
# For CPack and installation process, good information can be found at:
# https://martin-fieber.de/blog/cmake-cpack-cross-platform-distributables/
#
# Contains definition for CMAKE_INSTALL_***DIR
include(GNUInstallDirs)
# get common install destinations for various platforms 
include(InstallRequiredSystemLibraries)
# enable CPack to escape values when writing its config files
set(CPACK_PACKAGE_NAME ${PROJECT_NAME})
set(CPACK_PACKAGE_INSTALL_DIRECTORY ${CPACK_PACKAGE_NAME})
set(CPACK_VERBATIM_VARIABLES YES)
set(CPACK_PACKAGE_VENDOR "OrangutanBaboozaAB")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/COPYING.txt")

if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
	set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
	set(CPACK_DEB_COMPONENT_INSTALL YES)
	set(CPACK_GENERATOR TGZ DEB)
	set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Apeman")
	set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS YES)
	set(CPACK_PACKAGING_INSTALL_PREFIX "/opt/${PROJECT_NAME}")

	# set RPATH for executable in install package 
	set_target_properties(${PROJECT_NAME} PROPERTIES INSTALL_RPATH $ORIGIN/../${CMAKE_INSTALL_BINDIR})
	set_target_properties(${PROJECT_NAME} PROPERTIES INSTALL_RPATH $ORIGIN/../${CMAKE_INSTALL_LIBDIR})
	install(TARGETS SDL3-shared DESTINATION ${CMAKE_INSTALL_LIBDIR})

	#install(FILES $<TARGET_FILE:SDL3::SDL3> DESTINATION ${CMAKE_INSTALL_LIBDIR} RENAME libSDL3.so.0)
elseif (CMAKE_SYSTEM_NAME STREQUAL "Windows")
	set(CPACK_GENERATOR NSIS)
	set(CPACK_NSIS_MODIFY_PATH ON)

	install(TARGETS SDL3-shared DESTINATION ${CMAKE_INSTALL_BINDIR})
endif ()

install(TARGETS ${PROJECT_NAME} DESTINATION ${CMAKE_INSTALL_BINDIR}
	RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}	# dlls and such on windows
	LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}	# (shared libraries, except dlls on windows)
	ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}	# static libraries
)

install(DIRECTORY asset_components/led_projects DESTINATION asset_components)
install(DIRECTORY asset_components/textures DESTINATION asset_components)
install(DIRECTORY asset_components/ttf DESTINATION asset_components)

install(DIRECTORY assets/shaders DESTINATION assets)
install(DIRECTORY assets/fonts DESTINATION assets)
install(DIRECTORY assets/sprites DESTINATION assets)

# Must always be after all CPACK_* variables have been defined 
include(CPack)
