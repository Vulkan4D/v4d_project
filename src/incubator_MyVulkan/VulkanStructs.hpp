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

struct VulkanBuffer {
	VkDeviceSize size = 0;
	VkBuffer buffer = VK_NULL_HANDLE;
	VkDeviceMemory memory = VK_NULL_HANDLE;
	void* data = nullptr;
};
