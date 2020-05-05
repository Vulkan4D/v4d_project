set(ModuleName "test")

file(GLOB_RECURSE ModuleSourceFiles "${CMAKE_CURRENT_SOURCE_DIR}/${ModuleName}/**.cpp")

add_library(${ModuleName} MODULE 
	${ModuleSourceFiles}
)

target_link_libraries(${ModuleName}
	v4d
)

target_include_directories(${ModuleName}
	PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}"
)
