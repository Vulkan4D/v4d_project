#include <v4d.h>

using namespace v4d::io;

SharedLibraryInstance::SharedLibraryInstance(const std::string& name, const std::string& path) : name(name), path(path) {
	#ifdef _WINDOWS
		filePath = path + ".dll";
	#else// LINUX
		filePath = path + ".so";
	#endif
	
	// Use new file if exists
	if (FilePath::FileExists(filePath+".new")) {
		#ifdef _WINDOWS
			// QuickFix for windows.. because 'overwrite_existing' option is not working... we get the 'file exists' error... WTF...
			std::filesystem::remove(filePath);
			std::filesystem::copy_file(filePath+".new", filePath);
		#else
			std::filesystem::copy_file(filePath+".new", filePath, std::filesystem::copy_options::overwrite_existing);
		#endif
		std::filesystem::remove(filePath+".new");
	}
	
	// Load the library
	#ifdef _WINDOWS
		this->handle = LoadLibrary(filePath.c_str());
		auto err = GetLastError();
	#else// LINUX
		this->handle = dlopen(filePath.c_str(), RTLD_LAZY); // or RTLD_NOW
		auto err = dlerror();
	#endif
	
	if (!handle) {
		LOG_ERROR("Error loading shared library " << name << " in '" << filePath << "' : " << err)
	}
}

SharedLibraryInstance::~SharedLibraryInstance() {
	if (handle) {
		#ifdef _WINDOWS
			FreeLibrary(handle);
		#else// LINUX
			dlclose(handle);
		#endif
	}
}

///////////////////////////////////////////////////////////

SharedLibraryInstance* SharedLibraryLoader::Load(const std::string& name, std::string path) {
	std::lock_guard lock(loadedLibrariesMutex);
	if (path == "") path = name;
	if (loadedLibraries.find(name) == loadedLibraries.end()) {
		SharedLibraryInstance* instance = new SharedLibraryInstance(name, path);
		if (!instance->handle) {
			delete instance;
			return nullptr;
		}
		loadedLibraries.emplace(name, instance);
	}
	return loadedLibraries[name];
}

SharedLibraryLoader::~SharedLibraryLoader() {
	std::lock_guard lock(loadedLibrariesMutex);
	for (auto [name, lib] : loadedLibraries) {
		delete lib;
	}
}

void SharedLibraryLoader::Unload(const std::string& name) {
	std::lock_guard lock(loadedLibrariesMutex);
	if (loadedLibraries.find(name) != loadedLibraries.end()) {
		delete loadedLibraries[name];
		loadedLibraries.erase(name);
	}
}

SharedLibraryInstance* SharedLibraryLoader::Reload(const std::string& name) {
	std::lock_guard lock(loadedLibrariesMutex);
	if (loadedLibraries.find(name) != loadedLibraries.end()) {
		std::string path = loadedLibraries[name]->path;
		Unload(name);
		return Load(name, path);
	}
	return nullptr;
}
