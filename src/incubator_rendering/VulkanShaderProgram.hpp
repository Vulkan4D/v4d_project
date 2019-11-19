#pragma once

#include "VulkanStructs.hpp"
#include "VulkanShader.hpp"
#include "VulkanDevice.hpp"
#include "VulkanDescriptorSet.hpp"

class VulkanShaderProgram {
private:
	std::vector<VulkanShaderInfo> shaderFiles;
	std::vector<VulkanShader> shaders;
	std::vector<VkPipelineShaderStageCreateInfo> stages;
	std::vector<VkVertexInputBindingDescription> bindings;
	std::vector<VkVertexInputAttributeDescription> attributes;
	
	std::vector<VulkanDescriptorSet*> descriptorSets {};
	
	std::vector<VkDescriptorSetLayout> layouts {};

public:

	VulkanShaderProgram() {

	}
	VulkanShaderProgram(const std::vector<VulkanShaderInfo>& infos) {
		for (auto& info : infos)
			shaderFiles.push_back(info);
	}
	
	void AddVertexInputBinding(uint32_t binding, uint32_t stride, VkVertexInputRate inputRate, std::vector<VulkankVertexInputAttributeDescription> attrs) {
		bindings.emplace_back(VkVertexInputBindingDescription{binding, stride, inputRate});
		for (auto attr : attrs) {
			attributes.emplace_back(VkVertexInputAttributeDescription{attr.location, binding, attr.format, attr.offset});
		}
	}

	void AddVertexInputBinding(uint32_t stride, VkVertexInputRate inputRate, std::vector<VulkankVertexInputAttributeDescription> attrs) {
		AddVertexInputBinding(bindings.size(), stride, inputRate, attrs);
	}

	void LoadShaders() {
		shaders.clear();
		for (auto& shader : shaderFiles) {
			shaders.emplace_back(shader.filepath, shader.entryPoint, shader.specializationInfo);
		}
	}
	
	void UnloadShaders() {
		shaders.clear();
	}
	
	void CreateShaderStages(VulkanDevice* device) {
		if (stages.size() == 0) {
			for (auto& shader : shaders) {
				shader.CreateShaderModule(device);
				stages.push_back(shader.stageInfo);
			}
		}
	}
	
	void DestroyShaderStages(VulkanDevice* device) {
		if (stages.size() > 0) {
			stages.clear();
			for (auto& shader : shaders) {
				shader.DestroyShaderModule(device);
			}
		}
	}
	
	void AddDescriptorSet(VulkanDescriptorSet* descriptorSet) {
		descriptorSets.push_back(descriptorSet);
	}
	
	inline std::vector<VkPipelineShaderStageCreateInfo>* GetStages() {
		return &stages;
	}

	inline std::vector<VkVertexInputBindingDescription>* GetBindings() {
		return &bindings;
	}

	inline std::vector<VkVertexInputAttributeDescription>* GetAttributes() {
		return &attributes;
	}
	
	inline std::vector<VkDescriptorSetLayout>* GetDescriptorSetLayouts() {
		if (layouts.size() > 0) layouts.clear();
		for (auto* set : descriptorSets) {
			layouts.push_back(set->GetDescriptorSetLayout());
		}
		return &layouts;
	}

};
