#include <vector>
#include <stdexcept>
#include <iostream>

#include "xvk.hpp"
#include "GLFW/glfw3.h"

#define VULKAN_API_VERSION VK_API_VERSION_1_1
#define ENGINE_NAME "xvk"
#define ENGINE_VERSION VK_MAKE_VERSION(1, 0, 0)
#define APPLICATION_NAME "xvk Sample Vulkan Application"
#define APPLICATION_VERSION VK_MAKE_VERSION(1, 0, 0)

std::vector<const char*> vulkanRequiredExtensions = {
	// extensions from glfw for creating a window are automatically added to this list
	#ifdef _DEBUG
	VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
	#endif
};
std::vector<const char*> vulkanRequiredLayers = {
	#ifdef _DEBUG
	"VK_LAYER_LUNARG_standard_validation",
	#endif
};

int main() {
	xvk::Loader vulkanLoader;
	xvk::Instance vulkanInstance;
	xvk::Device vulkanDevice;
	uint32_t vulkanQueueFamilyIndex;
	VkQueue vulkanGraphicsQueue;
	VkSurfaceKHR vulkanSurface;
	
	// init GLFW
	if (!glfwInit()) {
		throw std::runtime_error("glfw init failed");
	}

	// Check for vulkan support
	if (!glfwVulkanSupported()) {
		throw std::runtime_error("Vulkan not supported");
	}
	
	// Required vulkan extensions for GLFW
	uint32_t glfwExtensionCount = 0;
	const char** glfwExtensions;
	glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
	vulkanRequiredExtensions.reserve(glfwExtensionCount);
	for (uint32_t i = 0; i < glfwExtensionCount; i++) {
		vulkanRequiredExtensions.push_back(glfwExtensions[i]);
	}

	// Prepare structs for the Vulkan Instance
	VkApplicationInfo appInfo{};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = APPLICATION_NAME;
	appInfo.applicationVersion = APPLICATION_VERSION;
	appInfo.pEngineName = ENGINE_NAME;
	appInfo.engineVersion = ENGINE_VERSION;
	appInfo.apiVersion = VULKAN_API_VERSION;

	VkInstanceCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	createInfo.pApplicationInfo = &appInfo;
	createInfo.enabledExtensionCount = (uint32_t)vulkanRequiredExtensions.size();
	createInfo.ppEnabledExtensionNames = vulkanRequiredExtensions.data();
	createInfo.enabledLayerCount = (uint32_t)vulkanRequiredLayers.size();
	createInfo.ppEnabledLayerNames = vulkanRequiredLayers.data();
	
	// Load Vulkan library
	if (!vulkanLoader()) {
		throw std::runtime_error("Failed to load vulkan library");
		return 1;
	}
	
	// Create the Vulkan instance
	vulkanInstance(&vulkanLoader, &createInfo);

	// Get physical devices
	uint32_t nbPhysicalDevices = 0;
	if ((vulkanInstance.EnumeratePhysicalDevices(&nbPhysicalDevices, nullptr)) != VK_SUCCESS) {
		throw std::runtime_error("Failed to enumerate physical devices");
	}
	std::vector<VkPhysicalDevice> physicalDevices(nbPhysicalDevices);
	if ((vulkanInstance.EnumeratePhysicalDevices(&nbPhysicalDevices, physicalDevices.data())) != VK_SUCCESS) {
		throw std::runtime_error("Failed to enumerate physical devices");
	}
	
	// Select a suitable physical device
	VkPhysicalDevice selectedPhysicalDevice = VK_NULL_HANDLE;
	uint32_t selectedQueueFamilyIndex = 0;
	for (auto physicalDevice : physicalDevices) {
		// Check device properties
		VkPhysicalDeviceProperties deviceProperties;
		VkPhysicalDeviceFeatures deviceFeatures;
		vulkanInstance.GetPhysicalDeviceProperties(physicalDevice, &deviceProperties);
		vulkanInstance.GetPhysicalDeviceFeatures(physicalDevice, &deviceFeatures);
		if (deviceProperties.limits.maxImageDimension2D < 4096) continue;
		// Check device features
		uint32_t queueFamiliesCount = 0;
		vulkanInstance.GetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamiliesCount, nullptr);
		if (queueFamiliesCount == 0) continue;
		std::vector<VkQueueFamilyProperties> queueFamilyProperties(queueFamiliesCount);
		vulkanInstance.GetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamiliesCount, queueFamilyProperties.data());
		selectedQueueFamilyIndex = 0;
		for (auto queueFamilyProperty : queueFamilyProperties) {
			if (queueFamilyProperty.queueCount > 0 && queueFamilyProperty.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
				
				// Check for presentation support (Using glfw for checking extensions in a crossplatform way)
				if (!glfwGetPhysicalDevicePresentationSupport(vulkanInstance.handle, physicalDevice, selectedQueueFamilyIndex)) {
					continue; // Device not suitable, keep looking for the one !
				}
				
				// All is good, use that device !
				selectedPhysicalDevice = physicalDevice;
				break;
			}
			selectedQueueFamilyIndex++;
		}
	}
	if (selectedPhysicalDevice == VK_NULL_HANDLE) {
		throw std::runtime_error("Failed to find a suitable physical device");
	}
	vulkanQueueFamilyIndex = selectedQueueFamilyIndex;

	// Create logical device
	std::vector<float> queuePriorities = {1.0};
	VkDeviceQueueCreateInfo queueCreateInfo = {
		VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
		nullptr, // pNext
		0, // flags,
		selectedQueueFamilyIndex,
		(uint32_t)queuePriorities.size(),
		queuePriorities.data()
	};
	VkDeviceCreateInfo deviceCreateInfo = {
		VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
		nullptr, // pNext
		0, // flags
		1, // queueCreateInfoCount
		&queueCreateInfo, // can be an array to create a logical device with multiple queues
		0, // enabled layer count
		nullptr, // array enabled layer names
		0, // enabled extensions count
		nullptr, // array enabled extensions names
		nullptr // enabled features
	};
	
	// Create the actual vulkan device
	vulkanDevice(&vulkanInstance, selectedPhysicalDevice, &deviceCreateInfo);
	
	// Get graphics queue
	vulkanDevice.GetDeviceQueue(vulkanQueueFamilyIndex, 0/*queue index for this queue family*/, &vulkanGraphicsQueue);
	
	///////////////////////////////////////////////////
	// Everything Below this line is specific to GLFW
	
	// Create Window/Surface
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	GLFWwindow* window = glfwCreateWindow(800, 600, "TEST", nullptr, nullptr);
	if (glfwCreateWindowSurface(vulkanInstance.handle, window, nullptr, &vulkanSurface) != VK_SUCCESS) {
		throw std::runtime_error("Failed to create vulkan surface through GLFW");
	}
	
	// Input Events
	glfwSetKeyCallback(window, [](GLFWwindow* window, int key, int /*scancode*/, int action, int /*mods*/){
		if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
			glfwSetWindowShouldClose(window, 1);
		}
	});
	
	// Running...
	while (!glfwWindowShouldClose(window)) {
		glfwPollEvents();
		
		// Rendering happens here !!
		
	}
	
	// Cleanup
	vulkanInstance.DestroySurfaceKHR(vulkanSurface, nullptr);
	glfwDestroyWindow(window);
	glfwTerminate();
	
	std::cout << "Success !" << std::endl;
	return 0;
}
