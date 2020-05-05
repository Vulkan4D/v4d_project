/**
 * V4D Module Sample
 * 
 * This is a demonstration code for implementation of a Module for V4D
 * Modules are compiled into individual libraries (DLL) which will be loaded at runtime into the main application.
 * All modules MUST be compiled in 64bit and MUST have the following MACROS defined in compiler flags : 
 * 		_RELEASE or _DEBUG
 * 		_WINDOWS or _LINUX
 * 
 * All modules must define the required metadata and include v4d.h
 * 
 * The keyword V4DMODULE is a macro to put in front of every function that should be available to the main application. 
 * 
 * To load a module into the main application, first instantiate a ModulesLoader or use a global one : 
		v4d::modules::ModulesLoader modulesLoader;
 * 	Then load the module and check if it has been loaded successfully :
		auto sampleModule = modulesLoader.Load("sample");
		or
		auto sampleModule = v4dCore->LoadModule("sample");

		if (!sampleModule) LOG_ERROR("ERROR : Could not load sample module library")
 * 
 * Modules can include predefined optional functions and custom functions
 * 
 * To call a predefined function, simply call it using the instance pointer : 
 * 		sampleModule->V4D_ModuleCreate();
 * 
 * To call a custom function from a module, first get the function pointer using the Macro and check if it is valid : 
		LOAD_DLL_FUNC(sampleModule, int, test1, int)
		if (!test1) LOG_ERROR("Error getting symbol pointer. " << LOAD_DLL_ERR)
 * 	The first parameter is the instance of the module (the return value of ModulesLoader.Load())
 * 	The second parameter is the return type of the function we want to load
 * 	The 3rd parameter is the name of the function we are trying to load, and also the resulting function pointer name
 * 	All the remaining parameters are the argument types of that function
 * 	Error message can be fetched using the macro LOAD_DLL_ERR
 *  Then, we can simply call the function like so : 
 * 		int x = test1(5);
 * 
 * Predefined accessible module members are :
	v4d::Core* v4dCore;
	V4DSubmodules v4dSubmodules; // map of id=>void*
 */

// Metadata
#define _V4D_MODULE
#define THIS_MODULE_VENDOR_ID 123
#define THIS_MODULE_ID 12345
#define THIS_MODULE_REV 1
#define THIS_MODULE_NAME "Sample"
#define THIS_MODULE_DESCRIPTION "Sample V4D Module"

// V4D Core Header
#include <v4d.h>
using namespace v4d::modules;

// Submodules
#include "TestSubmodule.hpp"

////////////////////////////////////////////////////////////////////
// Predefined optional functions

V4DMODULE void V4D_ModuleCreate() {
	V4D_LOAD_SUBMODULE(TestSubmodule, 44)
}

V4DMODULE void V4D_ModuleDestroy() {
	V4D_UNLOAD_SUBMODULES(TestSubmodule)
}

////////////////////////////////////////////////////////////////////
// Example of a custom module function that can be accessed via the main application or other modules
V4DMODULE int test1(int b) {
	if (v4dCore->GetProjectName() == "Test Project") {
		return b * 2;
	} else {
		return 0;
	}
}

