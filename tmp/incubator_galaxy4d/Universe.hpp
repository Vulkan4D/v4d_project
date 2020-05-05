#pragma once

#include <v4d.h>
#include "../incubator_rendering/V4DRenderingPipeline.hh"
#include "../incubator_rendering/helpers/Geometry.hpp"
#include "../incubator_rendering/Camera.hpp"
// #include "../incubator_galaxy4d/UniversalPosition.hpp"

using namespace v4d::graphics;
using namespace v4d::graphics::vulkan;
using namespace v4d::graphics::vulkan::rtx;

#pragma region RayTracing structs

struct AccelerationStructure {
	VkAccelerationStructureNV accelerationStructure = VK_NULL_HANDLE;
	VkDeviceMemory memory = VK_NULL_HANDLE;
	uint64_t handle = 0;
	VkAccelerationStructureCreateInfoNV createInfo;
};

struct BottomLevelAccelerationStructure : public AccelerationStructure {
	std::vector<Geometry*> geometries {};
	std::vector<VkGeometryNV> rayTracingGeometries {};
	void GenerateRayTracingGeometries() {
		for (auto* geometry : geometries) {
			rayTracingGeometries.push_back(geometry->GetRayTracingGeometry());
		}
	}
};

struct RayTracingGeometryInstance {
	glm::mat3x4 transform;
	uint32_t instanceId : 24;
	uint32_t mask : 8;
	uint32_t instanceOffset : 24;
	uint32_t flags : 8; // VkGeometryInstanceFlagBitsNV
	uint64_t accelerationStructureHandle;
};

struct TopLevelAccelerationStructure : public AccelerationStructure {
	std::vector<RayTracingGeometryInstance> instances {};
};

struct GeometryInstance {
	glm::mat3x4 transform;
	uint32_t instanceId;
	// Ray tracing
	uint32_t rayTracingMask;
	uint32_t rayTracingShaderHitOffset;
	uint32_t rayTracingFlags;
	BottomLevelAccelerationStructure* rayTracingBlas;
	int rayTracingGeometryInstanceIndex = -1;
};

#pragma endregion

class Universe {
public:
	
	// int galaxyConvergences = 100;
	bool continuousGalaxyGen = true;
	int galaxyFrameIndex = 0;
	glm::dvec3 positionInUniverse {0};
	
	
	CubeMapImage galaxyCubeMapImage { 
		VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT // Raster
		|
		VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT // RTX
	};
	CubeMapImage galaxyDepthStencilCubeMapImage { VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT , { VK_FORMAT_D32_SFLOAT_S8_UINT }};
	RenderPass galaxyGenRenderPass, galaxyFadeRenderPass;
	PipelineLayout galaxyGenPipelineLayout, galaxyBoxPipelineLayout;
	GalaxyGenPushConstant galaxyGenPushConstant {};
	RasterShaderPipeline galaxyGenShader {galaxyGenPipelineLayout, {
		"incubator_rendering/assets/shaders/v4d_galaxy.gen.geom",
		"incubator_rendering/assets/shaders/v4d_galaxy.gen.vert",
		"incubator_rendering/assets/shaders/v4d_galaxy.gen.frag",
	}};
	RasterShaderPipeline galaxyBoxShader {galaxyBoxPipelineLayout, {
		"incubator_rendering/assets/shaders/v4d_galaxy.box.vert",
		"incubator_rendering/assets/shaders/v4d_galaxy.box.frag",
	}};
	GalaxyBoxPushConstant galaxyBoxPushConstant {};
	RasterShaderPipeline galaxyFadeShader {galaxyBoxPipelineLayout, {
		"incubator_rendering/assets/shaders/v4d_galaxy.fade.vert",
		"incubator_rendering/assets/shaders/v4d_galaxy.fade.geom",
		"incubator_rendering/assets/shaders/v4d_galaxy.fade.frag",
	}};
	
	
	struct Galaxy : public ProceduralGeometryData {
		glm::vec2 _padding;
		glm::vec4 posr;
		// v4d::noise::GalaxyInfo galaxyInfo;
		
		Galaxy(glm::vec4 posr)
		 : ProceduralGeometryData(glm::vec3(posr.x, posr.y, posr.z) - posr.w, glm::vec3(posr.x, posr.y, posr.z) + posr.w), posr(posr) {}
		 
		glm::vec3 GetPos() const {
			return glm::vec3(posr.x, posr.y, posr.z);
		}
		float GetRadius() const {
			return posr.w;
		}
		float GetDistanceFrom(glm::vec3 p) const {
			return std::max(0.0f, glm::distance(GetPos(), p) - GetRadius());
		}
	};
	std::vector<Galaxy> galaxies {};
	std::vector<Galaxy> activeGalaxies {};
	const size_t MAX_GALAXIES = (size_t)(pow(3, 3)*pow(32, 3)/10);
	Buffer galaxiesBuffer { VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT , MAX_GALAXIES * sizeof(Galaxy) };
	bool galaxiesGenerated = false;
	
	
	
#pragma region Ray Tracing stuff
	ShaderBindingTable shaderBindingTable {galaxyGenPipelineLayout, "incubator_rendering/assets/shaders/rtx_galaxies.rgen"};
	std::vector<Geometry*> geometries {};
	std::vector<GeometryInstance> geometryInstances {};
	uint32_t galaxiesRayTracingShaderOffset;
	
	// const bool useRayTracingForGalaxy = false;
	
	DescriptorSet* galaxyRayTracingDescriptorSet = nullptr;

	std::vector<BottomLevelAccelerationStructure> rayTracingBottomLevelAccelerationStructures {};
	TopLevelAccelerationStructure rayTracingTopLevelAccelerationStructure {};
	Buffer rayTracingShaderBindingTableBuffer {VK_BUFFER_USAGE_RAY_TRACING_BIT_NV};
	VkPhysicalDeviceRayTracingPropertiesNV rayTracingProperties{};
	float rayTracingImageScale = 1;
	
	void CreateRayTracingAccelerationStructures(Renderer* renderer, Device* renderingDevice, Queue& lowPriorityGraphicsQueue) {
		
		// Bottom level Acceleration structures
		for (auto& blas : rayTracingBottomLevelAccelerationStructures) {
			blas.GenerateRayTracingGeometries();
			
			blas.createInfo = {
				VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_NV,
				nullptr,// pNext
				0,// VkDeviceSize compactedSize
				{// VkAccelerationStructureInfoNV info
					VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_INFO_NV,
					nullptr,// pNext
					VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_NV,// type
					VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_NV /*| VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_NV*/,// flags
					0,// instanceCount
					(uint)blas.rayTracingGeometries.size(),// geometryCount
					blas.rayTracingGeometries.data()// VkGeometryNV pGeometries
				}
			};
			
			if (renderingDevice->CreateAccelerationStructureNV(&blas.createInfo, nullptr, &blas.accelerationStructure) != VK_SUCCESS)
				throw std::runtime_error("Failed to create bottom level acceleration structure");
				
			VkAccelerationStructureMemoryRequirementsInfoNV memoryRequirementsInfo {};
			memoryRequirementsInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_INFO_NV;
			memoryRequirementsInfo.accelerationStructure = blas.accelerationStructure;
			memoryRequirementsInfo.type = VK_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_TYPE_BUILD_SCRATCH_NV;
				
			VkMemoryRequirements2 memoryRequirements2 {};
			renderingDevice->GetAccelerationStructureMemoryRequirementsNV(&memoryRequirementsInfo, &memoryRequirements2);
			
			VkMemoryAllocateInfo memoryAllocateInfo {
				VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
				nullptr,// pNext
				memoryRequirements2.memoryRequirements.size,// VkDeviceSize allocationSize
				renderingDevice->GetPhysicalDevice()->FindMemoryType(memoryRequirements2.memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)// memoryTypeIndex
			};
			if (renderingDevice->AllocateMemory(&memoryAllocateInfo, nullptr, &blas.memory) != VK_SUCCESS)
				throw std::runtime_error("Failed to allocate memory for bottom level acceleration structure");
			
			VkBindAccelerationStructureMemoryInfoNV accelerationStructureMemoryInfo {
				VK_STRUCTURE_TYPE_BIND_ACCELERATION_STRUCTURE_MEMORY_INFO_NV,
				nullptr,// pNext
				blas.accelerationStructure,// accelerationStructure
				blas.memory,// memory
				0,// VkDeviceSize memoryOffset
				0,// uint32_t deviceIndexCount
				nullptr// pDeviceIndices
			};
			if (renderingDevice->BindAccelerationStructureMemoryNV(1, &accelerationStructureMemoryInfo) != VK_SUCCESS)
				throw std::runtime_error("Failed to bind bottom level acceleration structure memory");
			
			if (renderingDevice->GetAccelerationStructureHandleNV(blas.accelerationStructure, sizeof(uint64_t), &blas.handle))
				throw std::runtime_error("Failed to get bottom level acceleration structure handle");
		}
		
		// Geometry Instances
		rayTracingTopLevelAccelerationStructure.instances.reserve(geometryInstances.size());
		for (auto& instance : geometryInstances) if (instance.rayTracingBlas) {
			instance.rayTracingGeometryInstanceIndex = (int)rayTracingTopLevelAccelerationStructure.instances.size();
			rayTracingTopLevelAccelerationStructure.instances.push_back({
				instance.transform,
				instance.instanceId,
				instance.rayTracingMask,
				instance.rayTracingShaderHitOffset,
				instance.rayTracingFlags,
				instance.rayTracingBlas->handle
			});
		}
		
		{
			// Top Level acceleration structure
			VkAccelerationStructureCreateInfoNV accStructCreateInfo {
				VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_NV,
				nullptr,// pNext
				0,// VkDeviceSize compactedSize
				{// VkAccelerationStructureInfoNV info
					VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_INFO_NV,
					nullptr,// pNext
					VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_NV,// type
					VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_NV /*| VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_NV*/, // VkGeometryInstanceFlagBitsNV flags
					(uint)rayTracingTopLevelAccelerationStructure.instances.size(),// instanceCount
					0,// geometryCount
					nullptr// VkGeometryNV pGeometries
				}
			};
			if (renderingDevice->CreateAccelerationStructureNV(&accStructCreateInfo, nullptr, &rayTracingTopLevelAccelerationStructure.accelerationStructure) != VK_SUCCESS)
				throw std::runtime_error("Failed to create top level acceleration structure");
			
			VkAccelerationStructureMemoryRequirementsInfoNV memoryRequirementsInfo {};
			memoryRequirementsInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_INFO_NV;
			memoryRequirementsInfo.accelerationStructure = rayTracingTopLevelAccelerationStructure.accelerationStructure;
				
			VkMemoryRequirements2 memoryRequirements2 {};
			renderingDevice->GetAccelerationStructureMemoryRequirementsNV(&memoryRequirementsInfo, &memoryRequirements2);
			
			VkMemoryAllocateInfo memoryAllocateInfo {
				VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
				nullptr,// pNext
				memoryRequirements2.memoryRequirements.size,// VkDeviceSize allocationSize
				renderingDevice->GetPhysicalDevice()->FindMemoryType(memoryRequirements2.memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)// memoryTypeIndex
			};
			if (renderingDevice->AllocateMemory(&memoryAllocateInfo, nullptr, &rayTracingTopLevelAccelerationStructure.memory) != VK_SUCCESS)
				throw std::runtime_error("Failed to allocate memory for top level acceleration structure");
			
			VkBindAccelerationStructureMemoryInfoNV accelerationStructureMemoryInfo {
				VK_STRUCTURE_TYPE_BIND_ACCELERATION_STRUCTURE_MEMORY_INFO_NV,
				nullptr,// pNext
				rayTracingTopLevelAccelerationStructure.accelerationStructure,// accelerationStructure
				rayTracingTopLevelAccelerationStructure.memory,// memory
				0,// VkDeviceSize memoryOffset
				0,// uint32_t deviceIndexCount
				nullptr// const uint32_t* pDeviceIndices
			};
			if (renderingDevice->BindAccelerationStructureMemoryNV(1, &accelerationStructureMemoryInfo) != VK_SUCCESS)
				throw std::runtime_error("Failed to bind top level acceleration structure memory");
			
			if (renderingDevice->GetAccelerationStructureHandleNV(rayTracingTopLevelAccelerationStructure.accelerationStructure, sizeof(uint64_t), &rayTracingTopLevelAccelerationStructure.handle))
				throw std::runtime_error("Failed to get top level acceleration structure handle");
		}
		
		// Build Ray Tracing acceleration structures
		{
			// Instance buffer
			Buffer instanceBuffer(VK_BUFFER_USAGE_RAY_TRACING_BIT_NV);
			instanceBuffer.AddSrcDataPtr(&rayTracingTopLevelAccelerationStructure.instances);
			renderer->AllocateBufferStaged(lowPriorityGraphicsQueue, instanceBuffer);
			
			VkDeviceSize allBlasReqSize = 0;
			// RayTracing Scratch buffer
			VkAccelerationStructureMemoryRequirementsInfoNV memoryRequirementsInfo {};
			memoryRequirementsInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_INFO_NV;
			memoryRequirementsInfo.type = VK_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_TYPE_OBJECT_NV;
			for (auto& blas : rayTracingBottomLevelAccelerationStructures) {
				VkMemoryRequirements2 req;
				memoryRequirementsInfo.accelerationStructure = blas.accelerationStructure;
				renderingDevice->GetAccelerationStructureMemoryRequirementsNV(&memoryRequirementsInfo, &req);
				allBlasReqSize += req.memoryRequirements.size;
			}
			VkMemoryRequirements2 memoryRequirementsTopLevel;
			memoryRequirementsInfo.accelerationStructure = rayTracingTopLevelAccelerationStructure.accelerationStructure;
			renderingDevice->GetAccelerationStructureMemoryRequirementsNV(&memoryRequirementsInfo, &memoryRequirementsTopLevel);
			// Send scratch buffer
			const VkDeviceSize scratchBufferSize = std::max(allBlasReqSize, memoryRequirementsTopLevel.memoryRequirements.size);
			Buffer scratchBuffer(VK_BUFFER_USAGE_RAY_TRACING_BIT_NV, scratchBufferSize);
			scratchBuffer.Allocate(renderingDevice, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
			
			auto cmdBuffer = renderer->BeginSingleTimeCommands(lowPriorityGraphicsQueue);
				
				VkAccelerationStructureInfoNV accelerationStructBuildInfo {};
				accelerationStructBuildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_INFO_NV;
				accelerationStructBuildInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_NV /*| VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_NV*/;
			
				VkMemoryBarrier memoryBarrier {
					VK_STRUCTURE_TYPE_MEMORY_BARRIER,
					nullptr,// pNext
					VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_NV | VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_NV,// VkAccessFlags srcAccessMask
					VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_NV | VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_NV,// VkAccessFlags dstAccessMask
				};
				
				for (auto& blas : rayTracingBottomLevelAccelerationStructures) {
					accelerationStructBuildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_NV;
					accelerationStructBuildInfo.geometryCount = (uint)blas.rayTracingGeometries.size();
					accelerationStructBuildInfo.pGeometries = blas.rayTracingGeometries.data();
					accelerationStructBuildInfo.instanceCount = 0;
					
					renderingDevice->CmdBuildAccelerationStructureNV(
						cmdBuffer, 
						&accelerationStructBuildInfo, 
						VK_NULL_HANDLE, 
						0, 
						VK_FALSE, 
						blas.accelerationStructure, 
						VK_NULL_HANDLE, 
						scratchBuffer.buffer, 
						0
					);
					
					renderingDevice->CmdPipelineBarrier(
						cmdBuffer, 
						VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_NV, 
						VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_NV, 
						0, 
						1, &memoryBarrier, 
						0, 0, 
						0, 0
					);
				}
				
				accelerationStructBuildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_NV;
				accelerationStructBuildInfo.geometryCount = 0;
				accelerationStructBuildInfo.pGeometries = nullptr;
				accelerationStructBuildInfo.instanceCount = (uint)rayTracingTopLevelAccelerationStructure.instances.size();
				
				renderingDevice->CmdBuildAccelerationStructureNV(
					cmdBuffer, 
					&accelerationStructBuildInfo, 
					instanceBuffer.buffer, 
					0, 
					VK_FALSE, 
					rayTracingTopLevelAccelerationStructure.accelerationStructure, 
					VK_NULL_HANDLE, 
					scratchBuffer.buffer, 
					0
				);
				
				renderingDevice->CmdPipelineBarrier(
					cmdBuffer, 
					VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_NV, 
					VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_NV, 
					0, 
					1, &memoryBarrier, 
					0, 0, 
					0, 0
				);
			
			renderer->EndSingleTimeCommands(lowPriorityGraphicsQueue, cmdBuffer);
			
			scratchBuffer.Free(renderingDevice);
			instanceBuffer.Free(renderingDevice);
		}
	}
	void DestroyRayTracingAccelerationStructures(Renderer* renderer, Device* renderingDevice) {
		// Geometry instances
		for (auto& instance : geometryInstances) if (instance.rayTracingBlas) {
			instance.rayTracingGeometryInstanceIndex = -1;
		}
		// Acceleration Structures
		if (rayTracingTopLevelAccelerationStructure.accelerationStructure != VK_NULL_HANDLE) {
			renderingDevice->FreeMemory(rayTracingTopLevelAccelerationStructure.memory, nullptr);
			renderingDevice->DestroyAccelerationStructureNV(rayTracingTopLevelAccelerationStructure.accelerationStructure, nullptr);
			rayTracingTopLevelAccelerationStructure.accelerationStructure = VK_NULL_HANDLE;
			rayTracingTopLevelAccelerationStructure.instances.clear();
			for (auto& blas : rayTracingBottomLevelAccelerationStructures) {
				renderingDevice->FreeMemory(blas.memory, nullptr);
				renderingDevice->DestroyAccelerationStructureNV(blas.accelerationStructure, nullptr);
				blas.rayTracingGeometries.clear();
			}
		}
	}
	void CreateRayTracingPipeline(Renderer* renderer, Device* renderingDevice) {
		shaderBindingTable.CreateRayTracingPipeline(renderingDevice);
		
		// Shader Binding Table
		rayTracingShaderBindingTableBuffer.size = rayTracingProperties.shaderGroupHandleSize * shaderBindingTable.GetGroups().size();
		rayTracingShaderBindingTableBuffer.Allocate(renderingDevice, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
		shaderBindingTable.WriteShaderBindingTableToBuffer(renderingDevice, &rayTracingShaderBindingTableBuffer, rayTracingProperties.shaderGroupHandleSize);
	}
	void DestroyRayTracingPipeline(Renderer* renderer, Device* renderingDevice) {
		// Shader binding table
		rayTracingShaderBindingTableBuffer.Free(renderingDevice);
		// Ray tracing pipeline
		shaderBindingTable.DestroyRayTracingPipeline(renderingDevice);
	}
	
#pragma endregion
	
	void Init(Renderer* renderer) {
		renderer->RequiredDeviceExtension(VK_NV_RAY_TRACING_EXTENSION_NAME); // NVidia's RayTracing extension
		renderer->RequiredDeviceExtension(VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME); // Needed for RayTracing extension
	}
	void Info(Renderer* renderer, Device* renderingDevice) {
		// Query the ray tracing properties of the current implementation
		rayTracingProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PROPERTIES_NV;
		VkPhysicalDeviceProperties2 deviceProps2{};
		deviceProps2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
		deviceProps2.pNext = &rayTracingProperties;
		renderer->GetPhysicalDeviceProperties2(renderingDevice->GetPhysicalDevice()->GetHandle(), &deviceProps2);
	}
	
	void InitLayouts(Renderer* renderer, std::vector<DescriptorSet*>& descriptorSets, DescriptorSet* baseDescriptorSet_0) {
		
		// if (useRayTracingForGalaxy) {
			// Ray Tracing
			galaxyRayTracingDescriptorSet = descriptorSets.emplace_back(new DescriptorSet(0));
			galaxyRayTracingDescriptorSet->AddBinding_accelerationStructure(0, &rayTracingTopLevelAccelerationStructure.accelerationStructure, VK_SHADER_STAGE_RAYGEN_BIT_NV | VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV);
			galaxyRayTracingDescriptorSet->AddBinding_imageView(1, &galaxyCubeMapImage.view, VK_SHADER_STAGE_RAYGEN_BIT_NV);
			galaxyRayTracingDescriptorSet->AddBinding_storageBuffer(2, &galaxiesBuffer, VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV | VK_SHADER_STAGE_INTERSECTION_BIT_NV);
			galaxyGenPipelineLayout.AddDescriptorSet(galaxyRayTracingDescriptorSet);
			galaxyGenPipelineLayout.AddPushConstant<GalaxyGenPushConstant>(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_GEOMETRY_BIT | VK_SHADER_STAGE_RAYGEN_BIT_NV | VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV);
		// } else {
			// Galaxy Gen
			// galaxyGenPipelineLayout.AddPushConstant<GalaxyGenPushConstant>(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_GEOMETRY_BIT);
			galaxyGenShader.AddVertexInputBinding(sizeof(Galaxy), VK_VERTEX_INPUT_RATE_VERTEX, {
				{0, 32, VK_FORMAT_R32G32B32A32_SFLOAT},
				// {0, offsetof(Galaxy, Galaxy::posr), VK_FORMAT_R32G32B32A32_SFLOAT},
				// {1, offsetof(Galaxy, Galaxy::galaxyInfo) + offsetof(v4d::noise::GalaxyInfo, v4d::noise::GalaxyInfo::spiralCloudsFactor), VK_FORMAT_R32_SFLOAT}, // float
				// {2, offsetof(Galaxy, Galaxy::galaxyInfo) + offsetof(v4d::noise::GalaxyInfo, v4d::noise::GalaxyInfo::swirlTwist), VK_FORMAT_R32_SFLOAT}, // float
				// {3, offsetof(Galaxy, Galaxy::galaxyInfo) + offsetof(v4d::noise::GalaxyInfo, v4d::noise::GalaxyInfo::swirlDetail), VK_FORMAT_R32_SFLOAT}, // float
				// {4, offsetof(Galaxy, Galaxy::galaxyInfo) + offsetof(v4d::noise::GalaxyInfo, v4d::noise::GalaxyInfo::coreSize), VK_FORMAT_R32_SFLOAT}, // float
				// {5, offsetof(Galaxy, Galaxy::galaxyInfo) + offsetof(v4d::noise::GalaxyInfo, v4d::noise::GalaxyInfo::cloudsSize), VK_FORMAT_R32_SFLOAT}, // float
				// {6, offsetof(Galaxy, Galaxy::galaxyInfo) + offsetof(v4d::noise::GalaxyInfo, v4d::noise::GalaxyInfo::cloudsFrequency), VK_FORMAT_R32_SFLOAT}, // float
				// {7, offsetof(Galaxy, Galaxy::galaxyInfo) + offsetof(v4d::noise::GalaxyInfo, v4d::noise::GalaxyInfo::squish), VK_FORMAT_R32_SFLOAT}, // float
				// {8, offsetof(Galaxy, Galaxy::galaxyInfo) + offsetof(v4d::noise::GalaxyInfo, v4d::noise::GalaxyInfo::attenuationCloudsFrequency), VK_FORMAT_R32_SFLOAT}, // float
				// {9, offsetof(Galaxy, Galaxy::galaxyInfo) + offsetof(v4d::noise::GalaxyInfo, v4d::noise::GalaxyInfo::attenuationCloudsFactor), VK_FORMAT_R32_SFLOAT}, // float
				// {10, offsetof(Galaxy, Galaxy::galaxyInfo) + offsetof(v4d::noise::GalaxyInfo, v4d::noise::GalaxyInfo::position), VK_FORMAT_R32G32B32_SFLOAT}, // vec3
				// {11, offsetof(Galaxy, Galaxy::galaxyInfo) + offsetof(v4d::noise::GalaxyInfo, v4d::noise::GalaxyInfo::noiseOffset), VK_FORMAT_R32G32B32_SFLOAT}, // vec3
				// {12, offsetof(Galaxy, Galaxy::galaxyInfo) + offsetof(v4d::noise::GalaxyInfo, v4d::noise::GalaxyInfo::irregularities), VK_FORMAT_R32_SFLOAT}, // float
				// {13, offsetof(Galaxy, Galaxy::galaxyInfo) + offsetof(v4d::noise::GalaxyInfo, v4d::noise::GalaxyInfo::rotation) + 0, VK_FORMAT_R32G32B32A32_SFLOAT}, // mat4
				// {14, offsetof(Galaxy, Galaxy::galaxyInfo) + offsetof(v4d::noise::GalaxyInfo, v4d::noise::GalaxyInfo::rotation) + 16, VK_FORMAT_R32G32B32A32_SFLOAT}, // mat4
				// {15, offsetof(Galaxy, Galaxy::galaxyInfo) + offsetof(v4d::noise::GalaxyInfo, v4d::noise::GalaxyInfo::rotation) + 32, VK_FORMAT_R32G32B32A32_SFLOAT}, // mat4
				// {16, offsetof(Galaxy, Galaxy::galaxyInfo) + offsetof(v4d::noise::GalaxyInfo, v4d::noise::GalaxyInfo::rotation) + 48, VK_FORMAT_R32G32B32A32_SFLOAT}, // mat4
			});
		// }
		
		
		
		// Galaxy Box
		auto* galaxyBoxDescriptorSet_1 = descriptorSets.emplace_back(new DescriptorSet(1));
		galaxyBoxDescriptorSet_1->AddBinding_combinedImageSampler(0, &galaxyCubeMapImage, VK_SHADER_STAGE_FRAGMENT_BIT);
		galaxyBoxPipelineLayout.AddDescriptorSet(baseDescriptorSet_0);
		galaxyBoxPipelineLayout.AddDescriptorSet(galaxyBoxDescriptorSet_1);
		galaxyBoxPipelineLayout.AddPushConstant<GalaxyBoxPushConstant>(VK_SHADER_STAGE_VERTEX_BIT);
		
	}
	
	void ConfigureShaders() {
		
		// Galaxy Gen
		// if (useRayTracingForGalaxy) {
			shaderBindingTable.AddMissShader("incubator_rendering/assets/shaders/rtx_galaxies.rmiss");
			galaxiesRayTracingShaderOffset = shaderBindingTable.AddHitShader("incubator_rendering/assets/shaders/rtx_galaxies.rchit", "", "incubator_rendering/assets/shaders/rtx_galaxies.rint");
		// } else {
			galaxyGenShader.inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
			galaxyGenShader.depthStencilState.depthWriteEnable = VK_FALSE;
			galaxyGenShader.depthStencilState.depthTestEnable = VK_FALSE;
		// }
		
		// // Galaxy Fade
		// galaxyFadeShader.inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
		// galaxyFadeShader.depthStencilState.depthWriteEnable = VK_FALSE;
		// galaxyFadeShader.depthStencilState.depthTestEnable = VK_FALSE;
		// galaxyFadeShader.SetData(6);
		
		// Galaxy Box
		galaxyBoxShader.inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
		galaxyBoxShader.depthStencilState.depthWriteEnable = VK_FALSE;
		galaxyBoxShader.depthStencilState.depthTestEnable = VK_FALSE;
		galaxyBoxShader.SetData(4);
		
	}
	
	void ReadShaders() {
		galaxyGenShader.ReadShaders();
		galaxyBoxShader.ReadShaders();
		galaxyFadeShader.ReadShaders();
		
		// if (useRayTracingForGalaxy) {
			shaderBindingTable.ReadShaders();
		// }
	}
	
	void CreateResources(Renderer* renderer, Device* renderingDevice, float screenWidth, float screenHeight) {
		uint bgSize = (uint)std::max(screenWidth, screenHeight);
		
		galaxyCubeMapImage.Create(renderingDevice, bgSize, bgSize);
		galaxyDepthStencilCubeMapImage.viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
		galaxyDepthStencilCubeMapImage.Create(renderingDevice, bgSize, bgSize);
		
		renderer->TransitionImageLayout(galaxyCubeMapImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
	}
	
	void DestroyResources(Device* renderingDevice) {
		galaxyCubeMapImage.Destroy(renderingDevice);
		galaxyDepthStencilCubeMapImage.Destroy(renderingDevice);
	}
	
	void AllocateBuffers(Device* renderingDevice) {
		galaxiesBuffer.Allocate(renderingDevice, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, false);
		galaxiesBuffer.MapMemory(renderingDevice);
	}
	
	void FreeBuffers(Renderer* renderer, Device* renderingDevice) {
		galaxiesBuffer.UnmapMemory(renderingDevice);
		galaxiesBuffer.Free(renderingDevice);
		galaxiesGenerated = false;
		
		// if (useRayTracingForGalaxy) 
			DestroyRayTracingAccelerationStructures(renderer, renderingDevice);
	}
	
	void CreatePipelines(Renderer* renderer, Device* renderingDevice, std::vector<RasterShaderPipeline*>& opaqueLightingShaders) {
		galaxyGenPipelineLayout.Create(renderingDevice);
		galaxyBoxPipelineLayout.Create(renderingDevice);
		
		opaqueLightingShaders.push_back(&galaxyBoxShader);
		
		// if (useRayTracingForGalaxy) {
			CreateRayTracingPipeline(renderer, renderingDevice);
		// } else {// Galaxy Gen render pass
			// Color Attachment (Fragment shader Standard Output)
			VkAttachmentDescription colorAttachment = {};
				colorAttachment.format = galaxyCubeMapImage.format;
				colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
				// Color and depth data
				colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
				colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
				// Stencil data
				colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
				colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
				colorAttachment.initialLayout = VK_IMAGE_LAYOUT_GENERAL;
				colorAttachment.finalLayout = VK_IMAGE_LAYOUT_GENERAL;
			VkAttachmentReference colorAttachmentRef = {
				galaxyGenRenderPass.AddAttachment(colorAttachment),
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
			};
			// SubPass
			VkSubpassDescription subpass = {};
				subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
				subpass.colorAttachmentCount = 1;
				subpass.pColorAttachments = &colorAttachmentRef;
			galaxyGenRenderPass.AddSubpass(subpass);
			
			// Create the render pass
			galaxyGenRenderPass.Create(renderingDevice);
			galaxyGenRenderPass.CreateFrameBuffers(renderingDevice, galaxyCubeMapImage);
			
			// Shader pipeline
			galaxyGenShader.SetRenderPass(&galaxyCubeMapImage, galaxyGenRenderPass.handle, 0);
			galaxyGenShader.AddColorBlendAttachmentState(
				/*blendEnable*/				VK_TRUE,
				/*srcColorBlendFactor*/		VK_BLEND_FACTOR_SRC_ALPHA,
				/*dstColorBlendFactor*/		VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
				/*colorBlendOp*/			VK_BLEND_OP_MAX,
				/*srcAlphaBlendFactor*/		VK_BLEND_FACTOR_ONE,
				/*dstAlphaBlendFactor*/		VK_BLEND_FACTOR_ONE,
				/*alphaBlendOp*/			VK_BLEND_OP_MAX
			);
			galaxyGenShader.CreatePipeline(renderingDevice);
		// }
		
		// {// Galaxy Fade render pass
		// 	// Color Attachment (Fragment shader Standard Output)
		// 	VkAttachmentDescription colorAttachment = {};
		// 		colorAttachment.format = galaxyCubeMapImage.format;
		// 		colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
		// 		// Color and depth data
		// 		colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
		// 		colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		// 		// Stencil data
		// 		colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		// 		colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		// 		colorAttachment.initialLayout = VK_IMAGE_LAYOUT_GENERAL;
		// 		colorAttachment.finalLayout = VK_IMAGE_LAYOUT_GENERAL;
		// 	VkAttachmentReference colorAttachmentRef = {
		// 		galaxyFadeRenderPass.AddAttachment(colorAttachment),
		// 		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
		// 	};
		// 	// SubPass
		// 	VkSubpassDescription subpass = {};
		// 		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		// 		subpass.colorAttachmentCount = 1;
		// 		subpass.pColorAttachments = &colorAttachmentRef;
		// 		subpass.pDepthStencilAttachment = nullptr;
		// 	galaxyFadeRenderPass.AddSubpass(subpass);
			
		// 	// Create the render pass
		// 	galaxyFadeRenderPass.Create(renderingDevice);
		// 	galaxyFadeRenderPass.CreateFrameBuffers(renderingDevice, galaxyCubeMapImage);
			
		// 	// Shader pipeline
		// 	galaxyFadeShader.SetRenderPass(&galaxyCubeMapImage, galaxyFadeRenderPass.handle, 0);
		// 	galaxyFadeShader.AddColorBlendAttachmentState(
		// 		/*blendEnable*/				VK_TRUE,
		// 		/*srcColorBlendFactor*/		VK_BLEND_FACTOR_ONE,
		// 		/*dstColorBlendFactor*/		VK_BLEND_FACTOR_ONE,
		// 		/*colorBlendOp*/			VK_BLEND_OP_REVERSE_SUBTRACT,
		// 		/*srcAlphaBlendFactor*/		VK_BLEND_FACTOR_ONE,
		// 		/*dstAlphaBlendFactor*/		VK_BLEND_FACTOR_ONE,
		// 		/*alphaBlendOp*/			VK_BLEND_OP_MAX
		// 	);
		// 	// Shader stages
		// 	galaxyFadeShader.CreatePipeline(renderingDevice);
		// }
		
	}
	
	void DestroyPipelines(Renderer* renderer, Device* renderingDevice) {
		// Galaxy Gen
		// if (useRayTracingForGalaxy) {
			DestroyRayTracingPipeline(renderer, renderingDevice);
		// } else {
			galaxyGenShader.DestroyPipeline(renderingDevice);
			galaxyGenRenderPass.DestroyFrameBuffers(renderingDevice);
			galaxyGenRenderPass.Destroy(renderingDevice);
		// }
		
		// // Galaxy Fade
		// galaxyFadeShader.DestroyPipeline(renderingDevice);
		// galaxyFadeRenderPass.DestroyFrameBuffers(renderingDevice);
		// galaxyFadeRenderPass.Destroy(renderingDevice);
		
		galaxyBoxShader.DestroyPipeline(renderingDevice);
		
		galaxyGenPipelineLayout.Destroy(renderingDevice);
		galaxyBoxPipelineLayout.Destroy(renderingDevice);
	}
	
	void RecordGraphicsCommandBuffer(VkCommandBuffer commandBuffer, int imageIndex) {
		// After swapchain recreation, we need to reset this
		galaxyFrameIndex = 0;
	}
	
	void RunInOpaqueLightingPass(Device* renderingDevice, VkCommandBuffer commandBuffer) {
		galaxyBoxShader.Execute(renderingDevice, commandBuffer, &galaxyBoxPushConstant);
	}
	
	void RunLowPriorityGraphics(Device* renderingDevice, VkCommandBuffer commandBuffer) {
		
		// if (useRayTracingForGalaxy) {
			
			if (galaxyFrameIndex == 0) {
				renderingDevice->CmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_NV, shaderBindingTable.GetPipeline());
				shaderBindingTable.GetPipelineLayout()->Bind(renderingDevice, commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_NV);
				
				VkDeviceSize bindingStride = rayTracingProperties.shaderGroupHandleSize;
				VkDeviceSize bindingOffsetMissShader = bindingStride * shaderBindingTable.GetMissGroupOffset();
				VkDeviceSize bindingOffsetHitShader = bindingStride * shaderBindingTable.GetHitGroupOffset();
				
				// Push constant
				auto pushConstantRange = shaderBindingTable.GetPipelineLayout()->pushConstants[0];
				renderingDevice->CmdPushConstants(commandBuffer, shaderBindingTable.GetPipelineLayout()->handle, pushConstantRange.stageFlags, pushConstantRange.offset, pushConstantRange.size, &galaxyGenPushConstant);
				
				renderingDevice->CmdTraceRaysNV(
					commandBuffer, 
					rayTracingShaderBindingTableBuffer.buffer, 0,
					rayTracingShaderBindingTableBuffer.buffer, bindingOffsetMissShader, bindingStride,
					rayTracingShaderBindingTableBuffer.buffer, bindingOffsetHitShader, bindingStride,
					VK_NULL_HANDLE, 0, 0,
					galaxyCubeMapImage.width, galaxyCubeMapImage.height, galaxyCubeMapImage.arrayLayers
				);
				
			}
			
			galaxyFrameIndex++;
			
		// } else {
			
			if (continuousGalaxyGen/* || galaxyFrameIndex < galaxyConvergences*/) {
				// Galaxy Gen
				galaxyGenRenderPass.Begin(renderingDevice, commandBuffer, galaxyCubeMapImage);
				galaxyGenShader.Execute(renderingDevice, commandBuffer, &galaxyGenPushConstant);
				galaxyGenRenderPass.End(renderingDevice, commandBuffer);
				// // Galaxy Fade
				// galaxyFadeRenderPass.Begin(renderingDevice, commandBuffer, galaxyCubeMapImage);
				// galaxyFadeShader.Execute(renderingDevice, commandBuffer);
				// galaxyFadeRenderPass.End(renderingDevice, commandBuffer);
			}
		// }
	}
	
	void FrameUpdate(Renderer* renderer, Device* renderingDevice, Camera& mainCamera) {
		// Update Push Constants
		galaxyBoxPushConstant.inverseProjectionView = glm::inverse(mainCamera.GetProjectionViewMatrix());
	}
	
	void LowPriorityFrameUpdate(Renderer* renderer, Device* renderingDevice, Camera& mainCamera, Queue& lowPriorityGraphicsQueue) {
		
		// Generate galaxies
		if (!galaxiesGenerated) {
			galaxyFrameIndex = 0;
			if (galaxies.size() == 0) {
				galaxies.reserve(MAX_GALAXIES);
				// Galaxy Gen (temporary stuff)
				for (int gridX = -1; gridX <= 1; ++gridX) {
					for (int gridY = -1; gridY <= 1; ++gridY) {
						for (int gridZ = -1; gridZ <= 1; ++gridZ) {
							int subGridSize = v4d::noise::UniverseSubGridSize({gridX,gridY,gridZ});
							for (int x = 0; x < subGridSize; ++x) {
								for (int y = 0; y < subGridSize; ++y) {
									for (int z = 0; z < subGridSize; ++z) {
										glm::vec3 galaxyPositionInGrid = {
											float(gridX) + float(x)/float(subGridSize)/2.0f,
											float(gridY) + float(y)/float(subGridSize)/2.0f,
											float(gridZ) + float(z)/float(subGridSize)/2.0f
										};
										float galaxySizeFactor = v4d::noise::GalaxySizeFactorInUniverseGrid(galaxyPositionInGrid);
										if (galaxySizeFactor > 0.0f) {
											// For each existing galaxy
											glm::vec3 pos = galaxyPositionInGrid + v4d::noise::Noise3(galaxyPositionInGrid)/float(subGridSize)/2.0f;
											galaxies.push_back({
												glm::vec4(pos, galaxySizeFactor/subGridSize/2.0f) * 100.0f,
												// v4d::noise::GetGalaxyInfo(pos)
											});
										}
									}
								}
							}
						}
					}
				}
				galaxies.shrink_to_fit();
				LOG("Number of Galaxies generated : " << galaxies.size())
				
				// Sort galaxies (closest first)
				glm::vec3 cameraPosition = mainCamera.GetWorldPosition();
				std::sort(galaxies.begin(), galaxies.end(), [&cameraPosition](Galaxy& g1, Galaxy& g2){
					return g1.GetDistanceFrom(cameraPosition) < g2.GetDistanceFrom(cameraPosition);
				});
				
			}
			
			const int maxGalaxies = 800;
			activeGalaxies.clear();
			activeGalaxies.reserve(maxGalaxies);
			for (auto& galaxy : galaxies) {
				if (activeGalaxies.size() >= maxGalaxies) break;
				activeGalaxies.push_back(galaxy);
			}
			
			std::vector<Galaxy> rtGalaxies = activeGalaxies;
			
			// Copy galaxies data to buffer
			galaxiesBuffer.WriteToMappedData(renderingDevice, rtGalaxies.data(), rtGalaxies.size()*sizeof(Galaxy));
			
			// if (useRayTracingForGalaxy) {
				DestroyRayTracingAccelerationStructures(renderer, renderingDevice);
				
				rayTracingBottomLevelAccelerationStructures.clear();
				for (auto* geometry : geometries) delete geometry;
				geometries.clear();
				auto* galaxiesGeometry = new ProceduralGeometry<Galaxy>(rtGalaxies, &galaxiesBuffer);
				geometries.push_back(galaxiesGeometry);
				rayTracingBottomLevelAccelerationStructures.resize(1);
				rayTracingBottomLevelAccelerationStructures[0].geometries.push_back(galaxiesGeometry);

				// Assign instances
				glm::mat3x4 transform {
					1.0f, 0.0f, 0.0f, 0.0f,
					0.0f, 1.0f, 0.0f, 0.0f,
					0.0f, 0.0f, 1.0f, 0.0f
				};
				geometryInstances.clear();
				geometryInstances.reserve(1);
				// Spheres instance
				geometryInstances.push_back({
					transform,
					0, // instanceId
					0xff, // mask
					galaxiesRayTracingShaderOffset, // instanceOffset
					0, // flags
					&rayTracingBottomLevelAccelerationStructures[0]
				});
	
				CreateRayTracingAccelerationStructures(renderer, renderingDevice, lowPriorityGraphicsQueue);
				renderer->UpdateDescriptorSets({galaxyRayTracingDescriptorSet});
			
			// } else {
				galaxyGenShader.SetData(&galaxiesBuffer, activeGalaxies.size());
			// }
			
			galaxiesGenerated = true;
		}
	
		// Galaxy convergence
		// if (!useRayTracingForGalaxy) galaxyFrameIndex++;
		// if (galaxyFrameIndex > galaxyConvergences) galaxyFrameIndex = continuousGalaxyGen? 0 : galaxyConvergences;
		
		if (glm::distance(mainCamera.GetWorldPosition(), positionInUniverse) > 0.0) {
			// // if (!useRayTracingForGalaxy) {
			// 	VkClearColorValue clearColor {.0,.0,.0,.0};
			// 	VkImageSubresourceRange clearRange {VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,galaxyCubeMapImage.arrayLayers};
			// 	auto cmdBuffer = BeginSingleTimeCommands(lowPriorityGraphicsQueue);
			// 	TransitionImageLayout(cmdBuffer, galaxyCubeMapImage, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
			// 	renderingDevice->CmdClearColorImage(cmdBuffer, galaxyCubeMapImage.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearColor, 1, &clearRange);
			// 	TransitionImageLayout(cmdBuffer, galaxyCubeMapImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL);
			// 	EndSingleTimeCommands(lowPriorityGraphicsQueue, cmdBuffer);
			// // }
			galaxyFrameIndex = 0;
			positionInUniverse = mainCamera.GetWorldPosition();
		}
		
		// Update Push Constants
		galaxyGenPushConstant.cameraPosition = mainCamera.GetWorldPosition();
		galaxyGenPushConstant.frameIndex = galaxyFrameIndex;
		galaxyGenPushConstant.resolution = galaxyCubeMapImage.width;
	}
	
	
	
	
};
