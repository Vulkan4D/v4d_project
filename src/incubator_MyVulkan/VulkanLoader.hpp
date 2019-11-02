#pragma once

#include <vector>

#define XVK_INTERFACE_RAW_FUNCTIONS_ACCESSIBILITY private
#include <xvk.hpp>

#define VULKAN_API_VERSION VK_API_VERSION_1_1
#define V4D_ENGINE_NAME "Vulkan4D"
#define V4D_ENGINE_VERSION VK_MAKE_VERSION(1, 0, 0)

class VulkanLoader : public xvk::Loader {
public:
	std::vector<const char*> requiredInstanceExtensions {
		#ifdef _DEBUG
		VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
		#endif
	};
	std::vector<const char*> requiredInstanceLayers {
		#ifdef _DEBUG
		"VK_LAYER_LUNARG_standard_validation",
		#endif
	};
	
	void CheckExtensions() {
		LOG("Initializing Vulkan Extensions...");
		
		// Get supported extensions
		uint supportedExtensionCount = 0;
		vkEnumerateInstanceExtensionProperties(nullptr, &supportedExtensionCount, nullptr);
		std::vector<VkExtensionProperties> supportedExtensions(supportedExtensionCount);
		vkEnumerateInstanceExtensionProperties(nullptr, &supportedExtensionCount, supportedExtensions.data());
		std::for_each(supportedExtensions.begin(), supportedExtensions.end(), [](const auto& extension){
			LOG_VERBOSE("Supported Extension: " << extension.extensionName);
		});
		
		// Check support for required extensions
		for (const char* extensionName : requiredInstanceExtensions) {
			if (std::find_if(supportedExtensions.begin(), supportedExtensions.end(), [&extensionName](VkExtensionProperties extension){
				return strcmp(extension.extensionName, extensionName) == 0;
			}) != supportedExtensions.end()) {
				LOG("Enabling Vulkan Extension: " << extensionName);
			} else {
				throw std::runtime_error(std::string("Required Extension Not Supported: ") + extensionName);
			}
		}
	}

	void CheckLayers() {
		LOG("Initializing Vulkan Layers...");
		
		// Get Supported Layers
		uint supportedLayerCount;
		vkEnumerateInstanceLayerProperties(&supportedLayerCount, nullptr);
		std::vector<VkLayerProperties> supportedLayers(supportedLayerCount);
		vkEnumerateInstanceLayerProperties(&supportedLayerCount, supportedLayers.data());
		std::for_each(supportedLayers.begin(), supportedLayers.end(), [](const auto& layer){
			LOG_VERBOSE("Supported Layer: " << layer.layerName);
		});
		
		// Check support for required layers
		for (const char* layerName : requiredInstanceLayers) {
			if (std::find_if(supportedLayers.begin(), supportedLayers.end(), [&layerName](VkLayerProperties layer){
				return strcmp(layer.layerName, layerName) == 0;
			}) != supportedLayers.end()) {
				LOG("Enabling Vulkan Layer: " << layerName);
			} else {
				throw std::runtime_error(std::string("Layer Not Supported: ") + layerName);
			}
		}
	}

	void CheckVulkanVersion() {
		uint32_t apiVersion = 0;
		vkEnumerateInstanceVersion(&apiVersion);
		if (apiVersion < VULKAN_API_VERSION) {
			uint vMajor = (VULKAN_API_VERSION & (511 << 22)) >> 22;
			uint vMinor = (VULKAN_API_VERSION & (1023 << 12)) >> 12;
			uint vPatch = VULKAN_API_VERSION & 4095;
			throw std::runtime_error("Vulkan Version " + std::to_string(vMajor) + "." + std::to_string(vMinor) + "." + std::to_string(vPatch) + " is Not supported");
		}
	}

};
