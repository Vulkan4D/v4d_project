#pragma once

namespace v4d::modules {
	
	// SubModule categories/types
	const SUBMODULE_TYPE_T SUBMODULE_TYPE_UNSUPPORTED	= 0x00000000LL << 32;
	// const SUBMODULE_TYPE_T SUBMODULE_TYPE_MISC			= 0x00000001LL << 32;
	// const SUBMODULE_TYPE_T SUBMODULE_TYPE_CORE			= 0x00000002LL << 32;
	// const SUBMODULE_TYPE_T SUBMODULE_TYPE_PROCESSING	= 0x00000004LL << 32;
	// const SUBMODULE_TYPE_T SUBMODULE_TYPE_NETWORKING	= 0x00000008LL << 32;
	const SUBMODULE_TYPE_T SUBMODULE_TYPE_GRAPHICS		= 0x00000010LL << 32;
	// const SUBMODULE_TYPE_T SUBMODULE_TYPE_AUDIO			= 0x00000020LL << 32;
	// const SUBMODULE_TYPE_T SUBMODULE_TYPE_DATA			= 0x00000040LL << 32;
	const SUBMODULE_TYPE_T SUBMODULE_TYPE_TEST			= 0x00000080LL << 32;
	const SUBMODULE_TYPE_T SUBMODULE_TYPE_INPUT			= 0x00000100LL << 32;
	// const SUBMODULE_TYPE_T SUBMODULE_TYPE_WINDOW		= 0x00000200LL << 32;
	// const SUBMODULE_TYPE_T SUBMODULE_TYPE_PHYSICS		= 0x00000400LL << 32;
	// const SUBMODULE_TYPE_T SUBMODULE_TYPE_IO			= 0x00000800LL << 32;
	// const SUBMODULE_TYPE_T SUBMODULE_TYPE_LAUNCHER		= 0x00001000LL << 32;

}

#define V4D_DEFINE_SUBMODULE(submodule) friend ModuleInstance; inline static SUBMODULE_TYPE_T TYPE() {return submodule;}

// Base Submodule Classes
#include "submodules/Test.hpp"
#include "submodules/Rendering.hpp"
#include "submodules/Input.hpp"

namespace v4d::modules {
	template<class SubmoduleType>
	std::vector<SubmoduleType*> GetSubmodules() {
		std::vector<SubmoduleType*> submodules {};
		for (auto[id, module] : v4d::modules::ModuleInstance::GetLoadedModules()) {
			for (void* submodule : module->GetSubmodules(SubmoduleType::TYPE())) {
				submodules.push_back((SubmoduleType*)submodule);
			}
		}
		return submodules;
	}
}

#ifdef _V4D_MODULE

	// Max number of vendors : 4 billion
	// Max number of modules per vendor : 4 billion

	#define V4D_MODULE_ID(ven,mod) ((((v4d::modules::MODULE_ID_T)ven)<<32) | (((v4d::modules::MODULE_ID_T)mod)<<16))
	
	#define V4D_LOAD_SUBMODULE(type, ...) (type*)v4dSubmodules[type::TYPE()].emplace_back(new type(__VA_ARGS__));
	#define V4D_UNLOAD_SUBMODULES(type) {for (auto* submodule : v4dSubmodules[type::TYPE()]) delete (type*)submodule;}
	
	// Initial source code for all V4D Modules (Which will all be compiled as individual dlls loaded at runtime, also used for modding/plugins)

	v4d::Core* v4dCore; //TODO implement some kind of v4dInterface... Because this is somewhat dangerous... 

	v4d::modules::V4DSubmodules v4dSubmodules {};

	#ifdef THIS_MODULE_VENDOR_ID
		V4DMODULE int __V4D_GetModuleVendorID() {
			return THIS_MODULE_VENDOR_ID;
		}
		V4DMODULE int __V4D_GetModuleID() {
			return THIS_MODULE_ID;
		}
		V4DMODULE v4d::modules::MODULE_ID_T __V4D_GetFullModuleID() {
			return V4D_MODULE_ID(THIS_MODULE_VENDOR_ID, THIS_MODULE_ID);
		}
		V4DMODULE std::string __V4D_GetCoreBuildVersion() {
			return V4D_VERSION;
		}
		V4DMODULE std::string __V4D_GetModuleName() {
			return THIS_MODULE_NAME;
		}
		V4DMODULE int __V4D_GetModuleRevision() {
			return THIS_MODULE_REV;
		}
		V4DMODULE std::string __V4D_GetModuleDescription() {
			return THIS_MODULE_DESCRIPTION;
		}
		V4DMODULE void __V4D_InitModule(v4d::Core* instance) {
			v4dCore = instance;
		}
		V4DMODULE v4d::modules::V4DSubmodules* __V4D_GetSubmodules() {
			return &v4dSubmodules;
		}
		
	#endif
#endif
