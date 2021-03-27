# V4D Core
file(GLOB_RECURSE V4DGameSourceFiles **.cpp)
add_library(game SHARED 
	${V4DGameSourceFiles}
)
target_link_libraries(game
	PUBLIC pthread
)
target_include_directories(game
	PRIVATE
		"${CMAKE_CURRENT_SOURCE_DIR}"
		"${PROJECT_SOURCE_DIR}/src/v4d/core"
		"${PROJECT_SOURCE_DIR}/src/v4d/xvk"
)
target_compile_definitions(game
	PRIVATE -D_V4D_GAME
)
set_target_properties(game
	PROPERTIES
		COMPILE_FLAGS ${BUILD_FLAGS}
		LIBRARY_OUTPUT_DIRECTORY_DEBUG "${V4D_PROJECT_BUILD_DIR}/debug"
		RUNTIME_OUTPUT_DIRECTORY_DEBUG "${V4D_PROJECT_BUILD_DIR}/debug"
		LIBRARY_OUTPUT_DIRECTORY_RELEASE "${V4D_PROJECT_BUILD_DIR}/release"
		RUNTIME_OUTPUT_DIRECTORY_RELEASE "${V4D_PROJECT_BUILD_DIR}/release"
)

# Platform-specific libs
if(WIN32)
	target_link_libraries(game
		PUBLIC
			gcc
			stdc++
	)
else()
	target_link_libraries(game
		PUBLIC
			dl
	)
endif()
