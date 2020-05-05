#include <v4d.h>

using namespace v4d::modules;

ModuleInstance::ModuleInstance(SharedLibraryInstance* libInstance, const std::string& sysName, v4d_core_weak v4dCore) {
	name = sysName;
	path = libInstance->path;
	handle = libInstance->handle;
	loaded = __V4D_LoadModuleFunctions();
	if (loaded) {
		fullModuleID = __V4D_GetFullModuleID();
		moduleName = __V4D_GetModuleName();
		moduleDescription = __V4D_GetModuleDescription();
		moduleRevision = __V4D_GetModuleRevision();
		__V4D_InitModule(v4dCore.lock().get());
		if (V4D_ModuleCreate) V4D_ModuleCreate();
		submodules = __V4D_GetSubmodules();
		loadedModules[fullModuleID] = this;
	}
}

ModuleInstance::~ModuleInstance() {
	if (loaded) {
		loadedModules[fullModuleID] = nullptr;
		if (V4D_ModuleDestroy) V4D_ModuleDestroy();
	}
}

///////////////////////////////////////////////////////////

ModulesLoader::~ModulesLoader() {
	std::lock_guard<std::mutex> lock(loadedModulesMutex);
	for (auto [name, sys] : loadedModules) {
		delete sys;
	}
}

ModuleInstance* ModulesLoader::Load(const std::string& sysName) {
	std::lock_guard<std::mutex> lock(loadedModulesMutex);
	if (loadedModules.find(sysName) == loadedModules.end()) {
		std::string libName = GetLibName(sysName);
		auto libInstance = sharedLibraryLoader.Load(libName, std::string("modules/") + sysName + "/" + sysName);
		if (!libInstance) {
			return nullptr;
		}
		
		LOAD_DLL_FUNC(libInstance, std::string, __V4D_GetCoreBuildVersion)
		if (!__V4D_GetCoreBuildVersion) {
			LOG_ERROR("Error getting symbol pointer for __V4D_GetCoreBuildVersion. " << LOAD_DLL_ERR)
			sharedLibraryLoader.Unload(libName);
			return nullptr;
		}

		std::string moduleV4dVersion = __V4D_GetCoreBuildVersion();
		if (moduleV4dVersion != V4D_VERSION) {
			LOG_ERROR("V4D Core Libs version mismatch (App:" << V4D_VERSION << " != Module:" << moduleV4dVersion << ")")
			sharedLibraryLoader.Unload(libName);
			return nullptr;
		}

		ModuleInstance* moduleInstance = new ModuleInstance(libInstance, sysName, v4dCore);
		if (!moduleInstance->IsLoaded()) {
			delete moduleInstance;
			sharedLibraryLoader.Unload(libName);
			return nullptr;
		}

		loadedModules.emplace(sysName, moduleInstance);
		return moduleInstance;
	}
	return loadedModules[sysName];
}

void ModulesLoader::Unload(const std::string& sysName) {
	std::lock_guard<std::mutex> lock(loadedModulesMutex);
	if (loadedModules.find(sysName) != loadedModules.end()) {
		delete loadedModules[sysName];
		loadedModules.erase(sysName);
		sharedLibraryLoader.Unload(GetLibName(sysName));
	}
}

void ModulesLoader::Reload(const std::string& sysName) {
	std::lock_guard<std::mutex> lock(loadedModulesMutex);
	if (loadedModules.find(sysName) != loadedModules.end()) {
		Unload(sysName);
		Load(sysName);
	}
}


bool ModuleInstance::IsLoaded() const {
	return loaded;
}

std::vector<void*>& ModuleInstance::GetSubmodules (SUBMODULE_TYPE_T submoduleType) {
	static std::vector<void*> emptyResult {}; //TODO find a better/safer way to do this
	if (submodules) {
		try {
			return submodules->at(submoduleType);
		} catch (...) {}
	}
	return emptyResult;
}

ModuleInstance* ModuleInstance::Get(MODULE_ID_T moduleID) {
	try {
		return loadedModules.at(moduleID);
	} catch (...) {}
	return nullptr;
}

V4DModules& ModuleInstance::GetLoadedModules() {
	return loadedModules;
}

V4DModules ModuleInstance::loadedModules {};
