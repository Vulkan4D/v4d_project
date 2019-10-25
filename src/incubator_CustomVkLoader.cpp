#include <common/pch.hh>
#include "config.hh"
#include <v4d.h>

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>

#define LOAD_VULKAN_GLOBAL_FUNCTION(func) \
	PFN_##func func; if (!(func = (PFN_##func) vkGetInstanceProcAddr(nullptr, #func))){ LOG_ERROR("Failed to load vulkan function pointer for " << #func) }
#define LOAD_VULKAN_INSTANCE_FUNCTION(instance, func) \
	PFN_##func func; if (!(func = (PFN_##func) vkGetInstanceProcAddr(instance, #func))){ LOG_ERROR("Failed to load vulkan instance function pointer for " << #func) }
#define LOAD_VULKAN_DEVICE_FUNCTION(device, func) \
	PFN_##func func; if (!(func = (PFN_##func) vkGetDeviceProcAddr(device, #func))){ LOG_ERROR("Failed to load vulkan device function pointer for " << #func) }

#define LOAD_VULKAN_GLOBAL_FUNCTIONS() \
	LOAD_VULKAN_GLOBAL_FUNCTION( vkCreateInstance )\
	LOAD_VULKAN_GLOBAL_FUNCTION( vkEnumerateInstanceExtensionProperties )\

#define LOAD_VULKAN_INSTANCE_FUNCTIONS(instance) \
	LOAD_VULKAN_INSTANCE_FUNCTION(instance, vkEnumeratePhysicalDevices )\
	LOAD_VULKAN_INSTANCE_FUNCTION(instance, vkGetPhysicalDeviceProperties )\
	LOAD_VULKAN_INSTANCE_FUNCTION(instance, vkGetPhysicalDeviceFeatures )\
	LOAD_VULKAN_INSTANCE_FUNCTION(instance, vkGetPhysicalDeviceQueueFamilyProperties )\
	LOAD_VULKAN_INSTANCE_FUNCTION(instance, vkCreateDevice )\
	LOAD_VULKAN_INSTANCE_FUNCTION(instance, vkGetDeviceProcAddr )\
	LOAD_VULKAN_INSTANCE_FUNCTION(instance, vkEnumerateDeviceExtensionProperties )\
	LOAD_VULKAN_INSTANCE_FUNCTION(instance, vkGetPhysicalDeviceMemoryProperties )\
	LOAD_VULKAN_INSTANCE_FUNCTION(instance, vkDestroyInstance )\

#define LOAD_VULKAN_DEVICE_FUNCTIONS(device) \
	LOAD_VULKAN_DEVICE_FUNCTION(device, vkGetDeviceQueue )\
	LOAD_VULKAN_DEVICE_FUNCTION(device, vkDeviceWaitIdle )\
	LOAD_VULKAN_DEVICE_FUNCTION(device, vkDestroyDevice )\
	LOAD_VULKAN_DEVICE_FUNCTION(device, vkCreateSemaphore )\
	LOAD_VULKAN_DEVICE_FUNCTION(device, vkCreateCommandPool )\
	LOAD_VULKAN_DEVICE_FUNCTION(device, vkAllocateCommandBuffers )\
	LOAD_VULKAN_DEVICE_FUNCTION(device, vkBeginCommandBuffer )\
	LOAD_VULKAN_DEVICE_FUNCTION(device, vkCmdPipelineBarrier )\
	LOAD_VULKAN_DEVICE_FUNCTION(device, vkCmdClearColorImage )\
	LOAD_VULKAN_DEVICE_FUNCTION(device, vkEndCommandBuffer )\
	LOAD_VULKAN_DEVICE_FUNCTION(device, vkQueueSubmit )\
	LOAD_VULKAN_DEVICE_FUNCTION(device, vkFreeCommandBuffers )\
	LOAD_VULKAN_DEVICE_FUNCTION(device, vkDestroyCommandPool )\
	LOAD_VULKAN_DEVICE_FUNCTION(device, vkDestroySemaphore )\
	LOAD_VULKAN_DEVICE_FUNCTION(device, vkCreateImageView )\
	LOAD_VULKAN_DEVICE_FUNCTION(device, vkCreateRenderPass )\
	LOAD_VULKAN_DEVICE_FUNCTION(device, vkCreateFramebuffer )\
	LOAD_VULKAN_DEVICE_FUNCTION(device, vkCreateShaderModule )\
	LOAD_VULKAN_DEVICE_FUNCTION(device, vkCreatePipelineLayout )\
	LOAD_VULKAN_DEVICE_FUNCTION(device, vkCreateGraphicsPipelines )\
	LOAD_VULKAN_DEVICE_FUNCTION(device, vkCmdBeginRenderPass )\
	LOAD_VULKAN_DEVICE_FUNCTION(device, vkCmdBindPipeline )\
	LOAD_VULKAN_DEVICE_FUNCTION(device, vkCmdDraw )\
	LOAD_VULKAN_DEVICE_FUNCTION(device, vkCmdEndRenderPass )\
	LOAD_VULKAN_DEVICE_FUNCTION(device, vkDestroyShaderModule )\
	LOAD_VULKAN_DEVICE_FUNCTION(device, vkDestroyPipelineLayout )\
	LOAD_VULKAN_DEVICE_FUNCTION(device, vkDestroyPipeline )\
	LOAD_VULKAN_DEVICE_FUNCTION(device, vkDestroyRenderPass )\
	LOAD_VULKAN_DEVICE_FUNCTION(device, vkDestroyFramebuffer )\
	LOAD_VULKAN_DEVICE_FUNCTION(device, vkDestroyImageView )\
	LOAD_VULKAN_DEVICE_FUNCTION(device, vkCreateFence )\
	LOAD_VULKAN_DEVICE_FUNCTION(device, vkCreateBuffer )\
	LOAD_VULKAN_DEVICE_FUNCTION(device, vkGetBufferMemoryRequirements )\
	LOAD_VULKAN_DEVICE_FUNCTION(device, vkAllocateMemory )\
	LOAD_VULKAN_DEVICE_FUNCTION(device, vkBindBufferMemory )\
	LOAD_VULKAN_DEVICE_FUNCTION(device, vkMapMemory )\
	LOAD_VULKAN_DEVICE_FUNCTION(device, vkFlushMappedMemoryRanges )\
	LOAD_VULKAN_DEVICE_FUNCTION(device, vkUnmapMemory )\
	LOAD_VULKAN_DEVICE_FUNCTION(device, vkCmdSetViewport )\
	LOAD_VULKAN_DEVICE_FUNCTION(device, vkCmdSetScissor )\
	LOAD_VULKAN_DEVICE_FUNCTION(device, vkCmdBindVertexBuffers )\
	LOAD_VULKAN_DEVICE_FUNCTION(device, vkWaitForFences )\
	LOAD_VULKAN_DEVICE_FUNCTION(device, vkResetFences )\
	LOAD_VULKAN_DEVICE_FUNCTION(device, vkFreeMemory )\
	LOAD_VULKAN_DEVICE_FUNCTION(device, vkDestroyBuffer )\
	LOAD_VULKAN_DEVICE_FUNCTION(device, vkDestroyFence )\
	LOAD_VULKAN_DEVICE_FUNCTION(device, vkCmdCopyBuffer )\
	LOAD_VULKAN_DEVICE_FUNCTION(device, vkCreateImage )\
	LOAD_VULKAN_DEVICE_FUNCTION(device, vkGetImageMemoryRequirements )\
	LOAD_VULKAN_DEVICE_FUNCTION(device, vkBindImageMemory )\
	LOAD_VULKAN_DEVICE_FUNCTION(device, vkCreateSampler )\
	LOAD_VULKAN_DEVICE_FUNCTION(device, vkCmdCopyBufferToImage )\
	LOAD_VULKAN_DEVICE_FUNCTION(device, vkCreateDescriptorSetLayout )\
	LOAD_VULKAN_DEVICE_FUNCTION(device, vkCreateDescriptorPool )\
	LOAD_VULKAN_DEVICE_FUNCTION(device, vkAllocateDescriptorSets )\
	LOAD_VULKAN_DEVICE_FUNCTION(device, vkUpdateDescriptorSets )\
	LOAD_VULKAN_DEVICE_FUNCTION(device, vkCmdBindDescriptorSets )\
	LOAD_VULKAN_DEVICE_FUNCTION(device, vkDestroyDescriptorPool )\
	LOAD_VULKAN_DEVICE_FUNCTION(device, vkDestroyDescriptorSetLayout )\
	LOAD_VULKAN_DEVICE_FUNCTION(device, vkDestroySampler )\
	LOAD_VULKAN_DEVICE_FUNCTION(device, vkDestroyImage )\
	

// GLFW
// #include <GLFW/glfw3.h>

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

#ifdef _WINDOWS
	#define LOAD_LIBRARY_FUNC_PROC_ADDRESS GetProcAddress
#else
	#define LOAD_LIBRARY_FUNC_PROC_ADDRESS dlsym
#endif

int main() {
	// if (!glfwInit()) {
	// 	LOG_ERROR("glfw init failed")
	// 	return 1;
	// }

	// if (!glfwVulkanSupported()) {
	// 	LOG_ERROR("Vulkan not supported")
	// 	return 1;
	// }

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

	// Load vulkan loader library
	#ifdef _WINDOWS
		auto vulkanLoader = LoadLibrary("vulkan-1.dll");
	#else
		auto vulkanLoader = dlopen("libvulkan.so", RTLD_NOW);
	#endif
	if (vulkanLoader == nullptr) {
		LOG_ERROR("Failed to load vulkan loader library")
		return 1;
	}
	PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr = (PFN_vkGetInstanceProcAddr)LOAD_LIBRARY_FUNC_PROC_ADDRESS(vulkanLoader, "vkGetInstanceProcAddr");
	if (vkGetInstanceProcAddr == nullptr) {
		LOG_ERROR("Failed to load vkGetInstanceProcAddr function pointer")
		return 1;
	}
	#ifdef __GNUC__
		#pragma GCC diagnostic push
		#pragma GCC diagnostic ignored "-Wunused-variable"
	#endif
	LOAD_VULKAN_GLOBAL_FUNCTIONS()

	// Create the Vulkan instance
	if (vkCreateInstance(&createInfo, nullptr, &vulkanInstance) != VK_SUCCESS) {
		LOG_ERROR("Failed to create Vulkan Instance")
		return 1;
	}

	LOAD_VULKAN_INSTANCE_FUNCTIONS(vulkanInstance)
	#ifdef __GNUC__
		#pragma GCC diagnostic pop
	#endif


	// Get physical devices
	uint32_t nbPhysicalDevices = 0;
	if ((vkEnumeratePhysicalDevices(vulkanInstance, &nbPhysicalDevices, nullptr)) != VK_SUCCESS) {
		LOG_ERROR("Failed to enumerate physical devices")
		return 1;
	}
	std::vector<VkPhysicalDevice> physicalDevices(nbPhysicalDevices);
	if ((vkEnumeratePhysicalDevices(vulkanInstance, &nbPhysicalDevices, physicalDevices.data())) != VK_SUCCESS) {
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
		vkGetPhysicalDeviceProperties(physicalDevice, &deviceProperties);
		vkGetPhysicalDeviceFeatures(physicalDevice, &deviceFeatures);
		if (deviceProperties.limits.maxImageDimension2D < 4096) continue;
		// Check device features
		uint32_t queueFamiliesCount = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamiliesCount, nullptr);
		if (queueFamiliesCount == 0) continue;
		std::vector<VkQueueFamilyProperties> queueFamilyProperties(queueFamiliesCount);
		vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamiliesCount, queueFamilyProperties.data());
		selectedQueueFamilyIndex = 0;
		for (auto queueFamilyProperty : queueFamilyProperties) {
			if (queueFamilyProperty.queueCount > 0 && queueFamilyProperty.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
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
	if (vkCreateDevice(selectedPhysicalDevice, &deviceCreateInfo, nullptr, &vulkanDevice) != VK_SUCCESS) {
		LOG_ERROR("Failed to create vulkan device")
		return 1;
	}
	vulkanQueueFamilyIndex = selectedQueueFamilyIndex;
	
	// Load device functions
	LOAD_VULKAN_DEVICE_FUNCTIONS(vulkanDevice)
	
	// Get graphics queue
	vkGetDeviceQueue(vulkanDevice, vulkanQueueFamilyIndex, 0/*queue index for this queue family*/, &vulkanGraphicsQueue);

	// Cleanup
	if (vulkanDevice != VK_NULL_HANDLE) {
		vkDeviceWaitIdle(vulkanDevice);
		vkDestroyDevice(vulkanDevice, nullptr);
	}
	if (vulkanInstance != VK_NULL_HANDLE) {
		vkDestroyInstance(vulkanInstance, nullptr);
	}
	
	// Unload vulkan loader library
	#ifdef _WINDOWS
		FreeLibrary(vulkanLoader);
	#else
		dlclose(vulkanLoader);
	#endif

	std::cout << "Success !" << std::endl;
	return 0;
}
