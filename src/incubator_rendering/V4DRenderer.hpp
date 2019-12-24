#pragma once
#include <v4d.h>
#include "V4DRenderingPipeline.hh"
#include "Camera.hpp"
#include "../incubator_rendering/helpers/Geometry.hpp"

#include "../incubator_galaxy4d/Noise.hpp"
// #include "../incubator_galaxy4d/UniversalPosition.hpp"

using namespace v4d::graphics;
using namespace v4d::graphics::vulkan::rtx;

class V4DRenderer : public v4d::graphics::Renderer {
	using v4d::graphics::Renderer::Renderer;
	
	#pragma region Buffers
	Buffer cameraUniformBuffer {VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, sizeof(CameraUBO), true};
	std::vector<Buffer*> stagedBuffers {};
	#pragma endregion
	
	#pragma region UI
	float uiImageScale = 1.0;
	Image uiImage { VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT ,1,1, { VK_FORMAT_R8G8B8A8_SNORM }};
	RenderPass uiRenderPass;
	PipelineLayout uiPipelineLayout;
	std::vector<RasterShaderPipeline*> uiShaders {};
	#pragma endregion
	
	#pragma region Galaxy rendering
	
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
	
	#pragma endregion
	
	#pragma region Standard Pipelines
	PipelineLayout standardPipelineLayout;
	#pragma endregion
	
	#pragma region Render passes
	RenderPass	opaqueRasterPass,
				opaqueLightingPass,
				transparentRasterPass,
				transparentLightingPass,
				thumbnailRenderPass,
				postProcessingRenderPass;
	#pragma endregion
	
	#pragma region Shaders
	
	std::vector<RasterShaderPipeline*> opaqueRasterizationShaders {};
	std::vector<RasterShaderPipeline*> transparentRasterizationShaders {};
	PipelineLayout lightingPipelineLayout, postProcessingPipelineLayout, thumbnailPipelineLayout;
	RasterShaderPipeline opaqueLightingShader {lightingPipelineLayout, {
		"incubator_rendering/assets/shaders/v4d_lighting.vert",
		"incubator_rendering/assets/shaders/v4d_lighting.opaque.frag",
	}};
	RasterShaderPipeline transparentLightingShader {lightingPipelineLayout, {
		"incubator_rendering/assets/shaders/v4d_lighting.vert",
		"incubator_rendering/assets/shaders/v4d_lighting.transparent.frag",
	}};
	RasterShaderPipeline postProcessingShader {postProcessingPipelineLayout, {
		"incubator_rendering/assets/shaders/v4d_postProcessing.vert",
		"incubator_rendering/assets/shaders/v4d_postProcessing.frag",
	}};
	RasterShaderPipeline thumbnailShader {thumbnailPipelineLayout, {
		"incubator_rendering/assets/shaders/v4d_thumbnail.vert",
		"incubator_rendering/assets/shaders/v4d_thumbnail.frag",
	}};
	
	#pragma endregion
	
	#pragma region Temporary galaxy stuff
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
	#pragma endregion
	
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

private: // Ray Tracing stuff
	ShaderBindingTable shaderBindingTable {galaxyGenPipelineLayout, "incubator_rendering/assets/shaders/rtx_galaxies.rgen"};
	std::vector<Geometry*> geometries {};
	std::vector<GeometryInstance> geometryInstances {};
	uint32_t galaxiesRayTracingShaderOffset;
	
	const bool useRayTracingForGalaxy = true;
	
	DescriptorSet* galaxyRayTracingDescriptorSet = nullptr;

	std::vector<BottomLevelAccelerationStructure> rayTracingBottomLevelAccelerationStructures {};
	TopLevelAccelerationStructure rayTracingTopLevelAccelerationStructure {};
	Buffer rayTracingShaderBindingTableBuffer {VK_BUFFER_USAGE_RAY_TRACING_BIT_NV};
	VkPhysicalDeviceRayTracingPropertiesNV rayTracingProperties{};
	float rayTracingImageScale = 1;
	
	void RayTracingInit() {
		RequiredDeviceExtension(VK_NV_RAY_TRACING_EXTENSION_NAME); // NVidia's RayTracing extension
		RequiredDeviceExtension(VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME); // Needed for RayTracing extension
	}
	void RayTracingInfo() {
		// Query the ray tracing properties of the current implementation
		rayTracingProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PROPERTIES_NV;
		VkPhysicalDeviceProperties2 deviceProps2{};
		deviceProps2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
		deviceProps2.pNext = &rayTracingProperties;
		GetPhysicalDeviceProperties2(renderingDevice->GetPhysicalDevice()->GetHandle(), &deviceProps2);
	}
	void CreateRayTracingAccelerationStructures() {
		
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
			AllocateBufferStaged(lowPriorityGraphicsQueue, instanceBuffer);
			
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
			
			auto cmdBuffer = BeginSingleTimeCommands(lowPriorityGraphicsQueue);
				
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
			
			EndSingleTimeCommands(lowPriorityGraphicsQueue, cmdBuffer);
			
			scratchBuffer.Free(renderingDevice);
			instanceBuffer.Free(renderingDevice);
		}
	}
	void DestroyRayTracingAccelerationStructures() {
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
	void CreateRayTracingPipeline() {
		shaderBindingTable.CreateRayTracingPipeline(renderingDevice);
		
		// Shader Binding Table
		rayTracingShaderBindingTableBuffer.size = rayTracingProperties.shaderGroupHandleSize * shaderBindingTable.GetGroups().size();
		rayTracingShaderBindingTableBuffer.Allocate(renderingDevice, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
		shaderBindingTable.WriteShaderBindingTableToBuffer(renderingDevice, &rayTracingShaderBindingTableBuffer, rayTracingProperties.shaderGroupHandleSize);
	}
	void DestroyRayTracingPipeline() {
		// Shader binding table
		rayTracingShaderBindingTableBuffer.Free(renderingDevice);
		// Ray tracing pipeline
		shaderBindingTable.DestroyRayTracingPipeline(renderingDevice);
	}
	
	
public: // Camera
	Camera mainCamera;
	
private: // Init
	void ScorePhysicalDeviceSelection(int& score, PhysicalDevice* physicalDevice) override {}
	void Init() override {
		RayTracingInit();
	}
	void Info() override {
		RayTracingInfo();
	}

	void InitLayouts() override {
		
		if (useRayTracingForGalaxy) {
			// Ray Tracing
			galaxyRayTracingDescriptorSet = descriptorSets.emplace_back(new DescriptorSet(0));
			galaxyRayTracingDescriptorSet->AddBinding_accelerationStructure(0, &rayTracingTopLevelAccelerationStructure.accelerationStructure, VK_SHADER_STAGE_RAYGEN_BIT_NV | VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV);
			galaxyRayTracingDescriptorSet->AddBinding_imageView(1, &galaxyCubeMapImage.view, VK_SHADER_STAGE_RAYGEN_BIT_NV);
			galaxyRayTracingDescriptorSet->AddBinding_storageBuffer(2, &galaxiesBuffer, VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV | VK_SHADER_STAGE_INTERSECTION_BIT_NV);
			galaxyGenPipelineLayout.AddDescriptorSet(galaxyRayTracingDescriptorSet);
			galaxyGenPipelineLayout.AddPushConstant<GalaxyGenPushConstant>(VK_SHADER_STAGE_RAYGEN_BIT_NV | VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV);
		} else {
			// Galaxy Gen
			galaxyGenPipelineLayout.AddPushConstant<GalaxyGenPushConstant>(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_GEOMETRY_BIT);
			galaxyGenShader.AddVertexInputBinding(sizeof(Galaxy), VK_VERTEX_INPUT_RATE_VERTEX, {
				{0, 8, VK_FORMAT_R32G32B32A32_SFLOAT},
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
		}
		
		// Base descriptor set containing CameraUBO and such, almost all shaders should use it
		auto* baseDescriptorSet_0 = descriptorSets.emplace_back(new DescriptorSet(0));
		baseDescriptorSet_0->AddBinding_uniformBuffer(0, &cameraUniformBuffer, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_GEOMETRY_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
		
		// Galaxy Box
		auto* galaxyBoxDescriptorSet_1 = descriptorSets.emplace_back(new DescriptorSet(1));
		galaxyBoxDescriptorSet_1->AddBinding_combinedImageSampler(0, &galaxyCubeMapImage, VK_SHADER_STAGE_FRAGMENT_BIT);
		galaxyBoxPipelineLayout.AddDescriptorSet(baseDescriptorSet_0);
		galaxyBoxPipelineLayout.AddDescriptorSet(galaxyBoxDescriptorSet_1);
		galaxyBoxPipelineLayout.AddPushConstant<GalaxyBoxPushConstant>(VK_SHADER_STAGE_VERTEX_BIT);
		
		// Standard pipeline
		//TODO standardPipelineLayout
		
		// Lighting pass
		auto* gBuffersDescriptorSet_1 = descriptorSets.emplace_back(new DescriptorSet(1));
		for (int i = 0; i < Camera::GBUFFER_NB_IMAGES; ++i)
			gBuffersDescriptorSet_1->AddBinding_inputAttachment(i, &mainCamera.GetGBuffer(i).view, VK_SHADER_STAGE_FRAGMENT_BIT);
		lightingPipelineLayout.AddDescriptorSet(baseDescriptorSet_0);
		lightingPipelineLayout.AddDescriptorSet(gBuffersDescriptorSet_1);
		
		// Thumbnail Gen
		auto* thumbnailDescriptorSet_1 = descriptorSets.emplace_back(new DescriptorSet(1));
		thumbnailDescriptorSet_1->AddBinding_combinedImageSampler(0, &mainCamera.GetTmpImage(), VK_SHADER_STAGE_FRAGMENT_BIT);
		thumbnailPipelineLayout.AddDescriptorSet(baseDescriptorSet_0);
		thumbnailPipelineLayout.AddDescriptorSet(thumbnailDescriptorSet_1);
		
		// Post-Processing
		auto* postProcessingDescriptorSet_1 = descriptorSets.emplace_back(new DescriptorSet(1));
		postProcessingDescriptorSet_1->AddBinding_combinedImageSampler(0, &mainCamera.GetTmpImage(), VK_SHADER_STAGE_FRAGMENT_BIT);
		postProcessingDescriptorSet_1->AddBinding_combinedImageSampler(1, &uiImage, VK_SHADER_STAGE_FRAGMENT_BIT);
		postProcessingPipelineLayout.AddDescriptorSet(baseDescriptorSet_0);
		postProcessingPipelineLayout.AddDescriptorSet(postProcessingDescriptorSet_1);
		
		// UI
		//TODO uiPipelineLayout
		
	}
	
	void ConfigureShaders() override {
		// Galaxy Gen
		if (useRayTracingForGalaxy) {
			shaderBindingTable.AddMissShader("incubator_rendering/assets/shaders/rtx_galaxies.rmiss");
			galaxiesRayTracingShaderOffset = shaderBindingTable.AddHitShader("incubator_rendering/assets/shaders/rtx_galaxies.rchit", "", "incubator_rendering/assets/shaders/rtx_galaxies.rint");
		} else {
			galaxyGenShader.inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
			galaxyGenShader.depthStencilState.depthWriteEnable = VK_FALSE;
			galaxyGenShader.depthStencilState.depthTestEnable = VK_FALSE;
		}
		
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
		
		// Standard pipeline
		//TODO opaqueRasterizationShaders, transparentRasterizationShaders
		
		// Lighting Passes
		opaqueLightingShader.inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
		opaqueLightingShader.depthStencilState.depthTestEnable = VK_FALSE;
		opaqueLightingShader.depthStencilState.depthWriteEnable = VK_FALSE;
		opaqueLightingShader.SetData(3);
		transparentLightingShader.inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
		transparentLightingShader.depthStencilState.depthTestEnable = VK_FALSE;
		transparentLightingShader.depthStencilState.depthWriteEnable = VK_FALSE;
		transparentLightingShader.SetData(3);
		
		// Thumbnail Gen
		thumbnailShader.inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
		thumbnailShader.depthStencilState.depthTestEnable = VK_FALSE;
		thumbnailShader.depthStencilState.depthWriteEnable = VK_FALSE;
		thumbnailShader.SetData(3);
		
		// Post-Processing
		postProcessingShader.inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
		postProcessingShader.depthStencilState.depthTestEnable = VK_FALSE;
		postProcessingShader.depthStencilState.depthWriteEnable = VK_FALSE;
		postProcessingShader.SetData(3);
		
		// UI
		//TODO uiShaders
		
	}
	
private: // Resources

	void CreateResources() override {
		float screenWidth = (float)swapChain->extent.width;
		float screenHeight = (float)swapChain->extent.height;
		uint bgSize = (uint)std::max(screenWidth, screenHeight);
		uint uiWidth = (uint)(screenWidth * uiImageScale);
		uint uiHeight = (uint)(screenHeight * uiImageScale);
		
		// Create images
		galaxyCubeMapImage.Create(renderingDevice, bgSize, bgSize);
		galaxyDepthStencilCubeMapImage.viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
		galaxyDepthStencilCubeMapImage.Create(renderingDevice, bgSize, bgSize);
		uiImage.Create(renderingDevice, uiWidth, uiHeight);
		mainCamera.SetRenderTarget(swapChain);
		mainCamera.CreateResources(renderingDevice);
		
		// Transition images
		TransitionImageLayout(galaxyCubeMapImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
		TransitionImageLayout(uiImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
		TransitionImageLayout(mainCamera.GetThumbnailImage(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
	}
	
	void DestroyResources() override {
		// Destroy images
		galaxyCubeMapImage.Destroy(renderingDevice);
		galaxyDepthStencilCubeMapImage.Destroy(renderingDevice);
		uiImage.Destroy(renderingDevice);
		mainCamera.DestroyResources(renderingDevice);
	}
	
	void AllocateBuffers() override {
		// Staged Buffers
		AllocateBuffersStaged(transferQueue, stagedBuffers);
		
		// Other buffers
		cameraUniformBuffer.Allocate(renderingDevice, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, false);
		cameraUniformBuffer.MapMemory(renderingDevice);
		galaxiesBuffer.Allocate(renderingDevice, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, false);
		galaxiesBuffer.MapMemory(renderingDevice);
	}
	
	void FreeBuffers() override {
		// Staged Buffers
		for (auto& buffer : stagedBuffers) {
			buffer->Free(renderingDevice);
		}
		
		// Other buffers
		cameraUniformBuffer.UnmapMemory(renderingDevice);
		cameraUniformBuffer.Free(renderingDevice);
		galaxiesBuffer.UnmapMemory(renderingDevice);
		galaxiesBuffer.Free(renderingDevice);
		galaxiesGenerated = false;
		
		if (useRayTracingForGalaxy) DestroyRayTracingAccelerationStructures();
	}

private: // Graphics configuration

	void CreatePipelines() override {
		galaxyGenPipelineLayout.Create(renderingDevice);
		galaxyBoxPipelineLayout.Create(renderingDevice);
		standardPipelineLayout.Create(renderingDevice);
		lightingPipelineLayout.Create(renderingDevice);
		thumbnailPipelineLayout.Create(renderingDevice);
		postProcessingPipelineLayout.Create(renderingDevice);
		uiPipelineLayout.Create(renderingDevice);
		
		if (useRayTracingForGalaxy) {
			CreateRayTracingPipeline();
		} else {// Galaxy Gen render pass
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
		}
		
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
		
		{// Opaque Raster pass
			std::array<VkAttachmentDescription, Camera::GBUFFER_NB_IMAGES> attachments {};
			std::array<VkAttachmentReference, Camera::GBUFFER_NB_IMAGES> colorAttachmentRefs {};
			for (int i = 0; i < Camera::GBUFFER_NB_IMAGES; ++i) {
				// Format
				attachments[i].format = mainCamera.GetGBuffer(i).format;
				attachments[i].samples = VK_SAMPLE_COUNT_1_BIT;
				// Color
				attachments[i].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
				attachments[i].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
				// Stencil
				attachments[i].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
				attachments[i].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
				// Layout
				attachments[i].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
				attachments[i].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				colorAttachmentRefs[i] = {
					opaqueRasterPass.AddAttachment(attachments[i]),
					VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
				};
			}
			
			// SubPass
			VkSubpassDescription subpass = {};
				subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
				subpass.colorAttachmentCount = colorAttachmentRefs.size();
				subpass.pColorAttachments = colorAttachmentRefs.data();
			opaqueRasterPass.AddSubpass(subpass);
			
			// Create the render pass
			opaqueRasterPass.Create(renderingDevice);
			opaqueRasterPass.CreateFrameBuffers(renderingDevice, mainCamera.GetGBuffers().data(), mainCamera.GetGBuffers().size());
			
			// Shaders
			for (auto* shaderPipeline : opaqueRasterizationShaders) {
				shaderPipeline->SetRenderPass(&mainCamera.GetGBuffer(0), opaqueRasterPass.handle, 0);
				for (int i = 0; i < Camera::GBUFFER_NB_IMAGES; ++i)
					shaderPipeline->AddColorBlendAttachmentState(VK_FALSE);
				shaderPipeline->CreatePipeline(renderingDevice);
			}
		}
		
		{// Opaque Lighting pass
			const int nbColorAttachments = 1;
			const int nbInputAttachments = Camera::GBUFFER_NB_IMAGES;
			std::array<VkAttachmentDescription, nbColorAttachments + nbInputAttachments> attachments {};
			std::vector<Image*> images(nbColorAttachments + nbInputAttachments);
			std::array<VkAttachmentReference, nbColorAttachments> colorAttachmentRefs;
			std::array<VkAttachmentReference, nbInputAttachments> inputAttachmentRefs;
			
			// Color attachment
			images[0] = &mainCamera.GetTmpImage();
			attachments[0].format = mainCamera.GetTmpImage().format;
			attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
			attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			attachments[0].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			colorAttachmentRefs[0] = {
				opaqueLightingPass.AddAttachment(attachments[0]),
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
			};
			
			// Input attachments
			for (int i = 0; i < nbInputAttachments; ++i) {
				images[i+nbColorAttachments] = &mainCamera.GetGBuffer(i);
				attachments[i+nbColorAttachments].format = mainCamera.GetGBuffer(i).format;
				attachments[i+nbColorAttachments].samples = VK_SAMPLE_COUNT_1_BIT;
				attachments[i+nbColorAttachments].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
				attachments[i+nbColorAttachments].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
				attachments[i+nbColorAttachments].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
				attachments[i+nbColorAttachments].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
				attachments[i+nbColorAttachments].initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				attachments[i+nbColorAttachments].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				inputAttachmentRefs[i] = {
					opaqueLightingPass.AddAttachment(attachments[i+nbColorAttachments]),
					VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
				};
			}
		
			// SubPass
			VkSubpassDescription subpass {};
				subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
				subpass.colorAttachmentCount = colorAttachmentRefs.size();
				subpass.pColorAttachments = colorAttachmentRefs.data();
				subpass.inputAttachmentCount = inputAttachmentRefs.size();
				subpass.pInputAttachments = inputAttachmentRefs.data();
			opaqueLightingPass.AddSubpass(subpass);
			
			// Create the render pass
			opaqueLightingPass.Create(renderingDevice);
			opaqueLightingPass.CreateFrameBuffers(renderingDevice, images);
			
			// Shaders
			for (auto* shaderPipeline : {&galaxyBoxShader, &opaqueLightingShader}) {
				shaderPipeline->SetRenderPass(&mainCamera.GetTmpImage(), opaqueLightingPass.handle, 0);
				shaderPipeline->AddColorBlendAttachmentState();
				shaderPipeline->CreatePipeline(renderingDevice);
			}
		}
		
		{// Transparent Raster pass
			std::array<VkAttachmentDescription, Camera::GBUFFER_NB_IMAGES> attachments {};
			std::array<VkAttachmentReference, Camera::GBUFFER_NB_IMAGES> colorAttachmentRefs {};
			for (int i = 0; i < Camera::GBUFFER_NB_IMAGES; ++i) {
				// Format
				attachments[i].format = mainCamera.GetGBuffer(i).format;
				attachments[i].samples = VK_SAMPLE_COUNT_1_BIT;
				// Color
				attachments[i].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
				attachments[i].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
				// Stencil
				attachments[i].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
				attachments[i].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
				// Layout
				attachments[i].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
				attachments[i].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				colorAttachmentRefs[i] = {
					transparentRasterPass.AddAttachment(attachments[i]),
					VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
				};
			}
			
			// SubPass
			VkSubpassDescription subpass = {};
				subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
				subpass.colorAttachmentCount = colorAttachmentRefs.size();
				subpass.pColorAttachments = colorAttachmentRefs.data();
			transparentRasterPass.AddSubpass(subpass);
			
			// Create the render pass
			transparentRasterPass.Create(renderingDevice);
			transparentRasterPass.CreateFrameBuffers(renderingDevice, mainCamera.GetGBuffers().data(), mainCamera.GetGBuffers().size());
			
			// Shaders
			for (auto* shaderPipeline : transparentRasterizationShaders) {
				shaderPipeline->SetRenderPass(&mainCamera.GetGBuffer(0), transparentRasterPass.handle, 0);
				for (int i = 0; i < Camera::GBUFFER_NB_IMAGES; ++i)
					shaderPipeline->AddColorBlendAttachmentState(VK_FALSE);
				shaderPipeline->CreatePipeline(renderingDevice);
			}
		}
		
		{// Transparent Lighting pass
			const int nbColorAttachments = 1;
			const int nbInputAttachments = Camera::GBUFFER_NB_IMAGES;
			std::array<VkAttachmentDescription, nbColorAttachments + nbInputAttachments> attachments {};
			std::vector<Image*> images(nbColorAttachments + nbInputAttachments);
			std::array<VkAttachmentReference, nbColorAttachments> colorAttachmentRefs;
			std::array<VkAttachmentReference, nbInputAttachments> inputAttachmentRefs;
			
			// Color attachment
			images[0] = &mainCamera.GetTmpImage();
			attachments[0].format = mainCamera.GetTmpImage().format;
			attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
			attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
			attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			attachments[0].initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			attachments[0].finalLayout = VK_IMAGE_LAYOUT_GENERAL;
			colorAttachmentRefs[0] = {
				transparentLightingPass.AddAttachment(attachments[0]),
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
			};
			
			// Input attachments
			for (int i = 0; i < nbInputAttachments; ++i) {
				images[i+nbColorAttachments] = &mainCamera.GetGBuffer(i);
				attachments[i+nbColorAttachments].format = mainCamera.GetGBuffer(i).format;
				attachments[i+nbColorAttachments].samples = VK_SAMPLE_COUNT_1_BIT;
				attachments[i+nbColorAttachments].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
				attachments[i+nbColorAttachments].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
				attachments[i+nbColorAttachments].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
				attachments[i+nbColorAttachments].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
				attachments[i+nbColorAttachments].initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				attachments[i+nbColorAttachments].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				inputAttachmentRefs[i] = {
					transparentLightingPass.AddAttachment(attachments[i+nbColorAttachments]),
					VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
				};
			}
		
			// SubPass
			VkSubpassDescription subpass {};
				subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
				subpass.colorAttachmentCount = colorAttachmentRefs.size();
				subpass.pColorAttachments = colorAttachmentRefs.data();
				subpass.inputAttachmentCount = inputAttachmentRefs.size();
				subpass.pInputAttachments = inputAttachmentRefs.data();
			transparentLightingPass.AddSubpass(subpass);
			
			// Create the render pass
			transparentLightingPass.Create(renderingDevice);
			transparentLightingPass.CreateFrameBuffers(renderingDevice, images);
			
			// Shaders
			for (auto* shaderPipeline : {&transparentLightingShader}) {
				shaderPipeline->SetRenderPass(&mainCamera.GetTmpImage(), transparentLightingPass.handle, 0);
				shaderPipeline->AddColorBlendAttachmentState();
				shaderPipeline->CreatePipeline(renderingDevice);
			}
		}
		
		{// Post Processing render pass
			VkAttachmentDescription colorAttachment = {};
				colorAttachment.format = swapChain->format.format;
				colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
				colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
				colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
				colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
				colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
			VkAttachmentReference colorAttachmentRef = {
				postProcessingRenderPass.AddAttachment(colorAttachment),
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
			};
			
			// SubPass
			VkSubpassDescription subpass = {};
				subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
				subpass.colorAttachmentCount = 1;
				subpass.pColorAttachments = &colorAttachmentRef;
			postProcessingRenderPass.AddSubpass(subpass);
			
			// Create the render pass
			postProcessingRenderPass.Create(renderingDevice);
			postProcessingRenderPass.CreateFrameBuffers(renderingDevice, swapChain);
			
			// Shaders
			for (auto* shaderPipeline : {&postProcessingShader}) {
				shaderPipeline->SetRenderPass(swapChain, postProcessingRenderPass.handle, 0);
				shaderPipeline->AddColorBlendAttachmentState();
				shaderPipeline->CreatePipeline(renderingDevice);
			}
		}
		
		{// Thumbnail Gen render pass
			VkAttachmentDescription colorAttachment = {};
				colorAttachment.format = mainCamera.GetThumbnailImage().format;
				colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
				colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
				colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
				colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
				colorAttachment.finalLayout = VK_IMAGE_LAYOUT_GENERAL;
			VkAttachmentReference colorAttachmentRef = {
				thumbnailRenderPass.AddAttachment(colorAttachment),
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
			};
			
			// SubPass
			VkSubpassDescription subpass = {};
				subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
				subpass.colorAttachmentCount = 1;
				subpass.pColorAttachments = &colorAttachmentRef;
			thumbnailRenderPass.AddSubpass(subpass);
			
			// Create the render pass
			thumbnailRenderPass.Create(renderingDevice);
			thumbnailRenderPass.CreateFrameBuffers(renderingDevice, mainCamera.GetThumbnailImage());
			
			// Shaders
			for (auto* shaderPipeline : {&thumbnailShader}) {
				shaderPipeline->SetRenderPass(&mainCamera.GetThumbnailImage(), thumbnailRenderPass.handle, 0);
				shaderPipeline->AddColorBlendAttachmentState();
				shaderPipeline->CreatePipeline(renderingDevice);
			}
		}
		
		{// UI render pass
			// Color Attachment (Fragment shader Standard Output)
			VkAttachmentDescription colorAttachment = {};
				colorAttachment.format = uiImage.format;
				colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
				// Color and depth data
				colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
				colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
				// Stencil data
				colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
				colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
				colorAttachment.initialLayout = VK_IMAGE_LAYOUT_GENERAL;
				colorAttachment.finalLayout = VK_IMAGE_LAYOUT_GENERAL;
			VkAttachmentReference colorAttachmentRef = {
				uiRenderPass.AddAttachment(colorAttachment),
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
			};
			// SubPass
			VkSubpassDescription subpass = {};
				subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
				subpass.colorAttachmentCount = 1;
				subpass.pColorAttachments = &colorAttachmentRef;
				subpass.pDepthStencilAttachment = nullptr;
			uiRenderPass.AddSubpass(subpass);
			
			// Create the render pass
			uiRenderPass.Create(renderingDevice);
			uiRenderPass.CreateFrameBuffers(renderingDevice, uiImage);
			
			// Shader pipeline
			for (auto* shader : uiShaders) {
				shader->SetRenderPass(&uiImage, uiRenderPass.handle, 0);
				shader->AddColorBlendAttachmentState();
				shader->CreatePipeline(renderingDevice);
			}
		}
		
	}
	
	void DestroyPipelines() override {
		// Galaxy Gen
		if (useRayTracingForGalaxy) {
			DestroyRayTracingPipeline();
		} else {
			galaxyGenShader.DestroyPipeline(renderingDevice);
			galaxyGenRenderPass.DestroyFrameBuffers(renderingDevice);
			galaxyGenRenderPass.Destroy(renderingDevice);
		}
		
		// // Galaxy Fade
		// galaxyFadeShader.DestroyPipeline(renderingDevice);
		// galaxyFadeRenderPass.DestroyFrameBuffers(renderingDevice);
		// galaxyFadeRenderPass.Destroy(renderingDevice);
		
		// Rasterization pipelines
		for (ShaderPipeline* shaderPipeline : opaqueRasterizationShaders) {
			shaderPipeline->DestroyPipeline(renderingDevice);
		}
		for (ShaderPipeline* shaderPipeline : transparentRasterizationShaders) {
			shaderPipeline->DestroyPipeline(renderingDevice);
		}
		opaqueRasterPass.DestroyFrameBuffers(renderingDevice);
		opaqueRasterPass.Destroy(renderingDevice);
		transparentRasterPass.DestroyFrameBuffers(renderingDevice);
		transparentRasterPass.Destroy(renderingDevice);
		
		// Lighting pipelines
		for (ShaderPipeline* shaderPipeline : {&galaxyBoxShader, &opaqueLightingShader, &transparentLightingShader}) {
			shaderPipeline->DestroyPipeline(renderingDevice);
		}
		opaqueLightingPass.DestroyFrameBuffers(renderingDevice);
		opaqueLightingPass.Destroy(renderingDevice);
		transparentLightingPass.DestroyFrameBuffers(renderingDevice);
		transparentLightingPass.Destroy(renderingDevice);
		
		// Thumbnail Gen
		for (auto* shaderPipeline : {&thumbnailShader}) {
			shaderPipeline->DestroyPipeline(renderingDevice);
		}
		thumbnailRenderPass.DestroyFrameBuffers(renderingDevice);
		thumbnailRenderPass.Destroy(renderingDevice);
		
		// Post-processing
		for (auto* shaderPipeline : {&postProcessingShader}) {
			shaderPipeline->DestroyPipeline(renderingDevice);
		}
		postProcessingRenderPass.DestroyFrameBuffers(renderingDevice);
		postProcessingRenderPass.Destroy(renderingDevice);
		
		// UI
		for (auto* shaderPipeline : uiShaders) {
			shaderPipeline->DestroyPipeline(renderingDevice);
		}
		uiRenderPass.DestroyFrameBuffers(renderingDevice);
		uiRenderPass.Destroy(renderingDevice);
		
		////////////////////////////
		// Pipeline layouts
		galaxyGenPipelineLayout.Destroy(renderingDevice);
		galaxyBoxPipelineLayout.Destroy(renderingDevice);
		standardPipelineLayout.Destroy(renderingDevice);
		lightingPipelineLayout.Destroy(renderingDevice);
		thumbnailPipelineLayout.Destroy(renderingDevice);
		postProcessingPipelineLayout.Destroy(renderingDevice);
		uiPipelineLayout.Destroy(renderingDevice);
	}
	
private: // Commands
	void RecordGraphicsCommandBuffer(VkCommandBuffer commandBuffer, int imageIndex) override {
		// Gen Thumbnail
		thumbnailRenderPass.Begin(renderingDevice, commandBuffer, mainCamera.GetThumbnailImage(), {{.0,.0,.0,.0}});
		thumbnailShader.Execute(renderingDevice, commandBuffer);
		thumbnailRenderPass.End(renderingDevice, commandBuffer);
		// Post Processing
		postProcessingRenderPass.Begin(renderingDevice, commandBuffer, swapChain, {{.0,.0,.0,.0}}, imageIndex);
		postProcessingShader.Execute(renderingDevice, commandBuffer);
		postProcessingRenderPass.End(renderingDevice, commandBuffer);
		
		// After swapchain recreation, we need to reset this
		galaxyFrameIndex = 0;
	}
	void RunDynamicGraphics(VkCommandBuffer commandBuffer) override {
		
		// Opaque Raster pass
		opaqueRasterPass.Begin(renderingDevice, commandBuffer, mainCamera.GetGBuffer(0), mainCamera.GetGBuffersClearValues());
			for (auto* shaderPipeline : opaqueRasterizationShaders) {
				shaderPipeline->Execute(renderingDevice, commandBuffer);
			}
		opaqueRasterPass.End(renderingDevice, commandBuffer);
	
		// Opaque Lighting pass
		opaqueLightingPass.Begin(renderingDevice, commandBuffer, mainCamera.GetTmpImage(), {{.0,.0,.0,.0}});
			galaxyBoxShader.Execute(renderingDevice, commandBuffer, &galaxyBoxPushConstant);
			opaqueLightingShader.Execute(renderingDevice, commandBuffer);
		opaqueLightingPass.End(renderingDevice, commandBuffer);
		
		// Transparent Raster pass
		transparentRasterPass.Begin(renderingDevice, commandBuffer, mainCamera.GetGBuffer(0), mainCamera.GetGBuffersClearValues());
			for (auto* shaderPipeline : transparentRasterizationShaders) {
				shaderPipeline->Execute(renderingDevice, commandBuffer);
			}
		transparentRasterPass.End(renderingDevice, commandBuffer);
		
		// Transparent Lighting pass
		transparentLightingPass.Begin(renderingDevice, commandBuffer, mainCamera.GetTmpImage());
			transparentLightingShader.Execute(renderingDevice, commandBuffer);
		transparentLightingPass.End(renderingDevice, commandBuffer);
	}
	void RunDynamicLowPriorityCompute(VkCommandBuffer) override {}
	void RunDynamicLowPriorityGraphics(VkCommandBuffer commandBuffer) override {
		// UI
		uiRenderPass.Begin(renderingDevice, commandBuffer, uiImage, {{.0,.0,.0,.0}});
		for (auto* shaderPipeline : uiShaders) {
			shaderPipeline->Execute(renderingDevice, commandBuffer);
		}
		uiRenderPass.End(renderingDevice, commandBuffer);
		
		if (useRayTracingForGalaxy) {
			
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
				
				galaxyFrameIndex++;
			}
			
		} else {
			
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
		}
	}
	
public: // Scene configuration
	void LoadScene() override {
		
	}
	
	void ReadShaders() override {
		galaxyGenShader.ReadShaders();
		galaxyBoxShader.ReadShaders();
		galaxyFadeShader.ReadShaders();
		thumbnailShader.ReadShaders();
		postProcessingShader.ReadShaders();
		opaqueLightingShader.ReadShaders();
		transparentLightingShader.ReadShaders();
		for (auto* shader : opaqueRasterizationShaders)
			shader->ReadShaders();
		for (auto* shader : transparentRasterizationShaders)
			shader->ReadShaders();
		for (auto* shader : uiShaders)
			shader->ReadShaders();
		if (useRayTracingForGalaxy) {
			shaderBindingTable.ReadShaders();
		}
	}
	
	void UnloadScene() override {
		// Staged buffers
		for (auto* buffer : stagedBuffers) {
			delete buffer;
		}
		stagedBuffers.clear();
	}
	
public: // Update
	void FrameUpdate(uint imageIndex) override {
		// Refresh camera matrices
		mainCamera.RefreshProjectionMatrix();
		mainCamera.RefreshViewMatrix();
		
		// Update and Write UBO
		mainCamera.RefreshProjectionMatrix();
		mainCamera.RefreshViewMatrix();
		mainCamera.RefreshUBO();
		cameraUniformBuffer.WriteToMappedData(renderingDevice, &mainCamera.GetUBO());
		
		// Update Push Constants
		galaxyBoxPushConstant.inverseProjectionView = glm::inverse(mainCamera.GetProjectionViewMatrix());
	}
	void LowPriorityFrameUpdate() override {
		
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
			}
			
			// Sort galaxies (closest first)
			glm::vec3 cameraPosition = mainCamera.GetWorldPosition();
			std::sort(galaxies.begin(), galaxies.end(), [&cameraPosition](Galaxy& g1, Galaxy& g2){
				return g1.GetDistanceFrom(cameraPosition) < g2.GetDistanceFrom(cameraPosition);
			});
			
			const int maxGalaxies = 10;
			activeGalaxies.clear();
			activeGalaxies.reserve(maxGalaxies);
			for (auto& galaxy : galaxies) {
				if (activeGalaxies.size() >= maxGalaxies) break;
				activeGalaxies.push_back(galaxy);
			}
			
			// Copy galaxies data to buffer
			galaxiesBuffer.WriteToMappedData(renderingDevice, activeGalaxies.data(), activeGalaxies.size()*sizeof(Galaxy));
			
			if (useRayTracingForGalaxy) {
				DestroyRayTracingAccelerationStructures();
				
				rayTracingBottomLevelAccelerationStructures.clear();
				for (auto* geometry : geometries) delete geometry;
				geometries.clear();
				auto* galaxiesGeometry = new ProceduralGeometry<Galaxy>(activeGalaxies, &galaxiesBuffer);
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
	
				CreateRayTracingAccelerationStructures();
				UpdateDescriptorSets({galaxyRayTracingDescriptorSet});
			
			} else {
				galaxyGenShader.SetData(&galaxiesBuffer, activeGalaxies.size());
			}
			
			galaxiesGenerated = true;
		}
		
		// Galaxy convergence
		if (!useRayTracingForGalaxy) galaxyFrameIndex++;
		// if (galaxyFrameIndex > galaxyConvergences) galaxyFrameIndex = continuousGalaxyGen? 0 : galaxyConvergences;
		
		if (glm::length(mainCamera.GetVelocity()) > 0.0) {
			if (!useRayTracingForGalaxy) {
				VkClearColorValue clearColor {.0,.0,.0,.0};
				VkImageSubresourceRange clearRange {VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,galaxyCubeMapImage.arrayLayers};
				auto cmdBuffer = BeginSingleTimeCommands(lowPriorityGraphicsQueue);
				TransitionImageLayout(cmdBuffer, galaxyCubeMapImage, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
				renderingDevice->CmdClearColorImage(cmdBuffer, galaxyCubeMapImage.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearColor, 1, &clearRange);
				TransitionImageLayout(cmdBuffer, galaxyCubeMapImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL);
				EndSingleTimeCommands(lowPriorityGraphicsQueue, cmdBuffer);
			}
			galaxyFrameIndex = 0;
		}
		
		// Update Push Constants
		galaxyGenPushConstant.cameraPosition = mainCamera.GetWorldPosition();
		galaxyGenPushConstant.frameIndex = galaxyFrameIndex;
		galaxyGenPushConstant.resolution = galaxyCubeMapImage.width;
	}
	
public: // ubo/conditional member variables
	// int galaxyConvergences = 100;
	bool continuousGalaxyGen = true;
	int galaxyFrameIndex = 0;
	
};
