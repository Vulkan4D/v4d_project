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

using namespace std;

// Debug Callback
#ifdef _DEBUG
VkDebugUtilsMessengerEXT vulkanCallbackExtFunction;
static VKAPI_ATTR VkBool32 VKAPI_CALL vulkanDebugCallback(
	VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
	VkDebugUtilsMessageTypeFlagsEXT messageType,
	const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
	void* /*pUserData*/) {
		string type;
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
				LOG_VERBOSE("VULKAN_VERBOSE"<<type<<": " << pCallbackData->pMessage);
			break;
			case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT: // Informational message like the creation of a resource
				LOG_VERBOSE("VULKAN_INFO"<<type<<": " << pCallbackData->pMessage);
			break;
			case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT: // Message about behavior that is not necessarily an error, but very likely a bug in your application
				LOG_WARN("VULKAN_WARNING"<<type<<": " << pCallbackData->pMessage);
			break;
			default:
			case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT: // Message about behavior that is invalid and may cause crashes
				LOG_ERROR("VULKAN_ERROR"<<type<<": " << pCallbackData->pMessage);
			break;
		}
		return VK_FALSE; // The callback returns a boolean that indicates if the Vulkan call that triggered the validation layer message should be aborted. If the callback returns true, then the call is aborted with the VK_ERROR_VALIDATION_FAILED_EXT error. This is normally only used to test the validation layers themselves, so you should always return VK_FALSE.
}
VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pCallback) {
	auto func = (PFN_vkCreateDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
	if (func != nullptr) {
		return func(instance, pCreateInfo, pAllocator, pCallback);
	} else {
		return VK_ERROR_EXTENSION_NOT_PRESENT;
	}
}
void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT callback, const VkAllocationCallbacks* pAllocator) {
	auto func = (PFN_vkDestroyDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
	if (func != nullptr) {
		func(instance, callback, pAllocator);
	}
}
static void VulkanCreateDebugCallback(VkInstance instance) {
	VkDebugUtilsMessengerCreateInfoEXT createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
	#ifdef _LOG_VERBOSE
	createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	#else
	createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	#endif
	createInfo.pfnUserCallback = vulkanDebugCallback;
	createInfo.pUserData = nullptr; // Optional
	if (CreateDebugUtilsMessengerEXT(instance, &createInfo, nullptr, &vulkanCallbackExtFunction) != VK_SUCCESS) {
		LOG_ERROR("Failed to set Vulkan Debug Callback Function");
	}
}
#endif


class Vulkan {
private:
	vector<VulkanGPU*> availableGPUs;

	void CheckExtensions() {
		LOG("Initializing Vulkan Extensions...");
		// Get supported extensions
		uint supportedExtensionCount = 0;
		vkEnumerateInstanceExtensionProperties(nullptr, &supportedExtensionCount, nullptr);
		vector<VkExtensionProperties> supportedExtensions(supportedExtensionCount);
		vkEnumerateInstanceExtensionProperties(nullptr, &supportedExtensionCount, supportedExtensions.data());
		for_each(supportedExtensions.begin(), supportedExtensions.end(), [](const auto& extension){
			LOG_VERBOSE("Supported Extension: " << extension.extensionName);
		});
		// Check support for required extensions
		for (const char* extensionName : vulkanRequiredExtensions) {
			if (find_if(supportedExtensions.begin(), supportedExtensions.end(), [&extensionName](VkExtensionProperties extension){
				return strcmp(extension.extensionName, extensionName) == 0;
			}) != supportedExtensions.end()) {
				LOG("Enabling Extension: " << extensionName);
			} else {
				throw runtime_error(string("Required Extension Not Supported: ") + extensionName);
			}
		}
	}

	void CheckLayers() {
		LOG("Initializing Vulkan Layers...");
		// Get Supported Layers
		uint supportedLayerCount;
		vkEnumerateInstanceLayerProperties(&supportedLayerCount, nullptr);
		vector<VkLayerProperties> supportedLayers(supportedLayerCount);
		vkEnumerateInstanceLayerProperties(&supportedLayerCount, supportedLayers.data());
		for_each(supportedLayers.begin(), supportedLayers.end(), [](const auto& layer){
			LOG_VERBOSE("Supported Layer: " << layer.layerName);
		});
		// Check support for required layers
		for (const char* layerName : vulkanRequiredLayers) {
			if (find_if(supportedLayers.begin(), supportedLayers.end(), [&layerName](VkLayerProperties layer){
				return strcmp(layer.layerName, layerName) == 0;
			}) != supportedLayers.end()) {
				LOG("Enabling Layer: " << layerName);
			} else {
				throw runtime_error(string("Layer Not Supported: ") + layerName);
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
			throw runtime_error("Vulkan Version " + to_string(vMajor) + "." + to_string(vMinor) + "." + to_string(vPatch) + " is Not supported");
		}
	}

	void LoadAvailableGPUs() {
		LOG("Initializing Physical Devices...");
		// Get Devices List
		uint deviceCount = 0;
		vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
		if (deviceCount == 0) throw runtime_error("Failed to find a GPU with Vulkan Support");
		vector<VkPhysicalDevice> devices(deviceCount);
		vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());
		// Add detected devices to our own physical device list
		availableGPUs.reserve(deviceCount);
		for (const auto& physicalDevice : devices) {
			VkPhysicalDeviceProperties properties = {};
			vkGetPhysicalDeviceProperties(physicalDevice, &properties);
			if (properties.apiVersion >= VULKAN_API_VERSION)
				availableGPUs.push_back(new VulkanGPU(physicalDevice));
		}
		// Error if no GPU was found
		if (availableGPUs.size() == 0) {
			throw runtime_error("No suitable GPU was found for required vulkan version");
		}
	}

protected:
	VkInstance instance;

public:
	Vulkan() {
		CheckExtensions();
		CheckLayers();
		CheckVulkanVersion();

		// Prepare structs for the Vulkan Instance
		VkApplicationInfo appInfo = {};
		appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		appInfo.pApplicationName = APPLICATION_NAME;
		appInfo.applicationVersion = APPLICATION_VERSION;
		appInfo.pEngineName = ENGINE_NAME;
		appInfo.engineVersion = ENGINE_VERSION;
		appInfo.apiVersion = VULKAN_API_VERSION;

		VkInstanceCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		createInfo.pApplicationInfo = &appInfo;
		createInfo.enabledExtensionCount = (uint)vulkanRequiredExtensions.size();
		createInfo.ppEnabledExtensionNames = vulkanRequiredExtensions.data();
		createInfo.enabledLayerCount = (uint)vulkanRequiredLayers.size();
		createInfo.ppEnabledLayerNames = vulkanRequiredLayers.data();

		// Create the Vulkan instance

		// #ifdef _VULKAN_LOAD_AT_RUNTIME
		// 	static PFN_vkCreateInstance vkCreateInstance = (PFN_vkCreateInstance) glfwGetInstanceProcAddress(NULL, "vkCreateInstance");
		// #endif

		if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS)
			throw runtime_error("Failed to create Vulkan Instance");

		// Debug Callback
		#ifdef _DEBUG
		VulkanCreateDebugCallback(instance);
		#endif

		// Load Physical Devices
		LoadAvailableGPUs();

		LOG("VULKAN INSTANCE CREATED!");
	}

	virtual ~Vulkan() {
		#ifdef _DEBUG
		DestroyDebugUtilsMessengerEXT(instance, vulkanCallbackExtFunction, nullptr);
		#endif
		vkDestroyInstance(instance, nullptr);
		for (auto *gpu : availableGPUs) delete gpu;
	}

	inline VkInstance GetInstance() const {
		return instance;
	}

	VulkanGPU* SelectSuitableGPU(const function<void(int&, VulkanGPU*)> &suitabilityFunc) {
		VulkanGPU *selectedGPU = nullptr;

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
		if (selectedGPU == nullptr) throw runtime_error("Failed to find a suitable GPU");
		return selectedGPU;
	}


	//////////////////////////////////////////////////////////////////////////////////////////////////////
	// vk* function pointers

	

};
