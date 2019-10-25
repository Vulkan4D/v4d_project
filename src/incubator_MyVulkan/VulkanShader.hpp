#pragma once

#include "VulkanStructs.hpp"
#include "VulkanDevice.hpp"

using namespace std;

static unordered_map<string, VkShaderStageFlagBits> SHADER_TYPES {
	{"vert", VK_SHADER_STAGE_VERTEX_BIT},
	{"tesc", VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT},
	{"tese", VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT},
	{"geom", VK_SHADER_STAGE_GEOMETRY_BIT},
	{"frag", VK_SHADER_STAGE_FRAGMENT_BIT},
	{"comp", VK_SHADER_STAGE_COMPUTE_BIT},
};

class VulkanShader {
private:

	VulkanDevice *device;
	string filepath;

	string name;
	string type;
	
	VkShaderModule module;

public:
	VkPipelineShaderStageCreateInfo stageInfo;

	VulkanShader(VulkanDevice *device, string filepath, const char* entryPoint = "main", VkSpecializationInfo *specializationInfo = nullptr) : device(device), filepath(filepath) {
		// Automatically add .spv if not present at the end of the filepath
		if (!regex_match(filepath, regex(R"(\.spv$)"))) {
			filepath += ".spv";
		}

		// Validate filepath
		auto filepathRegex = regex(R"(^(.*/|)([^/]+)\.([^\.]+)(\.spv)$)");
		if (!regex_match(filepath, filepathRegex)) {
			throw runtime_error("Invalid shader file path '" + filepath + "'");
		}

		// Read the file
		ifstream file(filepath, ios::ate | ios::binary);
		if (!file.is_open()) {
			throw runtime_error("Failed to load shader file '" + filepath + "'");
		}

		// Parse the file
		name = regex_replace(filepath, filepathRegex, "$2");
		type = regex_replace(filepath, filepathRegex, "$3");
		auto stageFlagBits = SHADER_TYPES[type];
		if (!stageFlagBits) {
			throw runtime_error("Invalid Shader Type " + type);
		}
		size_t fileSize = (size_t) file.tellg();
		vector<char> bytecode(fileSize);
		file.seekg(0);
		file.read(bytecode.data(), fileSize);
		file.close();

		// Create the shaderModule
		VkShaderModuleCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		createInfo.codeSize = bytecode.size();
		createInfo.pCode = reinterpret_cast<const uint32_t*>(bytecode.data());
		if (vkCreateShaderModule(device->GetHandle(), &createInfo, nullptr, &module) != VK_SUCCESS) {
			throw runtime_error("Failed to create Shader Module for shader " + name);
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

	~VulkanShader() {
		vkDestroyShaderModule(device->GetHandle(), module, nullptr);
	}

};
