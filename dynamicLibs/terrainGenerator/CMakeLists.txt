cmake_minimum_required(VERSION 3.10)
###########################################################################################

project(v4d_dynamicLibs_terrainGenerator)
set (thisLib "terrainGenerator")

enable_language(CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_SYSTEM_PROCESSOR x86_64)
if(WIN32)
	add_definitions(-D_WINDOWS -D_WIN32_WINNT=0x06030000)
else()
	add_definitions(-D_LINUX)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fmax-errors=1")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wfatal-errors")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wextra")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wunused")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wnon-virtual-dtor")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wcast-align")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Woverloaded-virtual")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wnull-dereference")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wpessimizing-move")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wredundant-move")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wlogical-op")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wdisabled-optimization")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wfloat-conversion")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unused-parameter")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-sign-compare")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unknown-pragmas")
endif()
set(CMAKE_SHARED_LIBRARY_PREFIX_CXX "")
set(CMAKE_SHARED_MODULE_PREFIX_CXX "")

add_library(${thisLib} SHARED "${PROJECT_SOURCE_DIR}/${thisLib}.cpp")

target_include_directories(${thisLib}
	PUBLIC
		"${CMAKE_CURRENT_SOURCE_DIR}"
		"${PROJECT_SOURCE_DIR}/../../src/v4d/xvk/glm"
)

# Platform-specific libs
if(WIN32)
	target_link_libraries(${thisLib}
		wsock32
		ws2_32
		gcc
		-static-libstdc++
	)
else()
	target_link_libraries(${thisLib}
		dl
	)
endif()
