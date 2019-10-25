#pragma once

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>

#define DEFINE_VULKAN_INTERFACE_FUNCTION(func) \
	PFN_##func func = 0;
#define LOAD_VULKAN_GLOBAL_FUNCTION(func) \
	if (!(func = (PFN_##func) vkGetInstanceProcAddr(nullptr, #func))){ LOG_ERROR("Failed to load vulkan global function pointer for " << #func) }
#define LOAD_VULKAN_INSTANCE_FUNCTION(func) \
	if (!(func = (PFN_##func) loader->vkGetInstanceProcAddr(instance, #func))){ LOG_ERROR("Failed to load vulkan instance function pointer for " << #func) }
#define LOAD_VULKAN_DEVICE_FUNCTION(func) \
	if (!(func = (PFN_##func) instance->vkGetDeviceProcAddr(device, #func))){ LOG_ERROR("Failed to load vulkan device function pointer for " << #func) }

	
namespace VulkanLoader {
	class BaseLoader {
	protected:
		#ifdef _WINDOWS
			HMODULE vulkanLoader;
		#else
			void* vulkanLoader;
		#endif
	public:
		
		DEFINE_VULKAN_INTERFACE_FUNCTION( vkGetInstanceProcAddr )
		
		BaseLoader() {
			#ifdef _WINDOWS
				vulkanLoader = LoadLibrary("vulkan-1.dll");
			#else
				vulkanLoader = dlopen("libvulkan.so", RTLD_NOW);
			#endif
			if (vulkanLoader == nullptr) {
				LOG_ERROR("Failed to load vulkan loader library")
				return;
			}
			#ifdef _WINDOWS
				vkGetInstanceProcAddr = (PFN_vkGetInstanceProcAddr)GetProcAddress(vulkanLoader, "vkGetInstanceProcAddr");
			#else
				vkGetInstanceProcAddr = (PFN_vkGetInstanceProcAddr)dlsym(vulkanLoader, "vkGetInstanceProcAddr");
			#endif
		}
		~BaseLoader() {
			// Unload vulkan loader library
			#ifdef _WINDOWS
				FreeLibrary(vulkanLoader);
			#else
				dlclose(vulkanLoader);
			#endif
		}
	};
	
	class BaseInstance {
	public:
		VkInstance handle;
		
		DEFINE_VULKAN_INTERFACE_FUNCTION( vkGetDeviceProcAddr )
		
		BaseInstance(BaseLoader* loader, VkInstance& instance) : handle(instance) {
			LOAD_VULKAN_INSTANCE_FUNCTION( vkGetDeviceProcAddr )
		}
		
		// ~BaseInstance() {}
	};
	
	class BaseDevice {
	public:
		VkDevice handle;
		
		BaseDevice(BaseInstance*, VkDevice& device) : handle(device) {
			
		}
		
		// ~BaseDevice() {}
	};
}
