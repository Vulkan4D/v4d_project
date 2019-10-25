#pragma once

class VulkanGPU;

#include "VulkanStructs.hpp"
#include "VulkanStructs.hpp"
#include "VulkanGPU.hpp"

using namespace std;

struct VulkanDeviceQueueInfo {
	string name;
	VkDeviceQueueCreateFlags flags;
	uint count;
	vector<float> priorities;
	VkSurfaceKHR surface = nullptr;
};

class VulkanDevice {
private:
	VulkanGPU *gpu;

	VkDevice handle;

	VkDeviceCreateInfo createInfo = {};
	VkDeviceQueueCreateInfo *queueCreateInfo;
	unordered_map<string, vector<VulkanQueue> > queues;

public:
	VulkanDevice(
		VulkanGPU *gpu,
		VkPhysicalDeviceFeatures &deviceFeatures,
		vector<const char*> extensions,
		vector<const char*> layers,
		const vector<VulkanDeviceQueueInfo> &queuesInfo) : gpu(gpu) {

			// Queues
			queueCreateInfo = new VkDeviceQueueCreateInfo[queuesInfo.size()];
			for (uint i = 0; i < queuesInfo.size(); i++) {
				queueCreateInfo[i] = {};
				queueCreateInfo[i].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
				queueCreateInfo[i].queueFamilyIndex = gpu->GetQueueFamilyIndexFromFlags(queuesInfo[i].flags, queuesInfo[i].count, queuesInfo[i].surface);
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

			if (vkCreateDevice(gpu->GetHandle(), &createInfo, nullptr, &handle) != VK_SUCCESS) {
				throw runtime_error("Failed to create logical device");
			}

			// Get Queues Handles
			for (auto queueInfo : queuesInfo) {
				queues[queueInfo.name] = vector<VulkanQueue>(queueInfo.count);
				for (uint i = 0; i < queueInfo.count; i++) {
					auto queueFamilyIndex = gpu->GetQueueFamilyIndexFromFlags(queueInfo.flags, queueInfo.count, queuesInfo[i].surface);
					auto *q = &queues[queueInfo.name][i];
					q->index = queueFamilyIndex;
					vkGetDeviceQueue(handle, queueFamilyIndex, i, &(q->handle));
				}
			}
	}

	~VulkanDevice() {
		vkDestroyDevice(handle, nullptr);
		delete[] queueCreateInfo;
	}

	inline VkDevice GetHandle() const {
		return handle;
	}

	inline VkPhysicalDevice GetPhysicalDeviceHandle() const {
		return gpu->GetHandle();
	}

	inline VulkanGPU* GetGPU() const {
		return gpu;
	}

	VulkanQueue GetPresentationQueue(VkSurfaceKHR surface, VkDeviceQueueCreateFlags flags = 0) {
		return GetQueue(gpu->GetQueueFamilyIndexFromFlags(flags, 1, surface));
	}

	VulkanQueue GetQueue(string name, uint index = 0) {
		return queues[name][index];
	}

	VulkanQueue GetQueue(uint queueFamilyIndex, uint index = 0) {
		VulkanQueue q = {queueFamilyIndex, nullptr};
		vkGetDeviceQueue(handle, queueFamilyIndex, index, &q.handle);
		return q;
	}

	void WaitIdle() {
		vkDeviceWaitIdle(handle);
	}

	void CreateCommandPool(uint queueIndex, VkCommandPoolCreateFlags flags, VkCommandPool *commandPool) {
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
		if (vkCreateCommandPool(handle, &commandPoolCreateInfo, nullptr, commandPool) != VK_SUCCESS) {
			throw runtime_error("Failed to create command pool");
		}
	}

	void DestroyCommandPool(VkCommandPool &commandPool) {
		vkDestroyCommandPool(handle, commandPool, nullptr);
	}

	void CreateDescriptorPool(vector<VkDescriptorType> types, uint32_t count, VkDescriptorPool &descriptorPool, VkDescriptorPoolCreateFlags flags = 0) {
		vector<VkDescriptorPoolSize> poolSizes;
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
		if (vkCreateDescriptorPool(handle, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
			throw runtime_error("Failed to create descriptor pool");
		}
	}

	void DestroyDescriptorPool(VkDescriptorPool &descriptorPool) {
		vkDestroyDescriptorPool(handle, descriptorPool, nullptr);
	}

	void CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory &bufferMemory) {
		VkBufferCreateInfo bufferInfo = {};
		bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufferInfo.size = size;
		bufferInfo.usage = usage;
		bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		if (vkCreateBuffer(handle, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
			throw runtime_error("Failed to create buffer");
		}

		VkMemoryRequirements memRequirements;
		vkGetBufferMemoryRequirements(handle, buffer, &memRequirements);

		VkMemoryAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize = memRequirements.size;
		allocInfo.memoryTypeIndex = gpu->FindMemoryType(memRequirements.memoryTypeBits, properties);

		// TODO !!!
		// It should be noted that in a real world application, you're not supposed to actually call vkAllocateMemory for every individual buffer. 
		// The maximum number of simultaneous memory allocations is limited by the maxMemoryAllocationCount physical device limit, which may be as low as 4096 even with high end hardware like a GTX 1080.
		// The right way to allocate memory for a large number of objects at the same time is to create a custom allocator that splits up a single allocation among many different objects by using the offset parameters that we've seen in many functions.
		// We can either implement such an allocator ourselves, or use the VulkanMemoryAllocator library provided by the GPUOpen initiative.
		
		if (vkAllocateMemory(handle, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
			throw runtime_error("Failed to allocate buffer memory");
		}

		vkBindBufferMemory(handle, buffer, bufferMemory, 0);
	}

	void CreateImage(uint32_t width, uint32_t height, uint32_t mipLevels, VkSampleCountFlagBits sampleCount, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags memoryPropertyFlags, VkImage &image, VkDeviceMemory &imageMemory) {
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

		if (vkCreateImage(handle, &imageInfo, nullptr, &image) != VK_SUCCESS) {
			throw runtime_error("Failed to create depth image");
		}

		VkMemoryRequirements memRequirements;
		vkGetImageMemoryRequirements(handle, image, &memRequirements);

		VkMemoryAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize = memRequirements.size;
		allocInfo.memoryTypeIndex = gpu->FindMemoryType(memRequirements.memoryTypeBits, memoryPropertyFlags);

		if (vkAllocateMemory(handle, &allocInfo, nullptr, &imageMemory) != VK_SUCCESS) {
			throw runtime_error("Failed to allocate image memory");
		}

		vkBindImageMemory(handle, image, imageMemory, 0);
	}

};
