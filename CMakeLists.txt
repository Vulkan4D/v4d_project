cmake_minimum_required(VERSION 3.10)

# Vulkan4D Project Build Template 0.1
###########################################################################################

project(v4d_demo_project)
set(APP_NAME demo)

option(COMPILE_TESTS "Build Unit Tests" ON)
option(COMPILE_GLFW "Compile GLFW from source instead of linking with installed library" ON)
option(COMPILE_BULLET "Compile Bullet3 from source instead of linking with installed library" OFF)
option(COMPILE_V4D_CORE "Compile Vulkan4D Core library from source" ON)
option(COMPILE_V4D_MODULES "Compile all Vulkan4D Modules from source" ON)
option(COMPILE_ASSETS "Compile all assets" ON)
option(USE_CCACHE "Use ccache" ON)
option(ENABLE_IMGUI "Enable ImGUI" ON)
option(ADDRESS_SANITIZER "Enable Address Sanitizer" OFF)

###########################################################################################

# Global specs
enable_language(CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Platforms
set(CMAKE_SYSTEM_PROCESSOR x86_64)
if(WIN32)
	add_definitions(-D_WINDOWS -D_WIN32_WINNT=0x06030000)
	set(BUILD_FLAGS "-W -Wno-cast-function-type")
else()
	add_definitions(-D_LINUX)
	# Warnings		https://gcc.gnu.org/onlinedocs/gcc-9.2.0/gcc/Warning-Options.html
	set(BUILD_FLAGS "-Wall")
	set(BUILD_FLAGS "${BUILD_FLAGS} -fmax-errors=1")
	set(BUILD_FLAGS "${BUILD_FLAGS} -Wfatal-errors")
	set(BUILD_FLAGS "${BUILD_FLAGS} -Wextra")
	set(BUILD_FLAGS "${BUILD_FLAGS} -Wunused")
	set(BUILD_FLAGS "${BUILD_FLAGS} -Wnon-virtual-dtor")
	set(BUILD_FLAGS "${BUILD_FLAGS} -Wcast-align")
	set(BUILD_FLAGS "${BUILD_FLAGS} -Woverloaded-virtual")
	set(BUILD_FLAGS "${BUILD_FLAGS} -Wnull-dereference")
	set(BUILD_FLAGS "${BUILD_FLAGS} -Wpessimizing-move")
	set(BUILD_FLAGS "${BUILD_FLAGS} -Wredundant-move")
	set(BUILD_FLAGS "${BUILD_FLAGS} -Wlogical-op")
	set(BUILD_FLAGS "${BUILD_FLAGS} -Wdisabled-optimization")
	set(BUILD_FLAGS "${BUILD_FLAGS} -Wfloat-conversion")
	set(BUILD_FLAGS "${BUILD_FLAGS} -Wno-unused-parameter")
	set(BUILD_FLAGS "${BUILD_FLAGS} -Wno-sign-compare")
	set(BUILD_FLAGS "${BUILD_FLAGS} -Wno-unknown-pragmas")
	# set(BUILD_FLAGS "${BUILD_FLAGS} -Wconversion")
	# set(BUILD_FLAGS "${BUILD_FLAGS} -Wsign-conversion")
	# set(BUILD_FLAGS "${BUILD_FLAGS} -Wshadow")
	
	# RPATH
	set(CMAKE_INSTALL_RPATH "\$ORIGIN")
	set(CMAKE_INSTALL_RPATH_USE_LINK_PATH True)
endif()

if(ADDRESS_SANITIZER)
	if(WIN32)
	else()
		string(APPEND CMAKE_CXX_FLAGS_DEBUG " -fno-omit-frame-pointer -fsanitize=address")
	endif()
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
set(CMAKE_SHARED_MODULE_PREFIX_CXX "")
SET(CMAKE_DEBUG_POSTFIX "" CACHE STRING "" FORCE)
SET(CMAKE_MINSIZEREL_POSTFIX "" CACHE STRING "" FORCE)
SET(CMAKE_RELWITHDEBINFO_POSTFIX "" CACHE STRING "" FORCE)

# Config-specific
if("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
	set(DEBUG 1)
	set(RUNTIME_OUTPUT_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG})
	add_definitions(-D_DEBUG)
else()
	set(RELEASE 1)
	set(RUNTIME_OUTPUT_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE})
	add_definitions(-D_RELEASE)
endif()


# GLFW
if(COMPILE_GLFW)
	set(BUILD_SHARED_LIBS ON CACHE BOOL "" FORCE)

	set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
	
	set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
	set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
	set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
	
	add_subdirectory("${PROJECT_SOURCE_DIR}/src/v4d/xvk/glfw")
else()
	find_package(glfw3 3.3 REQUIRED)
endif()


# Bullet3
if(COMPILE_BULLET)
	set(USE_DOUBLE_PRECISION ON CACHE BOOL "" FORCE)
	set(BUILD_SHARED_LIBS ON CACHE BOOL "" FORCE)
	
	set(BUILD_BULLET3 OFF CACHE BOOL "" FORCE)
	set(BUILD_EGL OFF CACHE BOOL "" FORCE)
	set(BUILD_ENET OFF CACHE BOOL "" FORCE)
	set(BUILD_PYBULLET OFF CACHE BOOL "" FORCE)
	set(BUILD_OPENGL3_DEMOS OFF CACHE BOOL "" FORCE)
	set(BUILD_BULLET2_DEMOS OFF CACHE BOOL "" FORCE)
	set(BUILD_EXTRAS OFF CACHE BOOL "" FORCE)
	set(BUILD_UNIT_TESTS OFF CACHE BOOL "" FORCE)
	
	set(INSTALL_LIBS OFF CACHE BOOL "" FORCE)
	set(USE_MSVC_RUNTIME_LIBRARY_DLL OFF CACHE BOOL "" FORCE)
	set(USE_GRAPHICAL_BENCHMARK OFF CACHE BOOL "" FORCE)
	set(USE_SOFT_BODY_MULTI_BODY_DYNAMICS_WORLD OFF CACHE BOOL "" FORCE)
	set(BUILD_CPU_DEMOS OFF CACHE BOOL "" FORCE)
	set(USE_GLUT OFF CACHE BOOL "" FORCE)
	SET(INTERNAL_CREATE_MSVC_RELATIVE_PATH_PROJECTFILES OFF CACHE BOOL "" FORCE)
	SET(INTERNAL_ADD_POSTFIX_EXECUTABLE_NAMES OFF CACHE BOOL "" FORCE)

	add_subdirectory("${PROJECT_SOURCE_DIR}/src/bullet3")
else()
	# find_package(BulletDynamics 2.89 REQUIRED)
	# find_package(LinearMath 2.89 REQUIRED)
	# find_package(BulletCollision 2.89 REQUIRED)
endif()


# V4D Core
if(COMPILE_V4D_CORE)
	add_subdirectory(src/v4d/core)
endif()


# V4D Modules
if(COMPILE_V4D_MODULES)
	add_subdirectory(src/v4d/modules)
endif()


# Assets
if(COMPILE_ASSETS)
	add_subdirectory(tools/assetcompiler)
endif()


# Demo / Project
add_executable(${APP_NAME}
	"${PROJECT_SOURCE_DIR}/src/main.cpp"
)
target_link_libraries(${APP_NAME}
	v4d
)
target_compile_definitions(${APP_NAME}
	PRIVATE -D_V4D_PROJECT
)
set_target_properties(${APP_NAME} PROPERTIES COMPILE_FLAGS ${BUILD_FLAGS})


# Unit Tests
if(COMPILE_TESTS)
	add_executable(tests
		"${PROJECT_SOURCE_DIR}/src/tests.cxx"
	)
	target_link_libraries(tests
		v4d
		# BulletDynamics
		# LinearMath
		# BulletCollision
	)
	target_compile_definitions(tests
		PRIVATE -D_V4D_PROJECT
	)
	# target_include_directories(v4d
	# 	PUBLIC
	# 		"${PROJECT_SOURCE_DIR}/src/bullet3/src/"
	# )
	# ADD_DEFINITIONS( -DBT_USE_DOUBLE_PRECISION)
	set_target_properties(tests PROPERTIES COMPILE_FLAGS "-W -Wno-cast-function-type")
endif()
