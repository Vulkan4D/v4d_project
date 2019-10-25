cmake_minimum_required(VERSION 3.10)
###########################################################################################

project(v4d_demo_project)
set(APP_NAME demo)
option(COMPILE_GLFW "Compile GLFW from source instead of linking with installed library" OFF)
option(COMPILE_V4D_CORE "Compile Vulkan4D Core library from source" ON)
option(COMPILE_V4D_MODULES "Compile all Vulkan4D Modules from source" ON)
option(USE_CCACHE "Use ccache" ON)

###########################################################################################

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
	# Warnings		https://gcc.gnu.org/onlinedocs/gcc-9.2.0/gcc/Warning-Options.html
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
	# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wconversion")
	# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wsign-conversion")
	# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wshadow")
endif()

# Config-specific
if("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
	add_definitions(-D_DEBUG)
else()
	add_definitions(-D_RELEASE)
endif()

# CCACHE
if(USE_CCACHE)
    find_program(CCACHE_FOUND ccache)
    if(CCACHE_FOUND)
        set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE ccache)
    endif(CCACHE_FOUND)
endif()

# Output Directories
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/debug")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/debug")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/release")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/release")
set(CMAKE_SHARED_LIBRARY_PREFIX_CXX "")


# glfw
if(COMPILE_GLFW)
	set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
	set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
	set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
	add_subdirectory("${PROJECT_SOURCE_DIR}/src/Vulkan-Hpp/glfw")
else()
	find_package(glfw3 3.3 REQUIRED)
endif()


# V4D Core
if(COMPILE_V4D_CORE)
	add_subdirectory(src/v4d/core)
endif()


# V4D Modules
if(COMPILE_V4D_MODULES)
	add_subdirectory(src/v4d/modules)
endif()


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


# Incubator
add_executable(incubator
	# "${PROJECT_SOURCE_DIR}/src/incubator_MyVulkan.cpp"
	"${PROJECT_SOURCE_DIR}/src/incubator_CustomVkLoader.cpp"
)
target_link_libraries(incubator
	v4d
	glfw
	# glslang
	# SPIRV
)
target_include_directories(incubator
	PRIVATE
		"${CMAKE_LIBRARY_OUTPUT_DIRECTORY}"
		"${PROJECT_SOURCE_DIR}/src/Vulkan-Hpp/glm"
		"${PROJECT_SOURCE_DIR}/src/Vulkan-Hpp/glfw/include"
		"${PROJECT_SOURCE_DIR}/src/Vulkan-Hpp/Vulkan-Headers/include"
		"${PROJECT_SOURCE_DIR}/src/Vulkan-Hpp/glslang"
)
target_compile_definitions(incubator
	PRIVATE -D_V4D_PROJECT
)
