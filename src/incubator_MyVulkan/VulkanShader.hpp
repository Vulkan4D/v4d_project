#pragma once

#include "VulkanStructs.hpp"
#include "VulkanDevice.hpp"

static std::unordered_map<std::string, VkShaderStageFlagBits> SHADER_TYPES {
	{"vert", VK_SHADER_STAGE_VERTEX_BIT},
	{"tesc", VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT},
	{"tese", VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT},
	{"geom", VK_SHADER_STAGE_GEOMETRY_BIT},
	{"frag", VK_SHADER_STAGE_FRAGMENT_BIT},
	{"comp", VK_SHADER_STAGE_COMPUTE_BIT},
	{"mesh", VK_SHADER_STAGE_MESH_BIT_NV},
	{"task", VK_SHADER_STAGE_TASK_BIT_NV},
	{"rgen", VK_SHADER_STAGE_RAYGEN_BIT_NV},
	// {"rint", VK_SHADER_STAGE_INTERSECTION_BIT_NV},
	// {"rahit", VK_SHADER_STAGE_},
	// {"rchit", VK_SHADER_STAGE_},
	// {"rmiss", VK_SHADER_STAGE_},
	// {"rcall", VK_SHADER_STAGE_},
};

class VulkanShader {
private:

	VulkanDevice* device;
	std::string filepath;

	std::string name;
	std::string type;
	
	VkShaderModule module;
	
public:
	VkPipelineShaderStageCreateInfo stageInfo;

	VulkanShader(VulkanDevice* device, std::string filepath, const char* entryPoint = "main", VkSpecializationInfo* specializationInfo = nullptr) : device(device), filepath(filepath) {
		// Automatically add .spv if not present at the end of the filepath
		if (!std::regex_match(filepath, std::regex(R"(\.spv$)"))) {
			filepath += ".spv";
		}

		// Validate filepath
		std::regex filepathRegex{R"(^(.*/|)([^/]+)\.([^\.]+)(\.spv)$)"};
		if (!std::regex_match(filepath, filepathRegex)) {
			throw std::runtime_error("Invalid shader file path '" + filepath + "'");
		}

		// Read the file
		std::ifstream file(filepath, std::fstream::ate | std::fstream::binary);
		if (!file.is_open()) {
			throw std::runtime_error("Failed to load shader file '" + filepath + "'");
		}

		// Parse the file
		name = std::regex_replace(filepath, filepathRegex, "$2");
		type = std::regex_replace(filepath, filepathRegex, "$3");
		auto stageFlagBits = SHADER_TYPES[type];
		if (!stageFlagBits) {
			throw std::runtime_error("Invalid Shader Type " + type);
		}
		size_t fileSize = (size_t) file.tellg();
		std::vector<char> bytecode(fileSize);
		file.seekg(0);
		file.read(bytecode.data(), fileSize);
		file.close();

		// Create the shaderModule
		VkShaderModuleCreateInfo createInfo {};
		createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		createInfo.codeSize = bytecode.size();
		createInfo.pCode = reinterpret_cast<const uint32_t*>(bytecode.data());
		if (device->CreateShaderModule(&createInfo, nullptr, &module) != VK_SUCCESS) {
			throw std::runtime_error("Failed to create Shader Module for shader " + name);
		}

		// Create Stage Info
		stageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		stageInfo.pNext = nullptr;
		stageInfo.flags = 0;
		stageInfo.stage = stageFlagBits;
		stageInfo.module = module;
		stageInfo.pName = entryPoint;
		stageInfo.pSpecializationInfo = specializationInfo;
	}

	// // Multistage single-file shader (Work-in-progress)
	// VulkanShader(VulkanDevice* device, std::filesystem::path filepath, std::string type, std::vector<char>& bytecode, const char* entryPoint = "main", VkSpecializationInfo* specializationInfo = nullptr) : device(device), filepath(filepath) {
	// 	name = filepath.filename();
	// 	auto stageFlagBits = SHADER_TYPES[type];
	// 	if (!stageFlagBits) {
	// 		throw std::runtime_error("Invalid Shader Type " + type);
	// 	}

	// 	// Create the shaderModule
	// 	VkShaderModuleCreateInfo createInfo {};
	// 	createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	// 	createInfo.codeSize = bytecode.size();
	// 	createInfo.pCode = reinterpret_cast<const uint32_t*>(bytecode.data());
	// 	if (device->CreateShaderModule(&createInfo, nullptr, &module) != VK_SUCCESS) {
	// 		throw std::runtime_error("Failed to create Shader Module for shader " + name);
	// 	}

	// 	// Create Stage Info
	// 	stageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	// 	stageInfo.pNext = nullptr;
	// 	stageInfo.flags = 0;
	// 	stageInfo.stage = stageFlagBits;
	// 	stageInfo.module = module;
	// 	stageInfo.pName = entryPoint;
	// 	stageInfo.pSpecializationInfo = specializationInfo;
	// }

	~VulkanShader() {
		device->DestroyShaderModule(module, nullptr);
	}

};