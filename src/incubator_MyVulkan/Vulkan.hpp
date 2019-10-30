#pragma once

// #include "vkfcn.hpp"
#include "VulkanStructs.hpp"
#include "VulkanGPU.hpp"
#include "VulkanDevice.hpp"
#include "VulkanSwapChain.hpp"
#include "VulkanSurface.hpp"
#include "VulkanShader.hpp"
#include "VulkanShaderProgram.hpp"
#include "VulkanGraphicsPipeline.hpp"
#include "VulkanRenderPass.hpp"

// Debug Callback
#ifdef _DEBUG
VkDebugUtilsMessengerEXT vulkanCallbackExtFunction;
static VKAPI_ATTR VkBool32 VKAPI_CALL vulkanDebugCallback(
	VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
	VkDebugUtilsMessageTypeFlagsEXT messageType,
	const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
	void* /*pUserData*/) {
		std::string type;
		switch (messageType) {
			default:
			case VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT: // Some event has happened that is unrelated to the specification or performance
				type = "";
			break;
			case VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT: // Something has happened that violates the specification or indicates a possible mistake
				type = "(validation)";
			break;
			case VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT: // Potential non-optimal use of Vulkan
				type = "(performance)";
			break;
		}
		switch (messageSeverity) {
			case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT: // Diagnostic message
				LOG_VERBOSE("VULKAN_VERBOSE" << type << ": " << pCallbackData->pMessage);
			break;
			case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT: // Informational message like the creation of a resource
				LOG_VERBOSE("VULKAN_INFO" << type << ": " << pCallbackData->pMessage);
			break;
			case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT: // Message about behavior that is not necessarily an error, but very likely a bug in your application
				LOG_WARN("VULKAN_WARNING" << type << ": " << pCallbackData->pMessage);
			break;
			default:
			case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT: // Message about behavior that is invalid and may cause crashes
				LOG_ERROR("VULKAN_ERROR" << type << ": " << pCallbackData->pMessage);
			break;
		}
		return VK_FALSE; // The callback returns a boolean that indicates if the Vulkan call that triggered the validation layer message should be aborted. If the callback returns true, then the call is aborted with the VK_ERROR_VALIDATION_FAILED_EXT error. This is normally only used to test the validation layers themselves, so you should always return VK_FALSE.
}
#endif

class Vulkan : public xvk::Interface::InstanceInterface {
private:
	std::vector<VulkanGPU*> availableGPUs;

	void CheckExtensions() {
		LOG("Initializing Vulkan Extensions...");
		
		// Get supported extensions
		uint supportedExtensionCount = 0;
		vulkanLoader->vkEnumerateInstanceExtensionProperties(nullptr, &supportedExtensionCount, nullptr);
		std::vector<VkExtensionProperties> supportedExtensions(supportedExtensionCount);
		vulkanLoader->vkEnumerateInstanceExtensionProperties(nullptr, &supportedExtensionCount, supportedExtensions.data());
		std::for_each(supportedExtensions.begin(), supportedExtensions.end(), [](const auto& extension){
			LOG_VERBOSE("Supported Extension: " << extension.extensionName);
		});
		
		// Check support for required extensions
		for (const char* extensionName : vulkanRequiredExtensions) {
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
		vulkanLoader->vkEnumerateInstanceLayerProperties(&supportedLayerCount, nullptr);
		std::vector<VkLayerProperties> supportedLayers(supportedLayerCount);
		vulkanLoader->vkEnumerateInstanceLayerProperties(&supportedLayerCount, supportedLayers.data());
		std::for_each(supportedLayers.begin(), supportedLayers.end(), [](const auto& layer){
			LOG_VERBOSE("Supported Layer: " << layer.layerName);
		});
		
		// Check support for required layers
		for (const char* layerName : vulkanRequiredLayers) {
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
		vulkanLoader->vkEnumerateInstanceVersion(&apiVersion);
		if (apiVersion < VULKAN_API_VERSION) {
			uint vMajor = (VULKAN_API_VERSION & (511 << 22)) >> 22;
			uint vMinor = (VULKAN_API_VERSION & (1023 << 12)) >> 12;
			uint vPatch = VULKAN_API_VERSION & 4095;
			throw std::runtime_error("Vulkan Version " + std::to_string(vMajor) + "." + std::to_string(vMinor) + "." + std::to_string(vPatch) + " is Not supported");
		}
	}

	void LoadAvailableGPUs() {
		LOG("Initializing Physical Devices...");
		
		// Get Devices List
		uint physicalDeviceCount = 0;
		EnumeratePhysicalDevices(&physicalDeviceCount, nullptr);
		if (physicalDeviceCount == 0) throw std::runtime_error("Failed to find a GPU with Vulkan Support");
		std::vector<VkPhysicalDevice> physicalDevices(physicalDeviceCount);
		EnumeratePhysicalDevices(&physicalDeviceCount, physicalDevices.data());
		
		// Add detected physicalDevices to our own physical device list
		availableGPUs.reserve(physicalDeviceCount);
		for (const auto& physicalDevice : physicalDevices) {
			VkPhysicalDeviceProperties properties = {};
			vkGetPhysicalDeviceProperties(physicalDevice, &properties);
			if (properties.apiVersion >= VULKAN_API_VERSION)
				availableGPUs.push_back(new VulkanGPU(this, physicalDevice));
		}
		
		// Error if no GPU was found
		if (availableGPUs.size() == 0) {
			throw std::runtime_error("No suitable GPU was found for required vulkan version");
		}
	}

protected:
	xvk::Loader* vulkanLoader;

public:
	Vulkan(xvk::Loader* loader, const char* applicationName, uint applicationVersion) : vulkanLoader(loader) {
		this->loader = loader;
		
		CheckExtensions();
		CheckLayers();
		CheckVulkanVersion();

		// Prepare appInfo for the Vulkan Instance
		VkApplicationInfo appInfo {
			VK_STRUCTURE_TYPE_APPLICATION_INFO,
			nullptr, //pNext
			applicationName,
			applicationVersion,
			V4D_ENGINE_NAME,
			V4D_ENGINE_VERSION,
			VULKAN_API_VERSION
		};

		// Create the Vulkan instance
		VkInstanceCreateInfo createInfo {
			VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
			nullptr, // pNext
			0, // flags
			&appInfo,
			(uint32_t)vulkanRequiredLayers.size(),
			vulkanRequiredLayers.data(),
			(uint32_t)vulkanRequiredExtensions.size(),
			vulkanRequiredExtensions.data()
		};

		// Create the Vulkan instance
		if (vulkanLoader->vkCreateInstance(&createInfo, nullptr, &handle) != VK_SUCCESS)
			throw std::runtime_error("Failed to create Vulkan Instance");
			
		LoadFunctionPointers();

		// Debug Callback
		#ifdef _DEBUG
			VkDebugUtilsMessengerCreateInfoEXT debugUtilsMessengerCreateInfo {
				VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
				nullptr,// pNext
				0,// flags
				VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, // messageSeverity
				VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT, // messageType
				vulkanDebugCallback,// pfnUserCallback
				nullptr,// pUserData
			};
			if (false) { // log verbose
				debugUtilsMessengerCreateInfo.messageSeverity |= VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT;
				debugUtilsMessengerCreateInfo.messageType |= VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT;
			}
			if (CreateDebugUtilsMessengerEXT(&debugUtilsMessengerCreateInfo, nullptr, &vulkanCallbackExtFunction) != VK_SUCCESS) {
				std::cout << "Failed to set Vulkan Debug Callback Function" << std::endl;
			}
		#endif

		// Load Physical Devices
		LoadAvailableGPUs();

		LOG("VULKAN INSTANCE CREATED!");
	}

	virtual ~Vulkan() {
		#ifdef _DEBUG
			DestroyDebugUtilsMessengerEXT(vulkanCallbackExtFunction, nullptr);
		#endif
		DestroyInstance(nullptr);
		for (auto *gpu : availableGPUs) delete gpu;
	}

	inline VkInstance GetHandle() const {
		return handle;
	}

	VulkanGPU* SelectSuitableGPU(const std::function<void(int&, VulkanGPU*)>& suitabilityFunc) {
		VulkanGPU* selectedGPU = nullptr;

		// Select Best MainGPU
		for (auto* gpu : availableGPUs) {
			static int bestScore = 0;
			int score = 0;

			suitabilityFunc(score, gpu);

			if (score > 0 && score > bestScore){
				selectedGPU = gpu;
				bestScore = score;
			}
		}

		// Make sure that all mandatory GPUs have been selected
		if (selectedGPU == nullptr) throw std::runtime_error("Failed to find a suitable GPU");
		return selectedGPU;
	}

};
