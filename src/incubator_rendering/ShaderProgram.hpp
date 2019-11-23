#pragma once

#include "VulkanStructs.hpp"
#include "Shader.hpp"
#include "Device.hpp"
#include "PipelineLayout.hpp"

class ShaderProgram {
private:
	std::vector<ShaderInfo> shaderFiles;
	std::vector<Shader> shaders;
	std::vector<VkPipelineShaderStageCreateInfo> stages;
	std::vector<VkVertexInputBindingDescription> bindings;
	std::vector<VkVertexInputAttributeDescription> attributes;
	
	PipelineLayout* pipelineLayout = nullptr;
	
public:

	ShaderProgram(PipelineLayout* pipelineLayout = nullptr, const std::vector<ShaderInfo>& infos = {}) : pipelineLayout(pipelineLayout) {
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
	
	void CreateShaderStages(Device* device) {
		if (stages.size() == 0) {
			for (auto& shader : shaders) {
				shader.CreateShaderModule(device);
				stages.push_back(shader.stageInfo);
			}
		}
	}
	
	void DestroyShaderStages(Device* device) {
		if (stages.size() > 0) {
			stages.clear();
			for (auto& shader : shaders) {
				shader.DestroyShaderModule(device);
			}
		}
	}
	
	void SetPipelineLayout(PipelineLayout* layout) {
		this->pipelineLayout = layout;
	}
	
	PipelineLayout* GetPipelineLayout() const {
		return pipelineLayout;
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
	
};
