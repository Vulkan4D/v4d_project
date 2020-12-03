#pragma once
#include "app.hh"

namespace app::vulkan {
	
	void Load() {
		app::vulkanLoader = new v4d::graphics::vulkan::Loader;
		
		// Validation layers
		#if defined(_DEBUG) && defined(_LINUX)
			app::vulkanLoader->requiredInstanceLayers.push_back("VK_LAYER_KHRONOS_validation");
		#endif
		
		// Load the Vulkan driver
		if (!(*app::vulkanLoader)()) 
			throw std::runtime_error("Failed to load Vulkan library");
		
		// Needed for RayTracing
		app::vulkanLoader->requiredInstanceExtensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
	}
	
	void Unload() {
		delete app::vulkanLoader;
	}
	
}
