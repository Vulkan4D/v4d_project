#pragma once

#include "VulkanStructs.hpp"
#include "Shader.hpp"
#include "Device.hpp"

class ShaderBindingTable {
private:
	
	std::map<uint32_t, ShaderInfo> shaderFiles;
	std::vector<VkRayTracingShaderGroupCreateInfoNV> groups;
	std::vector<Shader> shaderObjects;
	std::vector<VkPipelineShaderStageCreateInfo> stages;
	
	uint32_t hitGroupOffset = 0;
	uint32_t missGroupOffset = 0;
	
	uint32_t nextHitShaderOffset = 0;
	uint32_t nextMissShaderOffset = 0;
	
	PipelineLayout* pipelineLayout = nullptr;
	VkPipeline pipeline = VK_NULL_HANDLE;
	
public:

	VkPipeline GetPipeline() const {
		return pipeline;
	}
	
	PipelineLayout* GetPipelineLayout() const {
		return pipelineLayout;
	}
	
	std::vector<VkRayTracingShaderGroupCreateInfoNV> GetGroups() const {
		return groups;
	}
	
	std::vector<VkPipelineShaderStageCreateInfo> GetStages() const {
		return stages;
	}
	
	uint32_t GetHitGroupOffset() const {
		return hitGroupOffset;
	}
	
	uint32_t GetMissGroupOffset() const {
		return missGroupOffset;
	}

	// Rules: 
		// If type is VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_NV then generalShader must be a valid index into pStages referring to a shader of VK_SHADER_STAGE_RAYGEN_BIT_NV, VK_SHADER_STAGE_MISS_BIT_NV, or VK_SHADER_STAGE_CALLABLE_BIT_NV
		// If type is VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_NV then closestHitShader, anyHitShader, and intersectionShader must be VK_SHADER_UNUSED_NV
		// If type is VK_RAY_TRACING_SHADER_GROUP_TYPE_PROCEDURAL_HIT_GROUP_NV then intersectionShader must be a valid index into pStages referring to a shader of VK_SHADER_STAGE_INTERSECTION_BIT_NV
		// If type is VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_NV then intersectionShader must be VK_SHADER_UNUSED_NV
		// closestHitShader must be either VK_SHADER_UNUSED_NV or a valid index into pStages referring to a shader of VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV
		// anyHitShader must be either VK_SHADER_UNUSED_NV or a valid index into pStages referring to a shader of VK_SHADER_STAGE_ANY_HIT_BIT_NV
	
	uint32_t GetOrAddShaderFileIndex(ShaderInfo&& shader) {
		uint32_t index = 0;
		for (auto&[i, s] : shaderFiles) {
			if (s.filepath == shader.filepath) {
				break;
			}
			index++;
		}
		if (index == shaderFiles.size()) {
			shaderFiles.emplace(index, shader);
		}
		return index;
	}
	
	ShaderBindingTable(PipelineLayout* pipelineLayout, ShaderInfo rgen) : pipelineLayout(pipelineLayout) {
		groups.push_back({
			VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_NV,
			nullptr, // pNext
			VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_NV,
			GetOrAddShaderFileIndex(std::move(rgen)), // generalShader
			VK_SHADER_UNUSED_NV, // closestHitShader;
			VK_SHADER_UNUSED_NV, // anyHitShader;
			VK_SHADER_UNUSED_NV // intersectionShader;
		});
	}
	
	uint32_t AddMissShader(ShaderInfo rmiss) {
		if (missGroupOffset == 0) missGroupOffset = groups.size();
		groups.push_back({
			VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_NV,
			nullptr, // pNext
			VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_NV,
			GetOrAddShaderFileIndex(std::move(rmiss)), // generalShader
			VK_SHADER_UNUSED_NV, // closestHitShader;
			VK_SHADER_UNUSED_NV, // anyHitShader;
			VK_SHADER_UNUSED_NV // intersectionShader;
		});
		return nextMissShaderOffset++;
	}
	
	uint32_t AddHitShader(ShaderInfo rchit, ShaderInfo rahit = "", ShaderInfo rint = "") {
		if (hitGroupOffset == 0) hitGroupOffset = groups.size();
		groups.push_back({
			VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_NV,
			nullptr, // pNext
			rint.filepath != "" ? VK_RAY_TRACING_SHADER_GROUP_TYPE_PROCEDURAL_HIT_GROUP_NV : VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_NV,
			VK_SHADER_UNUSED_NV, // generalShader
			rchit.filepath != "" ? GetOrAddShaderFileIndex(std::move(rchit)) : VK_SHADER_UNUSED_NV, // closestHitShader;
			rahit.filepath != "" ? GetOrAddShaderFileIndex(std::move(rahit)) : VK_SHADER_UNUSED_NV, // anyHitShader;
			rint.filepath != "" ? GetOrAddShaderFileIndex(std::move(rint)) : VK_SHADER_UNUSED_NV // intersectionShader;
		});
		return nextHitShaderOffset++;
	}
	
	void LoadShaders() {
		shaderObjects.clear();
		for (auto&[i, shader] : shaderFiles) {
			shaderObjects.emplace_back(shader.filepath, shader.entryPoint, shader.specializationInfo);
		}
	}
	
	void UnloadShaders() {
		shaderObjects.clear();
	}
	
	void CreateShaderStages(Device* device) {
		if (stages.size() == 0) {
			for (auto& shader : shaderObjects) {
				shader.CreateShaderModule(device);
				stages.push_back(shader.stageInfo);
			}
		}
	}
	
	void DestroyShaderStages(Device* device) {
		if (stages.size() > 0) {
			stages.clear();
			for (auto& shader : shaderObjects) {
				shader.DestroyShaderModule(device);
			}
		}
	}
	
	VkPipeline CreateRayTracingPipeline(Device* device) {
		CreateShaderStages(device);
		
		VkRayTracingPipelineCreateInfoNV rayTracingPipelineInfo {};
		rayTracingPipelineInfo.sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_NV;
		rayTracingPipelineInfo.stageCount = (uint)stages.size();
		rayTracingPipelineInfo.pStages = stages.data();
		rayTracingPipelineInfo.groupCount = (uint)groups.size();
		rayTracingPipelineInfo.pGroups = groups.data();
		rayTracingPipelineInfo.maxRecursionDepth = 2;
		rayTracingPipelineInfo.layout = pipelineLayout->handle;
		
		if (device->CreateRayTracingPipelinesNV(VK_NULL_HANDLE, 1, &rayTracingPipelineInfo, nullptr, &pipeline) != VK_SUCCESS) //TODO support multiple ray tracing pipelines
			throw std::runtime_error("Failed to create ray tracing pipelines");
		
		return pipeline;
	}
	
	void DestroyRayTracingPipeline(Device* device) {
		device->DestroyPipeline(pipeline, nullptr);
		DestroyShaderStages(device);
	}
	
	void WriteShaderBindingTableToBuffer(Device* device, Buffer* buffer, uint32_t shaderGroupHandleSize) {
		uint32_t sbtSize = shaderGroupHandleSize * groups.size();
		uint8_t* data;
		device->MapMemory(buffer->memory, 0, sbtSize, 0, (void**)&data);
		auto shaderHandleStorage = new uint8_t[sbtSize];
		if (device->GetRayTracingShaderGroupHandlesNV(pipeline, 0, (uint)groups.size(), sbtSize, shaderHandleStorage) != VK_SUCCESS)
			throw std::runtime_error("Failed to get ray tracing shader group handles");
		
		for (uint32_t groupIndex = 0; groupIndex < groups.size(); ++groupIndex) {
			memcpy(data, shaderHandleStorage + groupIndex * shaderGroupHandleSize, shaderGroupHandleSize);
			data += shaderGroupHandleSize;
		}
		
		device->UnmapMemory(buffer->memory);
		delete[] shaderHandleStorage;
	}
	
};
