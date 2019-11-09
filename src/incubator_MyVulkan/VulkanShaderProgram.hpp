#pragma once

#include "VulkanStructs.hpp"
#include "VulkanShader.hpp"
#include "VulkanDevice.hpp"

class VulkanShaderProgram {
private:
	std::vector<VulkanShaderInfo> shaderFiles;
	std::vector<VulkanShader> shaders;
	std::vector<VkPipelineShaderStageCreateInfo> stages;
	std::vector<VkVertexInputBindingDescription> bindings;
	std::vector<VkVertexInputAttributeDescription> attributes;
	std::vector<VkDescriptorSetLayout> descriptorSetLayouts;
	VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo {};
	std::vector<VkDescriptorSetLayoutBinding> layoutBindings {};

public:

	VulkanShaderProgram() {

	}
	VulkanShaderProgram(const std::vector<VulkanShaderInfo>& infos) {
		for (auto& info : infos)
			shaderFiles.push_back(info);
	}
	
	void AddStage(VulkanShader* shader) {
		stages.push_back(shader->stageInfo);
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

	void AddLayoutBinding(VkDescriptorSetLayoutBinding binding) {
		layoutBindings.push_back(binding);
	}
	
	void AddLayoutBinding(
		uint32_t              binding,
		VkDescriptorType      descriptorType,
		uint32_t              descriptorCount = 1,
		VkShaderStageFlags    stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS,
		const VkSampler*      pImmutableSamplers = nullptr
	) {
		layoutBindings.push_back({binding, descriptorType, descriptorCount, stageFlags, pImmutableSamplers});
	}
	
	VkDescriptorSetLayout CreateDescriptorSetLayout(VulkanDevice* device) {
		descriptorSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		descriptorSetLayoutCreateInfo.bindingCount = layoutBindings.size();
		descriptorSetLayoutCreateInfo.pBindings = layoutBindings.data();
		size_t nextIndex = descriptorSetLayouts.size();
		descriptorSetLayouts.resize(nextIndex + 1);
		if (device->CreateDescriptorSetLayout(&descriptorSetLayoutCreateInfo, nullptr, &descriptorSetLayouts[nextIndex]) != VK_SUCCESS) {
			throw std::runtime_error("Failed to create descriptor set layout");
		}
		return std::ref(descriptorSetLayouts[nextIndex]);
	}

	void DestroyDescriptorSetLayout(VulkanDevice* device) {
		for (auto dsl : descriptorSetLayouts)
			device->DestroyDescriptorSetLayout(dsl, nullptr);
		descriptorSetLayouts.clear();
	}

	void LoadShaders(VulkanDevice* device) {
		for (auto& shader : shaderFiles) {
			shaders.emplace_back(shader.filepath, shader.entryPoint, shader.specializationInfo);
		}
		for (auto& shader : shaders) {
			shader.CreateShaderModule(device);
			stages.push_back(shader.stageInfo);
		}
	}
	
	void UnloadShaders(VulkanDevice* device) {
		for (auto shader : shaders) {
			shader.DestroyShaderModule(device);
		}
		shaders.clear();
		stages.clear();
	}
	
	inline std::vector<VkPipelineShaderStageCreateInfo>& GetStages() {
		return stages;
	}

	inline std::vector<VkVertexInputBindingDescription>& GetBindings() {
		return bindings;
	}

	inline std::vector<VkVertexInputAttributeDescription>& GetAttributes() {
		return attributes;
	}
	
	inline std::vector<VkDescriptorSetLayout>& GetDescriptorSetLayouts() {
		return descriptorSetLayouts;
	}

};
