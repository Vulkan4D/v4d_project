#include <common/pch.hh>
#include "config.hh"
#include <v4d.h>

#include "incubator_CustomVkLoader/Vulkan.hpp"

// GLFW
#include <GLFW/glfw3.h>

#define VULKAN_API_VERSION VK_API_VERSION_1_1
#define ENGINE_NAME "Vulkan4D"
#define ENGINE_VERSION VK_MAKE_VERSION(1, 0, 0)
#define APPLICATION_NAME "V4D Vulkan Incubator"
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

// Global stuff
VkInstance vulkanInstance;
VkDevice vulkanDevice;
uint32_t vulkanQueueFamilyIndex;
VkQueue vulkanGraphicsQueue;
VkSurfaceKHR vulkanSurface;

int main() {
	
	// init GLFW
	if (!glfwInit()) {
		LOG_ERROR("glfw init failed")
		return 1;
	}

	// Check for vulkan support
	if (!glfwVulkanSupported()) {
		LOG_ERROR("Vulkan not supported")
		return 1;
	}
	
	// Required vulkan extensions for GLFW
	uint glfwExtensionCount = 0;
	const char** glfwExtensions;
	glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
	vulkanRequiredExtensions.reserve(glfwExtensionCount);
	for (uint i = 0; i < glfwExtensionCount; i++) {
		vulkanRequiredExtensions.push_back(glfwExtensions[i]);
	}

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

	Vulkan::Loader vulkanLoader;

	// Create the Vulkan instance
	if (vulkanLoader.vkCreateInstance(&createInfo, nullptr, &vulkanInstance) != VK_SUCCESS) {
		LOG_ERROR("Failed to create Vulkan Instance")
		return 1;
	}

	Vulkan::Instance instance(&vulkanLoader, vulkanInstance);

	// Get physical devices
	uint32_t nbPhysicalDevices = 0;
	if ((instance.vkEnumeratePhysicalDevices(vulkanInstance, &nbPhysicalDevices, nullptr)) != VK_SUCCESS) {
		LOG_ERROR("Failed to enumerate physical devices")
		return 1;
	}
	std::vector<VkPhysicalDevice> physicalDevices(nbPhysicalDevices);
	if ((instance.vkEnumeratePhysicalDevices(vulkanInstance, &nbPhysicalDevices, physicalDevices.data())) != VK_SUCCESS) {
		LOG_ERROR("Failed to enumerate physical devices")
		return 1;
	}
	
	// Select suitable physical device
	VkPhysicalDevice selectedPhysicalDevice = VK_NULL_HANDLE;
	uint32_t selectedQueueFamilyIndex = 0;
	for (auto physicalDevice : physicalDevices) {
		// Check device properties
		VkPhysicalDeviceProperties deviceProperties;
		VkPhysicalDeviceFeatures deviceFeatures;
		instance.vkGetPhysicalDeviceProperties(physicalDevice, &deviceProperties);
		instance.vkGetPhysicalDeviceFeatures(physicalDevice, &deviceFeatures);
		if (deviceProperties.limits.maxImageDimension2D < 4096) continue;
		// Check device features
		uint32_t queueFamiliesCount = 0;
		instance.vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamiliesCount, nullptr);
		if (queueFamiliesCount == 0) continue;
		std::vector<VkQueueFamilyProperties> queueFamilyProperties(queueFamiliesCount);
		instance.vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamiliesCount, queueFamilyProperties.data());
		selectedQueueFamilyIndex = 0;
		for (auto queueFamilyProperty : queueFamilyProperties) {
			if (queueFamilyProperty.queueCount > 0 && queueFamilyProperty.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
				// Check for presentation support
				if (!glfwGetPhysicalDevicePresentationSupport(vulkanInstance, physicalDevice, selectedQueueFamilyIndex)) {
					continue;
				}
				// All is good, use that device !
				selectedPhysicalDevice = physicalDevice;
				break;
			}
			selectedQueueFamilyIndex++;
		}
	}
	if (selectedPhysicalDevice == VK_NULL_HANDLE) {
		LOG_ERROR("Failed to find a suitable physical device")
		return 1;
	}

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
	if (instance.vkCreateDevice(selectedPhysicalDevice, &deviceCreateInfo, nullptr, &vulkanDevice) != VK_SUCCESS) {
		LOG_ERROR("Failed to create vulkan device")
		return 1;
	}
	vulkanQueueFamilyIndex = selectedQueueFamilyIndex;
	
	// Load device functions
	Vulkan::Device device(&instance, vulkanDevice);
	
	// Get graphics queue
	device.vkGetDeviceQueue(vulkanDevice, vulkanQueueFamilyIndex, 0/*queue index for this queue family*/, &vulkanGraphicsQueue);
	
	// Create Window/Surface
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	GLFWwindow* window = glfwCreateWindow(800, 600, "TEST", nullptr, nullptr);
	if (glfwCreateWindowSurface(vulkanInstance, window, nullptr, &vulkanSurface) != VK_SUCCESS) {
		LOG_ERROR("Failed to create vulkan surface through GLFW")
		return 1;
	}
	
	///////////////////////////////////////////////////
	
	// Input Events
	glfwSetKeyCallback(window, [](GLFWwindow* window, int key, int /*scancode*/, int action, int /*mods*/){
		if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
			glfwSetWindowShouldClose(window, 1);
		}
	});
	
	// Running...
	while (!glfwWindowShouldClose(window)) {
		glfwPollEvents();
		
		// Rendering here !!
		
	}
	
	///////////////////////////////////////////////////

	// Cleanup
	
	instance.vkDestroySurfaceKHR(vulkanInstance, vulkanSurface, nullptr);
	glfwDestroyWindow(window);
	glfwTerminate();
	if (vulkanDevice != VK_NULL_HANDLE) {
		device.vkDeviceWaitIdle(vulkanDevice);
		device.vkDestroyDevice(vulkanDevice, nullptr);
	}
	if (vulkanInstance != VK_NULL_HANDLE) {
		instance.vkDestroyInstance(vulkanInstance, nullptr);
	}
	
	std::cout << "Success !" << std::endl;
	return 0;
}
