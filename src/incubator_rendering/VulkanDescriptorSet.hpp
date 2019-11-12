#pragma once

#include <vector>
#include <map>
#include <xvk.hpp>
#include "VulkanBuffer.hpp"
#include "VulkanDescriptorSet.hpp"

enum DescriptorPointerType {
	STORAGE_BUFFER, // VulkanBuffer ---> VK_DESCRIPTOR_TYPE_STORAGE_BUFFER
	UNIFORM_BUFFER, // VulkanBuffer ---> VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER
	IMAGE_VIEW, // VkImageView ---> VK_DESCRIPTOR_TYPE_STORAGE_IMAGE
	ACCELERATION_STRUCTURE, // VkAccelerationStructureNV ---> VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_NV
};

#define DESCRIPTOR_SET_ADD_BINDING_TYPE(descriptor_type, data_ptr_type, name, stage_flags, t)\
	void AddBinding_ ## name ( \
		uint32_t binding, \
		data_ptr_type* data, \
		VkShaderStageFlags stageFlags = stage_flags, \
		VkDescriptorType type = t, \
		uint32_t descriptorCount = 1, \
		uint32_t dstArrayElement = 0, \
		VkSampler* pImmutableSamplers = nullptr \
	) { \
		bindings[binding] = { \
			binding, \
			stageFlags, \
			type, \
			descriptorCount, \
			dstArrayElement, \
			descriptor_type, \
			pImmutableSamplers, \
			data, \
			nullptr \
		}; \
	}
	
#define DESCRIPTOR_SET_DEFINE_BINDINGS\
	DESCRIPTOR_SET_ADD_BINDING_TYPE( STORAGE_BUFFER, VulkanBuffer, storageBuffer, VK_SHADER_STAGE_ALL, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER )\
	DESCRIPTOR_SET_ADD_BINDING_TYPE( UNIFORM_BUFFER, VulkanBuffer, uniformBuffer, VK_SHADER_STAGE_ALL, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER )\
	DESCRIPTOR_SET_ADD_BINDING_TYPE( IMAGE_VIEW, VkImageView, imageView, VK_SHADER_STAGE_ALL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE )\
	DESCRIPTOR_SET_ADD_BINDING_TYPE( ACCELERATION_STRUCTURE, VkAccelerationStructureNV, accelerationStructure, VK_SHADER_STAGE_ALL, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_NV )\

// Not implemented yet
    // VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC
    // VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC
    // VK_DESCRIPTOR_TYPE_SAMPLER
    // VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
    // VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE
    // VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER
    // VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER
    // VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT
    // VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK_EXT
	
struct DescriptorBinding {
	uint32_t binding;
	VkShaderStageFlags stageFlags;
	VkDescriptorType descriptorType;
	uint32_t descriptorCount;
	uint32_t dstArrayElement;
	DescriptorPointerType pointerType;
	VkSampler* pImmutableSamplers;
	
	void* data = nullptr;
	void* writeInfo = nullptr;
	
	~DescriptorBinding() {
		if (writeInfo) {
			switch (pointerType) {
				case STORAGE_BUFFER:
					delete (VkDescriptorBufferInfo*)writeInfo;
				break;
				case UNIFORM_BUFFER:
					delete (VkDescriptorBufferInfo*)writeInfo;
				break;
				case IMAGE_VIEW:
					delete (VkDescriptorImageInfo*)writeInfo;
				break;
				case ACCELERATION_STRUCTURE:
					delete (VkWriteDescriptorSetAccelerationStructureNV*)writeInfo;
				break;
				default: 
					throw std::runtime_error("pointerType is not implemented in destructor");
			}
		}
	}
	
	VkWriteDescriptorSet GetWriteDescriptorSet(VkDescriptorSet descriptorSet) {
		VkWriteDescriptorSet descriptorWrite {};
		descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrite.dstBinding = binding;
		descriptorWrite.dstSet = descriptorSet;
		
		descriptorWrite.dstArrayElement = dstArrayElement;
		descriptorWrite.descriptorCount = descriptorCount;
		descriptorWrite.descriptorType = descriptorType;
		
		switch (pointerType) {
			case STORAGE_BUFFER:
				writeInfo = new VkDescriptorBufferInfo {
					((VulkanBuffer*)data)->buffer,// VkBuffer buffer
					0,// VkDeviceSize offset
					((VulkanBuffer*)data)->size,// VkDeviceSize range
				};
				descriptorWrite.pBufferInfo = (VkDescriptorBufferInfo*)writeInfo;
			break;
			case UNIFORM_BUFFER:
				writeInfo = new VkDescriptorBufferInfo {
					((VulkanBuffer*)data)->buffer,// VkBuffer buffer
					0,// VkDeviceSize offset
					((VulkanBuffer*)data)->size,// VkDeviceSize range
				};
				descriptorWrite.pBufferInfo = (VkDescriptorBufferInfo*)writeInfo;
			break;
			case IMAGE_VIEW:
				writeInfo = new VkDescriptorImageInfo {
					VK_NULL_HANDLE,// VkSampler sampler
					*(VkImageView*)data,// VkImageView imageView
					VK_IMAGE_LAYOUT_GENERAL,// VkImageLayout imageLayout
				};
				descriptorWrite.pImageInfo = (VkDescriptorImageInfo*)writeInfo;
			break;
			case ACCELERATION_STRUCTURE:
				writeInfo = new VkWriteDescriptorSetAccelerationStructureNV {
					VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_NV,// VkStructureType sType
					nullptr,// const void* pNext
					1,// uint32_t accelerationStructureCount
					(VkAccelerationStructureNV*)data,// const VkAccelerationStructureNV* pAccelerationStructures
				};
				descriptorWrite.pNext = (VkWriteDescriptorSetAccelerationStructureNV*)writeInfo;
			break;
			default: 
				throw std::runtime_error("pointerType is not implemented in GetWriteDescriptorSet()");
			break;
		}
		return descriptorWrite;
	}
};

class VulkanDescriptorSet {
public:
	uint32_t set;
	VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
	VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
private:
	std::map<uint32_t, DescriptorBinding> bindings {};
	
public:
	
	VulkanDescriptorSet(uint32_t set) : set(set) {}
	
	std::map<uint32_t, DescriptorBinding>& GetBindings() {
		return bindings;
	}
	
	VkDescriptorSetLayout GetDescriptorSetLayout() const {
		return descriptorSetLayout;
	}
	
	void CreateDescriptorSetLayout(VulkanDevice* device) {
		std::vector<VkDescriptorSetLayoutBinding> layoutBindings {};
		for (auto& [binding, set] : bindings) {
			layoutBindings.push_back({binding, set.descriptorType, set.descriptorCount, set.stageFlags, set.pImmutableSamplers});
		}
		VkDescriptorSetLayoutCreateInfo layoutInfo = {};
		layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		layoutInfo.bindingCount = layoutBindings.size();
		layoutInfo.pBindings = layoutBindings.data();
		
		if (device->CreateDescriptorSetLayout(&layoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS)
			throw std::runtime_error("Failed to create descriptor set layout");
	}
	
	void DestroyDescriptorSetLayout(VulkanDevice* device) {
		device->DestroyDescriptorSetLayout(descriptorSetLayout, nullptr);
		descriptorSetLayout = VK_NULL_HANDLE;
	}
	
	DESCRIPTOR_SET_DEFINE_BINDINGS
};
