#pragma once
#include <v4d.h>

#ifdef _WINDOWS
	#define DLL_FILE_HANDLER HINSTANCE
#else
	#define DLL_FILE_HANDLER void*
#endif

namespace v4d::io {
	struct V4DLIB SharedLibraryInstance {
		std::string name;
		std::string path;
		std::string filePath {""};
		DLL_FILE_HANDLER handle = 0;

		SharedLibraryInstance(){}
		SharedLibraryInstance(const std::string& name, const std::string& path);
		~SharedLibraryInstance();
	};

	class V4DLIB SharedLibraryLoader {
	private:
		std::recursive_mutex loadedLibrariesMutex;
		std::unordered_map<std::string, SharedLibraryInstance*> loadedLibraries {};

	public:
	
		SharedLibraryInstance* Load(const std::string& name, std::string path = "");
		void Unload(const std::string& name);
		SharedLibraryInstance* Reload(const std::string& name);
		~SharedLibraryLoader();
	};
}

#ifdef _WINDOWS
	#define LOAD_DLL_FUNC(instance, returntype, funcName, ...) \
		returntype (*funcName)(__VA_ARGS__); \
		*(void **)(&funcName) = (void*)GetProcAddress(instance->handle, #funcName);
#else// LINUX
	#define LOAD_DLL_FUNC(instance, returntype, funcName, ...) \
		returntype (*funcName)(__VA_ARGS__); \
		*(void **)(&funcName) = dlsym(instance->handle, #funcName);
#endif

#ifdef _WINDOWS
	#define LOAD_DLL_ERR \
		 GetLastError()
#else// LINUX
	#define LOAD_DLL_ERR \
		dlerror()
#endif
