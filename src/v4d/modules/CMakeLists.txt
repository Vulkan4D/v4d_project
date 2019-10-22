file(GLOB_RECURSE cmakeFiles **.cmake)
foreach (cmakeFile ${cmakeFiles})

	include(${cmakeFile})

	set_target_properties(${ModuleName}
		PROPERTIES
			LIBRARY_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/debug/modules/${ModuleName}"
			RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/debug/modules/${ModuleName}"
			LIBRARY_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/release/modules/${ModuleName}"
			RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/release/modules/${ModuleName}"
	)

endforeach()
