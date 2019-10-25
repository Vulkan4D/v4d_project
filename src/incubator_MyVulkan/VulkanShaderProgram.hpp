#pragma once

#include "VulkanStructs.hpp"
#include "VulkanShader.hpp"
#include "VulkanDevice.hpp"

using namespace std;

class VulkanShaderProgram {
private:
	VulkanDevice *device;
	vector<VulkanShader*> shaders;
	vector<VkPipelineShaderStageCreateInfo> stages;
	vector<VkVertexInputBindingDescription> bindings;
	vector<VkVertexInputAttributeDescription> attributes;
	vector<VkDescriptorSetLayout> descriptorSetLayouts;

public:
	VulkanShaderProgram() {

	}
	VulkanShaderProgram(VulkanDevice *device, const vector<VulkanShaderInfo> &infos) : device(device) {
		for (auto &info : infos)
			shaders.push_back(new VulkanShader(device, info.filepath, info.entryPoint, info.specializationInfo));
		for (auto shader : shaders)
			AddStage(shader);
	}
	VulkanShaderProgram(VulkanDevice *device, vector<VulkanShader*> shaders) : device(device) {
		for (auto *shader : shaders)
			AddStage(shader);
	}

	~VulkanShaderProgram() {
		for (auto dsl : descriptorSetLayouts)
			vkDestroyDescriptorSetLayout(device->GetHandle(), dsl, nullptr);
		for (auto *shader : shaders)
			delete shader;
	}


	inline void AddStage(VulkanShader *shader) {
		stages.push_back(shader->stageInfo);
	}

	void AddVertexInputBinding(uint32_t binding, uint32_t stride, VkVertexInputRate inputRate, vector<VulkankVertexInputAttributeDescription> attrs) {
		bindings.emplace_back(VkVertexInputBindingDescription{binding, stride, inputRate});
		for (auto attr : attrs) {
			attributes.emplace_back(VkVertexInputAttributeDescription{attr.location, binding, attr.format, attr.offset});
		}
	}

	inline void AddVertexInputBinding(uint32_t stride, VkVertexInputRate inputRate, vector<VulkankVertexInputAttributeDescription> attrs) {
		AddVertexInputBinding(bindings.size(), stride, inputRate, attrs);
	}

	inline VkDescriptorSetLayout AddLayoutBindings(const vector<VkDescriptorSetLayoutBinding> &lbs) {
		VkDescriptorSetLayoutCreateInfo layoutInfo = {};
		layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		layoutInfo.bindingCount = lbs.size();
		layoutInfo.pBindings = lbs.data();
		size_t nextIndex = descriptorSetLayouts.size();
		descriptorSetLayouts.resize(nextIndex + 1);
		if (vkCreateDescriptorSetLayout(device->GetHandle(), &layoutInfo, nullptr, &descriptorSetLayouts[nextIndex]) != VK_SUCCESS) {
			throw runtime_error("Failed to create descriptor set layout");
		}
		return ref(descriptorSetLayouts[nextIndex]);
	}

	inline vector<VkPipelineShaderStageCreateInfo>& GetStages() {
		return stages;
	}

	inline vector<VkVertexInputBindingDescription>& GetBindings() {
		return bindings;
	}

	inline vector<VkVertexInputAttributeDescription>& GetAttributes() {
		return attributes;
	}
	
	inline vector<VkDescriptorSetLayout>& GetDescriptorSetLayouts() {
		return descriptorSetLayouts;
	}

};
