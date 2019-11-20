#pragma once

#include "VulkanDescriptorSet.hpp"
#include "VulkanDevice.hpp"

struct VulkanPipelineLayout {
	
	std::vector<VulkanDescriptorSet*> descriptorSets {};
	std::vector<VkDescriptorSetLayout> layouts {};
	std::vector<VkDescriptorSet> vkDescriptorSets {};
	VkPipelineLayout handle = VK_NULL_HANDLE;

	std::vector<VkDescriptorSetLayout>* GetDescriptorSetLayouts() {
		return &layouts;
	}

	void AddDescriptorSet(VulkanDescriptorSet* descriptorSet) {
		descriptorSets.push_back(descriptorSet);
	}
	
	void Create(VulkanDevice* device) {
		for (auto* set : descriptorSets) {
			layouts.push_back(set->GetDescriptorSetLayout());
		}
		
		// Pipeline Layout
		VkPipelineLayoutCreateInfo pipelineLayoutInfo {};
		pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		if (layouts.size() > 0) {
			pipelineLayoutInfo.setLayoutCount = layouts.size();
			pipelineLayoutInfo.pSetLayouts = layouts.data();
		}

		//TODO
		pipelineLayoutInfo.pushConstantRangeCount = 0;
		pipelineLayoutInfo.pPushConstantRanges = nullptr;

		if (device->CreatePipelineLayout(&pipelineLayoutInfo, nullptr, &handle) != VK_SUCCESS) {
			throw std::runtime_error("Failed to create pipeline layout");
		}
		
		// Descriptor sets array
		vkDescriptorSets.reserve(descriptorSets.size());
		for (auto* set : descriptorSets) {
			vkDescriptorSets.push_back(set->descriptorSet);
		}
	}
	
	void Destroy(VulkanDevice* device) {
		vkDescriptorSets.clear();
		device->DestroyPipelineLayout(handle, nullptr);
		layouts.clear();
	}
	
	void Bind(VulkanDevice* device, VkCommandBuffer commandBuffer, VkPipelineBindPoint bindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS) {
		device->CmdBindDescriptorSets(commandBuffer, bindPoint, handle, 0, (uint)vkDescriptorSets.size(), vkDescriptorSets.data(), 0, nullptr);
	}
	
};
