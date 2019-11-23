#pragma once

class PhysicalDevice;

#include "VulkanStructs.hpp"
#include "PhysicalDevice.hpp"

struct DeviceQueueInfo {
	std::string name;
	VkDeviceQueueCreateFlags flags;
	uint count;
	std::vector<float> priorities;
	VkSurfaceKHR surface = VK_NULL_HANDLE;
};

class Device : public xvk::Interface::DeviceInterface {
private:
	PhysicalDevice* physicalDevice;

	VkDeviceCreateInfo createInfo {};
	VkDeviceQueueCreateInfo* queueCreateInfo;
	std::unordered_map<std::string, std::vector<VulkanQueue>> queues;

public:
	Device(
		PhysicalDevice* physicalDevice,
		VkPhysicalDeviceFeatures& deviceFeatures,
		std::vector<const char*>& extensions,
		std::vector<const char*>& layers,
		const std::vector<DeviceQueueInfo>& queuesInfo
	) : physicalDevice(physicalDevice) {
		instance = physicalDevice->GetVulkanInstance();

		// Queues
		queueCreateInfo = new VkDeviceQueueCreateInfo[queuesInfo.size()];
		for (uint i = 0; i < queuesInfo.size(); i++) {
			queueCreateInfo[i] = {};
			queueCreateInfo[i].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			queueCreateInfo[i].queueFamilyIndex = physicalDevice->GetQueueFamilyIndexFromFlags(queuesInfo[i].flags, queuesInfo[i].count, queuesInfo[i].surface);
			queueCreateInfo[i].queueCount = queuesInfo[i].count;
			queueCreateInfo[i].pQueuePriorities = queuesInfo[i].priorities.data();
		}

		createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		createInfo.pQueueCreateInfos = queueCreateInfo;
		createInfo.queueCreateInfoCount = queuesInfo.size();
		createInfo.pEnabledFeatures = &deviceFeatures;
		createInfo.enabledExtensionCount = extensions.size();
		createInfo.ppEnabledExtensionNames = extensions.data();
		createInfo.enabledLayerCount = layers.size();
		createInfo.ppEnabledLayerNames = layers.data();

		if (physicalDevice->CreateDevice(&createInfo, nullptr, &handle) != VK_SUCCESS) {
			throw std::runtime_error("Failed to create logical device");
		}
		
		LoadFunctionPointers();

		// Get Queues Handles
		for (auto queueInfo : queuesInfo) {
			queues[queueInfo.name] = std::vector<VulkanQueue>(queueInfo.count);
			for (uint i = 0; i < queueInfo.count; i++) {
				auto queueFamilyIndex = physicalDevice->GetQueueFamilyIndexFromFlags(queueInfo.flags, queueInfo.count, queuesInfo[i].surface);
				auto *q = &queues[queueInfo.name][i];
				q->index = queueFamilyIndex;
				GetDeviceQueue(queueFamilyIndex, i, &(q->handle));
			}
		}
	}

	~Device() {
		DeviceWaitIdle();
		delete[] queueCreateInfo;
		DestroyDevice(nullptr);
		handle = VK_NULL_HANDLE;
	}

	inline VkDevice GetHandle() const {
		return handle;
	}

	inline VkPhysicalDevice GetPhysicalDeviceHandle() const {
		return physicalDevice->GetHandle();
	}

	inline PhysicalDevice* GetPhysicalDevice() const {
		return physicalDevice;
	}

	VulkanQueue GetPresentationQueue(VkSurfaceKHR surface, VkDeviceQueueCreateFlags flags = 0) {
		return GetQueue(physicalDevice->GetQueueFamilyIndexFromFlags(flags, 1, surface));
	}

	VulkanQueue GetQueue(std::string name, uint index = 0) {
		return queues[name][index];
	}

	VulkanQueue GetQueue(uint queueFamilyIndex, uint index = 0) {
		VulkanQueue q = {queueFamilyIndex, nullptr};
		GetDeviceQueue(queueFamilyIndex, index, &q.handle);
		return q;
	}

	using xvk::Interface::DeviceInterface::CreateCommandPool;
	void CreateCommandPool(uint queueIndex, VkCommandPoolCreateFlags flags, VkCommandPool* commandPool) {
		// Command buffers are executed by submitting them on one of the device queues, like the graphics and presentation queues we retrieved.
		// Each command pool can only allocate command buffers that are submitted on a single type of queue.
		// We're going to record commands for drawing, which is why we've choosen the graphics queue family.
		VkCommandPoolCreateInfo commandPoolCreateInfo = {};
		commandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		commandPoolCreateInfo.queueFamilyIndex = queueIndex;
		commandPoolCreateInfo.flags = flags; // We will only record the command buffers at the beginning of the program and then execute them many times in the main loop, so we're not going to use flags here
								/*	VK_COMMAND_POOL_CREATE_TRANSIENT_BIT = Hint that command buffers are rerecorded with new commands very often (may change memory allocation behavior)
									VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT = Allow command buffers to be rerecorded individually, without this flag they all have to be reset together
								*/
		if (CreateCommandPool(&commandPoolCreateInfo, nullptr, commandPool) != VK_SUCCESS) {
			throw std::runtime_error("Failed to create command pool");
		}
	}

	using xvk::Interface::DeviceInterface::DestroyCommandPool;
	void DestroyCommandPool(VkCommandPool &commandPool) {
		DestroyCommandPool(commandPool, nullptr);
	}

	using xvk::Interface::DeviceInterface::CreateDescriptorPool;
	void CreateDescriptorPool(std::vector<VkDescriptorType> types, uint32_t count, VkDescriptorPool& descriptorPool, VkDescriptorPoolCreateFlags flags = 0) {
		std::vector<VkDescriptorPoolSize> poolSizes;
		poolSizes.reserve(types.size());
		for (auto type : types) {
			poolSizes.emplace_back(VkDescriptorPoolSize{type, count});
		}
		VkDescriptorPoolCreateInfo poolInfo = {};
		poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		poolInfo.poolSizeCount = poolSizes.size();
		poolInfo.pPoolSizes = poolSizes.data();
		poolInfo.maxSets = count;
		poolInfo.flags = flags;
		if (CreateDescriptorPool(&poolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
			throw std::runtime_error("Failed to create descriptor pool");
		}
	}
	void CreateDescriptorPool(std::map<VkDescriptorType, uint>& types, VkDescriptorPool& descriptorPool, VkDescriptorPoolCreateFlags flags = 0) {
		std::vector<VkDescriptorPoolSize> poolSizes;
		poolSizes.reserve(types.size());
		VkDescriptorPoolCreateInfo poolInfo = {};
		poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		poolInfo.flags = flags;
		poolInfo.maxSets = 0;
		for (auto [type, count] : types) {
			poolSizes.emplace_back(VkDescriptorPoolSize{type, count});
			poolInfo.maxSets += count;
		}
		poolInfo.poolSizeCount = poolSizes.size();
		poolInfo.pPoolSizes = poolSizes.data();
		if (CreateDescriptorPool(&poolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
			throw std::runtime_error("Failed to create descriptor pool");
		}
	}

	using xvk::Interface::DeviceInterface::DestroyDescriptorPool;
	void DestroyDescriptorPool(VkDescriptorPool &descriptorPool) {
		DestroyDescriptorPool(descriptorPool, nullptr);
	}

	using xvk::Interface::DeviceInterface::CreateImage;
	void CreateImage(uint32_t width, uint32_t height, uint32_t mipLevels, VkSampleCountFlagBits sampleCount, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags memoryPropertyFlags, VkImage& image, VkDeviceMemory& imageMemory) {
		VkImageCreateInfo imageInfo = {};
		imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageInfo.imageType = VK_IMAGE_TYPE_2D;
		imageInfo.extent.width = width;
		imageInfo.extent.height = height;
		imageInfo.extent.depth = 1;
		imageInfo.mipLevels = mipLevels;
		imageInfo.arrayLayers = 1;
		imageInfo.format = format;
		imageInfo.tiling = tiling;
		imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		imageInfo.usage = usage;
		imageInfo.samples = sampleCount;
		imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		imageInfo.flags = 0;

		if (CreateImage(&imageInfo, nullptr, &image) != VK_SUCCESS) {
			throw std::runtime_error("Failed to create depth image");
		}

		VkMemoryRequirements memRequirements;
		GetImageMemoryRequirements(image, &memRequirements);

		VkMemoryAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize = memRequirements.size;
		allocInfo.memoryTypeIndex = physicalDevice->FindMemoryType(memRequirements.memoryTypeBits, memoryPropertyFlags);

		if (AllocateMemory(&allocInfo, nullptr, &imageMemory) != VK_SUCCESS) {
			throw std::runtime_error("Failed to allocate image memory");
		}

		BindImageMemory(image, imageMemory, 0);
	}

};
