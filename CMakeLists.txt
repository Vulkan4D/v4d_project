# https://cmake.org/cmake/help/v3.16/guide/tutorial/index.html

cmake_minimum_required(VERSION 3.10)

project(v4d_demo_project)
set(APP_NAME demo)

# Global specs
enable_language(CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Platforms
set(CMAKE_SYSTEM_PROCESSOR x86_64)
if(WIN32)
	add_definitions(-D_WINDOWS -D_WIN32_WINNT=0x06030000)
else()
	add_definitions(-D_LINUX)
endif()

# Config-specific
if("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
	add_definitions(-D_DEBUG)
else()
	add_definitions(-D_RELEASE)
endif()

# Warnings
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} \
	-Wall \
	-Wextra \
	-Wnon-virtual-dtor \
	-Wcast-align \
	-Woverloaded-virtual \
	-Wconversion \
	-Wsign-conversion \
	-Wnull-dereference \
	-Wdouble-promotion \
	-Wformat=2 \
	-Wunused \
	-Wpessimizing-move \
	-Wredundant-move \
	\
	-Wduplicated-cond \
	-Wduplicated-branches \
	-Wlogical-op \
")

# Output Directories
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/debug")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/debug")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/release")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/release")
set(CMAKE_SHARED_LIBRARY_PREFIX_CXX "")

# V4D Core
add_subdirectory(src/v4d/core)

# Modules
add_subdirectory(src/v4d/modules/sample)

# Project
add_executable(${APP_NAME}
	"${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp"
)
target_link_libraries(${APP_NAME}
	v4d
)
target_include_directories(${APP_NAME}
	PRIVATE
		"${CMAKE_LIBRARY_OUTPUT_DIRECTORY}"
)
target_compile_definitions(${APP_NAME}
	PRIVATE -D_V4D_PROJECT
)

# Tests
add_executable(tests
	"${CMAKE_CURRENT_SOURCE_DIR}/src/tests.cxx"
)
target_link_libraries(tests
	v4d
)
target_include_directories(tests
	PRIVATE
		"${CMAKE_LIBRARY_OUTPUT_DIRECTORY}"
)
target_compile_definitions(tests
	PRIVATE -D_V4D_PROJECT
)


# # Incubator
# add_executable(incubator
# 	"${CMAKE_CURRENT_SOURCE_DIR}/src/incubator.cpp"
# )
# target_link_libraries(incubator
# 	v4d
# 	glfw
# 	vulkan
# 	glslang
# 	SPIRV
# )
# target_include_directories(incubator
# 	PRIVATE
# 		"${CMAKE_LIBRARY_OUTPUT_DIRECTORY}"
# 		"${CMAKE_CURRENT_SOURCE_DIR}/src/Vulkan-Hpp"
# 		"${CMAKE_CURRENT_SOURCE_DIR}/src/Vulkan-Hpp/glm"
# 		"${CMAKE_CURRENT_SOURCE_DIR}/src/Vulkan-Hpp/glslang"
# 		"${CMAKE_CURRENT_SOURCE_DIR}/src/Vulkan-Hpp/glfw/include"
# 		"${CMAKE_CURRENT_SOURCE_DIR}/src/Vulkan-Hpp/Vulkan-Headers/include"
# )
# target_compile_definitions(incubator
# 	PRIVATE -D_V4D_PROJECT
# )
# find_package(glfw3 3.3 REQUIRED)
# find_package(Vulkan 1.1 REQUIRED)

