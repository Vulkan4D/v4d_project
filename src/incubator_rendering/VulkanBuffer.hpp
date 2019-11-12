#pragma once

#include "VulkanDevice.hpp"

struct VulkanBuffer {
	// Mandatory fields
	VkBufferUsageFlags usage;
	VkDeviceSize size;
	
	// Additional fields
	VkSharingMode sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	
	// Allocated handles
	VkBuffer buffer = VK_NULL_HANDLE;
	VkDeviceMemory memory = VK_NULL_HANDLE;
	
	// Mapped data
	void* data = nullptr;
		
	struct BufferSrcDataPtr {
		void* dataPtr;
		size_t size;
		
		BufferSrcDataPtr(void* dataPtr, size_t size) : dataPtr(dataPtr), size(size) {}
	};

	// Data pointers to get copied into buffer
	std::vector<BufferSrcDataPtr> srcDataPointers {};
	
	VulkanBuffer(VkBufferUsageFlags usage, VkDeviceSize size = 0) : usage(usage), size(size) {}
	
	void AddSrcDataPtr(void* srcDataPtr, size_t size) {
		srcDataPointers.push_back(BufferSrcDataPtr(srcDataPtr, size));
	}
	
	template<class T>
	inline void AddSrcDataPtr(std::vector<T>* vector) {
		AddSrcDataPtr(vector->data(), vector->size() * sizeof(T));
	}
	
	void AllocateFromStaging(VulkanDevice* device, VkCommandBuffer commandBuffer, VulkanBuffer& stagingBuffer, VkDeviceSize size = 0, VkDeviceSize offset = 0) {
		if (stagingBuffer.buffer == VK_NULL_HANDLE)
			throw std::runtime_error("Staging buffer is not allocated");
		if (size == 0 && offset == 0) size = stagingBuffer.size;
		this->size = size;
		usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
		Allocate(device, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, false);
		Copy(device, commandBuffer, stagingBuffer.buffer, buffer, size, offset);
	}

	void Allocate(VulkanDevice* device, VkMemoryPropertyFlags properties, bool copySrcData = true) {
		if (size == 0) {
			for (auto& dataPointer : srcDataPointers) {
				size += dataPointer.size;
			}
		}
		VkBufferCreateInfo bufferInfo {};
		bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufferInfo.size = size;
		bufferInfo.usage = usage;
		bufferInfo.sharingMode = sharingMode;
		
		if (device->CreateBuffer(&bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
			throw std::runtime_error("Failed to create buffer");
		}

		VkMemoryRequirements memRequirements;
		device->GetBufferMemoryRequirements(buffer, &memRequirements);

		VkMemoryAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize = memRequirements.size;
		allocInfo.memoryTypeIndex = device->GetGPU()->FindMemoryType(memRequirements.memoryTypeBits, properties);

		// TODO !!!
		// It should be noted that in a real world application, we're not supposed to actually call vkAllocateMemory for every individual buffer-> 
		// The maximum number of simultaneous memory allocations is limited by the maxMemoryAllocationCount physical device limit, which may be as low as 4096 even with high end hardware like a GTX 1080.
		// The right way to allocate memory for a large number of objects at the same time is to create a custom allocator that splits up a single allocation among many different objects by using the offset parameters that we've seen in many functions.
		// We can either implement such an allocator ourselves, or use the VulkanMemoryAllocator library provided by the GPUOpen initiative.
		
		if (device->AllocateMemory(&allocInfo, nullptr, &memory) != VK_SUCCESS) {
			throw std::runtime_error("Failed to allocate buffer memory");
		}
		
		if (copySrcData && srcDataPointers.size() > 0) {
			MapMemory(device);
			long offset = 0;
			for (auto& dataPointer : srcDataPointers) {
				memcpy((byte*)data + offset, dataPointer.dataPtr, dataPointer.size);
				offset += dataPointer.size;
			}
			if ((properties & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) == 0) {
				VkMappedMemoryRange mappedRange {};
				mappedRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
				mappedRange.memory = memory;
				mappedRange.offset = 0;
				mappedRange.size = size;
				device->FlushMappedMemoryRanges(1, &mappedRange);
			}
			UnmapMemory(device);
		}

		device->BindBufferMemory(buffer, memory, 0);
	}
	
	void Free(VulkanDevice* device) {
		device->DestroyBuffer(buffer, nullptr);
		device->FreeMemory(memory, nullptr);
		buffer = VK_NULL_HANDLE;
		data = nullptr;
	}

	void MapMemory(VulkanDevice* device, VkDeviceSize offset = 0, VkDeviceSize size = 0, VkMemoryMapFlags flags = 0) {
		device->MapMemory(memory, offset, size == 0 ? this->size : size, flags, &data);
	}
	
	void UnmapMemory(VulkanDevice* device) {
		device->UnmapMemory(memory);
		data = nullptr;
	}
	
	static void CopyDataToBuffer(VulkanDevice* device, void* data, VulkanBuffer* buffer, VkDeviceSize offset = 0, VkDeviceSize size = 0, VkMemoryMapFlags flags = 0) {
		buffer->MapMemory(device, offset, size, flags);
		memcpy(buffer->data, data, size == 0 ? buffer->size : size);
		buffer->UnmapMemory(device);
	}
	
	static void Copy(VulkanDevice* device, VkCommandBuffer commandBuffer, VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size, VkDeviceSize srcOffset = 0, VkDeviceSize dstOffset = 0) {
		VkBufferCopy copyRegion = {};
		copyRegion.srcOffset = srcOffset;
		copyRegion.dstOffset = dstOffset;
		copyRegion.size = size;
		device->CmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);
	}

	static void Copy(VulkanDevice* device, VkCommandBuffer commandBuffer, VulkanBuffer srcBuffer, VulkanBuffer dstBuffer, VkDeviceSize size = 0, VkDeviceSize srcOffset = 0, VkDeviceSize dstOffset = 0) {
		VkBufferCopy copyRegion = {};
		copyRegion.srcOffset = srcOffset;
		copyRegion.dstOffset = dstOffset;
		copyRegion.size = size == 0 ? srcBuffer.size : size;
		device->CmdCopyBuffer(commandBuffer, srcBuffer.buffer, dstBuffer.buffer, 1, &copyRegion);
	}

};
