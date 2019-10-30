#pragma once

struct VulkankVertexInputAttributeDescription {
	uint32_t    location;
	uint32_t    offset;
	VkFormat    format;
};

struct VulkanShaderInfo {
	std::string filepath;
	const char* entryPoint = "main";
	VkSpecializationInfo *specializationInfo = nullptr;
};

struct VulkanQueue {
	uint32_t index;
	VkQueue handle;
};
