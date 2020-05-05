
function(ModuleVendor ven)
	set(_ModuleVendor "${ven}" PARENT_SCOPE)
endfunction()

function(ModuleName mod)
	set(_ModuleName "${mod}" PARENT_SCOPE)
endfunction()

function(SubModule _ModuleSourceFile _ModuleClass)
	set(_SubModuleName "${_ModuleVendor}_${_ModuleName}.${_ModuleClass}" PARENT_SCOPE)
	set(_SubModuleName "${_ModuleVendor}_${_ModuleName}.${_ModuleClass}")
	add_library(${_SubModuleName} MODULE "${_ModuleDir}/${_ModuleSourceFile}")
	target_link_libraries(${_SubModuleName} v4d)
	target_include_directories(${_SubModuleName} PRIVATE "${_ModuleDir}")
	set_target_properties(${_SubModuleName}
		PROPERTIES
			LIBRARY_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/debug/modules/${_ModuleVendor}_${_ModuleName}"
			RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/debug/modules/${_ModuleVendor}_${_ModuleName}"
			LIBRARY_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/release/modules/${_ModuleVendor}_${_ModuleName}"
			RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/release/modules/${_ModuleVendor}_${_ModuleName}"
	)
endfunction()

file(GLOB_RECURSE _modulesConfigFiles **/module.conf)
foreach (_moduleConfigFile ${_modulesConfigFiles})
	string(REGEX REPLACE "/module\.conf$" "" _ModuleDir ${_moduleConfigFile})
	include(${_moduleConfigFile})
endforeach()
