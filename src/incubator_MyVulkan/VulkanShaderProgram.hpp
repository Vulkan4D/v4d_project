#pragma once

#include "VulkanStructs.hpp"
#include "VulkanShader.hpp"
#include "VulkanDevice.hpp"

class VulkanShaderProgram {
private:
	VulkanDevice* device;
	std::vector<VulkanShader*> shaders;
	std::vector<VkPipelineShaderStageCreateInfo> stages;
	std::vector<VkVertexInputBindingDescription> bindings;
	std::vector<VkVertexInputAttributeDescription> attributes;
	std::vector<VkDescriptorSetLayout> descriptorSetLayouts;

public:

	enum : int {
		vert, // vertex
		tesc, // tessellation control
		tese, // tessellation evaluation
		geom, // geometry
		frag, // fragment
		comp, // compute
		mesh, // mesh
		task, // task
		rgen,  // ray generation
		rint,  // ray intersection
		rahit, // ray any hit
		rchit, // ray closest hit
		rmiss, // ray miss
		rcall, // ray callable
		NB_STAGE_TYPES
	};
	const std::unordered_map<int, std::string> SHADER_TYPES {
		{vert, "vert"},
		{tesc, "tesc"},
		{tese, "tese"},
		{geom, "geom"},
		{frag, "frag"},
		{comp, "comp"},
		{mesh, "mesh"},
		{task, "task"},
		{rgen,  "rgen"},
		{rint,  "rint"},
		{rahit, "rahit"},
		{rchit, "rchit"},
		{rmiss, "rmiss"},
		{rcall, "rcall"},
	};

	VulkanShaderProgram() {

	}
	VulkanShaderProgram(VulkanDevice* device, const std::vector<VulkanShaderInfo>& infos) : device(device) {
		for (auto& info : infos)
			shaders.push_back(new VulkanShader(device, info.filepath, info.entryPoint, info.specializationInfo));
		for (auto shader : shaders)
			AddStage(shader);
	}
	VulkanShaderProgram(VulkanDevice* device, std::vector<VulkanShader*> shaders) : device(device) {
		for (auto shader : shaders)
			AddStage(shader);
	}
	
	// // Multistage single-file shader (Work-in-progress)
	// VulkanShaderProgram(VulkanDevice* device, const std::string& filePath) : device(device) {
	// 	std::map<std::string, std::vector<char>> stages;
	// 	{
	// 		v4d::io::BinaryFileStream file(filePath);
	// 		if (file.Read<int>() != 0x07230203) 
	// 			throw std::runtime_error("Invalid shader file " + filePath);
	// 		int version = file.Read<int>();
	// 		int genmagnum = file.Read<int>();
	// 		int bound = file.Read<int>();
	// 		int reserved = file.Read<int>();
	// 		while (!file.IsEOF()) {
	// 			int pos = (int)file.GetReadPos();
	// 			uint bytes = file.Read<uint>();
	// 			int opcode = LOWORD(bytes);
	// 			int wordcount = HIWORD(bytes);
	// 			if (opcode == 15) {
	// 				int stage = file.Read<int>();
	// 				if (stage < 0)
	// 					throw std::runtime_error("Failed to read shader file " + filePath);
	// 				if (stage < NB_STAGE_TYPES) {
	// 					std::vector<byte> bytecode(wordcount);
	// 					file.ReadBytes(bytecode.data(), wordcount);
	// 					stages.emplace(SHADER_TYPES.at(stage), std::move(bytecode));
	// 				}
	// 			}
	// 			file.SetReadPos(pos + wordcount * 4);
	// 		}
	// 	}
		
	// 	for (auto&[type, bytecode] : stages)
	// 		shaders.push_back(new VulkanShader(device, filePath, type, bytecode, "main", nullptr));
			
	// 	for (auto shader : shaders)
	// 		AddStage(shader);
	// }

	~VulkanShaderProgram() {
		for (auto dsl : descriptorSetLayouts)
			device->DestroyDescriptorSetLayout(dsl, nullptr);
		for (auto shader : shaders)
			delete shader;
	}


	inline void AddStage(VulkanShader* shader) {
		stages.push_back(shader->stageInfo);
	}

	void AddVertexInputBinding(uint32_t binding, uint32_t stride, VkVertexInputRate inputRate, std::vector<VulkankVertexInputAttributeDescription> attrs) {
		bindings.emplace_back(VkVertexInputBindingDescription{binding, stride, inputRate});
		for (auto attr : attrs) {
			attributes.emplace_back(VkVertexInputAttributeDescription{attr.location, binding, attr.format, attr.offset});
		}
	}

	inline void AddVertexInputBinding(uint32_t stride, VkVertexInputRate inputRate, std::vector<VulkankVertexInputAttributeDescription> attrs) {
		AddVertexInputBinding(bindings.size(), stride, inputRate, attrs);
	}

	inline VkDescriptorSetLayout AddLayoutBindings(const std::vector<VkDescriptorSetLayoutBinding> &lbs) {
		VkDescriptorSetLayoutCreateInfo layoutInfo = {};
		layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		layoutInfo.bindingCount = lbs.size();
		layoutInfo.pBindings = lbs.data();
		size_t nextIndex = descriptorSetLayouts.size();
		descriptorSetLayouts.resize(nextIndex + 1);
		if (device->CreateDescriptorSetLayout(&layoutInfo, nullptr, &descriptorSetLayouts[nextIndex]) != VK_SUCCESS) {
			throw std::runtime_error("Failed to create descriptor set layout");
		}
		return std::ref(descriptorSetLayouts[nextIndex]);
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