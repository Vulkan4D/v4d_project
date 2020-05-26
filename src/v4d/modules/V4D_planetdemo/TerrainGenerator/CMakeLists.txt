cmake_minimum_required(VERSION 3.10)

# Vulkan4D Standalone Module Build Template 0.4
###########################################################################################

# Paths
project(V4D_planetdemo_TerrainGenerator) # Any unique name here
set(V4D_PROJECT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../../..") # path to v4d project or template root (containing /build/ and /src/v4d/)
set(V4D_PROJECT_BUILD_DIR "${V4D_PROJECT_DIR}/build") # output files will be placed in <V4D_PROJECT_BUILD_DIR>/[debug|release]/modules/<ModuleVendor>_<ModuleName>/<ModuleVendor>_<ModuleName>.<ClassName>.[dll|so]

# Include Global CMAKE configuration
include("${V4D_PROJECT_DIR}/src/v4d/core/global_build_config.cmake")
###########################################################################################

ModuleVendor( V4D )
ModuleName( planetdemo )


SubModule("V4D_planetdemo.TerrainGenerator.cpp" TerrainGenerator)

target_include_directories(${_SubModuleName}
	PRIVATE
		"${V4D_PROJECT_DIR}/src/v4d/xvk/glm"
)
