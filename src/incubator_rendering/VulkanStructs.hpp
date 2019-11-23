#pragma once

struct VulkankVertexInputAttributeDescription {
	uint32_t    location;
	uint32_t    offset;
	VkFormat    format;
};

struct ShaderInfo {
	std::string filepath;
	std::string entryPoint;
	VkSpecializationInfo* specializationInfo;
	
	ShaderInfo(const std::string& filepath, const std::string& entryPoint, VkSpecializationInfo* specializationInfo = nullptr) 
	 : filepath(filepath), entryPoint(entryPoint), specializationInfo(specializationInfo) {}
	
	ShaderInfo(const std::string& filepath, VkSpecializationInfo* specializationInfo) 
	 : filepath(filepath), entryPoint(""), specializationInfo(specializationInfo) {}
	 
	ShaderInfo(const std::string& filepath)
	 : filepath(filepath), entryPoint("main"), specializationInfo(nullptr) {}
	
	ShaderInfo(const char* filepath)
	 : filepath(filepath), entryPoint("main"), specializationInfo(nullptr) {}
};

struct VulkanQueue {
	uint32_t index;
	VkQueue handle;
};
