#pragma once

struct VulkankVertexInputAttributeDescription {
	uint32_t    location;
	uint32_t    offset;
	VkFormat    format;
};

struct VulkanShaderInfo {
	std::string filepath;
	std::string entryPoint;
	VkSpecializationInfo* specializationInfo;
	
	VulkanShaderInfo(const std::string& filepath, const std::string& entryPoint, VkSpecializationInfo* specializationInfo = nullptr) 
	 : filepath(filepath), entryPoint(entryPoint), specializationInfo(specializationInfo) {}
	
	VulkanShaderInfo(const std::string& filepath, VkSpecializationInfo* specializationInfo) 
	 : filepath(filepath), entryPoint(""), specializationInfo(specializationInfo) {}
	 
	VulkanShaderInfo(const std::string& filepath)
	 : filepath(filepath), entryPoint("main"), specializationInfo(nullptr) {}
	
	VulkanShaderInfo(const char* filepath)
	 : filepath(filepath), entryPoint("main"), specializationInfo(nullptr) {}
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
