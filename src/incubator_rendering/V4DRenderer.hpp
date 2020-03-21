#pragma once
#include <v4d.h>

#include "RenderTargetGroup.hpp"

using namespace v4d::graphics;
using namespace v4d::graphics::vulkan::rtx;

const uint32_t RAY_TRACING_TLAS_INITIAL_INSTANCES = 65536;
const uint32_t MAX_ACTIVE_LIGHTS = 256;

class V4DRenderer : public v4d::graphics::Renderer {
private: 
	using v4d::graphics::Renderer::Renderer;

	std::vector<v4d::modules::Rendering*> renderingSubmodules {};
	
	#pragma region Buffers
	StagedBuffer cameraUniformBuffer {VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, sizeof(Camera)};
	StagedBuffer activeLightsUniformBuffer {VK_BUFFER_USAGE_STORAGE_BUFFER_BIT};
		uint32_t nbActiveLights = 0;
		uint32_t activeLights[MAX_ACTIVE_LIGHTS];
	Buffer totalLuminance {VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, sizeof(glm::vec4)};
	#pragma endregion
	
	#pragma region UI
	float uiImageScale = 1.0;
	Image uiImage { VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT ,1,1, { VK_FORMAT_R8G8B8A8_SNORM }};
	#pragma endregion

	#pragma region Thumbnail/Histogram
	float thumbnailScale = 1.0/16;
	Image thumbnailImage { VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT };
	float exposureFactor = 1;
	#pragma endregion

	#pragma region Render passes
	RenderPass
				thumbnailRenderPass,
				postProcessingRenderPass,
				uiRenderPass;
	#pragma endregion
		
	#pragma region RayTracing structs

	struct AccelerationStructure {
		VkAccelerationStructureKHR accelerationStructure = VK_NULL_HANDLE;
		VkDeviceMemory memory = VK_NULL_HANDLE;
		uint64_t handle = 0;
		VkAccelerationStructureCreateInfoKHR createInfo {};
		std::vector<VkAccelerationStructureGeometryKHR> rayTracingGeometries {};
		std::vector<VkAccelerationStructureBuildOffsetInfoKHR> rayTracingGeometriesBuild {};
		std::vector<VkAccelerationStructureCreateGeometryTypeInfoKHR> rayTracingGeometriesCreate {};
		
		bool allowUpdate = false;
		Buffer scratchBuffer {VK_BUFFER_USAGE_RAY_TRACING_BIT_KHR};
		bool scratchBufferAllocated = false;
		bool built = false;
		
		VkDeviceSize GetMemoryRequirementsForScratchBuffer(Device* device) const {
			VkAccelerationStructureMemoryRequirementsInfoKHR memoryRequirementsInfo {};
			VkMemoryRequirements2 memoryRequirementsBuild {};
			memoryRequirementsInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_INFO_KHR;
			memoryRequirementsInfo.type = VK_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_TYPE_BUILD_SCRATCH_KHR;
			memoryRequirementsInfo.buildType = VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR;
			memoryRequirementsInfo.accelerationStructure = accelerationStructure;
			device->GetAccelerationStructureMemoryRequirementsKHR(&memoryRequirementsInfo, &memoryRequirementsBuild);
			if (allowUpdate) {
				VkMemoryRequirements2 memoryRequirementsUpdate {};
				memoryRequirementsInfo.type = VK_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_TYPE_UPDATE_SCRATCH_KHR;
				device->GetAccelerationStructureMemoryRequirementsKHR(&memoryRequirementsInfo, &memoryRequirementsUpdate);
				return std::max(memoryRequirementsBuild.memoryRequirements.size, memoryRequirementsUpdate.memoryRequirements.size);
			} else {
				return memoryRequirementsBuild.memoryRequirements.size;
			}
		}
	};

	struct BottomLevelAccelerationStructure : public AccelerationStructure {
		int nbGeometries = 0;
		
		void Build(Device* device, VkCommandBuffer cmdBuffer, const std::vector<Geometry*>& geometries, bool dynamicMesh = false) {
			
			rayTracingGeometries.clear();
			rayTracingGeometriesCreate.clear();
			rayTracingGeometriesBuild.clear();
			rayTracingGeometries.reserve(geometries.size());
			rayTracingGeometriesCreate.reserve(geometries.size());
			rayTracingGeometriesBuild.reserve(geometries.size());
			for (auto* geometry : geometries) if (geometry->active) {
				VkAccelerationStructureGeometryKHR geometryInfo {};
					geometryInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
					geometryInfo.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
					auto vertexBufferAddr = device->GetBufferDeviceOrHostAddressConst(Geometry::globalBuffers.vertexBuffer.deviceLocalBuffer.buffer);
					if (geometry->isProcedural) {
						geometryInfo.geometryType = VK_GEOMETRY_TYPE_AABBS_KHR;
						geometryInfo.geometry.aabbs.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_AABBS_DATA_KHR;
						geometryInfo.geometry.aabbs.stride = sizeof(Geometry::ProceduralVertexBuffer_T);
						geometryInfo.geometry.aabbs.data = vertexBufferAddr;
					} else {
						geometryInfo.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
						geometryInfo.geometry.triangles.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
						geometryInfo.geometry.triangles.vertexStride = sizeof(Geometry::VertexBuffer_T);
						geometryInfo.geometry.triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
						geometryInfo.geometry.triangles.indexType = sizeof(Geometry::IndexBuffer_T)==2? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32;
						geometryInfo.geometry.triangles.vertexData = vertexBufferAddr;
						geometryInfo.geometry.triangles.indexData = device->GetBufferDeviceOrHostAddressConst(Geometry::globalBuffers.indexBuffer.deviceLocalBuffer.buffer);
						if (geometry->transformBuffer) {
							geometryInfo.geometry.triangles.transformData = device->GetBufferDeviceOrHostAddressConst(geometry->transformBuffer->buffer);
						}
					}
				rayTracingGeometries.push_back(geometryInfo);
				rayTracingGeometriesCreate.push_back({
					VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_GEOMETRY_TYPE_INFO_KHR,
					nullptr,
					geometryInfo.geometryType,
					geometry->isProcedural? geometry->vertexCount : (geometry->indexCount/3),// maxPrimitiveCount
					geometry->isProcedural? VK_INDEX_TYPE_NONE_KHR : geometryInfo.geometry.triangles.indexType, // indexType
					geometry->isProcedural? 0 : geometry->vertexCount, // maxVertexCount
					geometry->isProcedural? VK_FORMAT_UNDEFINED : geometryInfo.geometry.triangles.vertexFormat, // vertexFormat
					(VkBool32)(geometry->transformBuffer? VK_TRUE : VK_FALSE)
				});
				rayTracingGeometriesBuild.push_back({
					geometry->isProcedural? geometry->vertexCount : (geometry->indexCount/3), // primitiveCount
					(uint32_t)(geometry->isProcedural? (geometry->vertexOffset*sizeof(Geometry::ProceduralVertexBuffer_T)) : (geometry->indexOffset*sizeof(Geometry::IndexBuffer_T))), // primitiveOffset
					geometry->isProcedural? 0 : geometry->vertexOffset, // firstVertex
					(uint32_t)geometry->transformOffset // transformOffset
				});
			}
			if (accelerationStructure != VK_NULL_HANDLE && nbGeometries != rayTracingGeometries.size()) {
				Destroy(device);
			}
			nbGeometries = rayTracingGeometries.size();
			
			if (accelerationStructure == VK_NULL_HANDLE) Create(device);
			
			// Scratch buffer
			if (!scratchBufferAllocated) {
				scratchBuffer.size = GetMemoryRequirementsForScratchBuffer(device);
				scratchBuffer.Allocate(device, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
				scratchBufferAllocated = true;
			}
			
			// Build bottom level acceleration structure
			const VkAccelerationStructureGeometryKHR* ppGeometries = rayTracingGeometries.data();
			const VkAccelerationStructureBuildOffsetInfoKHR* pGeometriesOffsets = rayTracingGeometriesBuild.data();
			
			VkAccelerationStructureBuildGeometryInfoKHR accelerationStructBuildInfo {};
				accelerationStructBuildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
				accelerationStructBuildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
				accelerationStructBuildInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
				if (allowUpdate) accelerationStructBuildInfo.flags |= VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR;
				accelerationStructBuildInfo.update = (allowUpdate && built && dynamicMesh)? VK_TRUE : VK_FALSE;
				accelerationStructBuildInfo.srcAccelerationStructure = accelerationStructure;
				accelerationStructBuildInfo.dstAccelerationStructure = accelerationStructure;
				accelerationStructBuildInfo.geometryCount = (uint)rayTracingGeometries.size();
				accelerationStructBuildInfo.ppGeometries = &ppGeometries;
				accelerationStructBuildInfo.geometryArrayOfPointers = VK_FALSE;
				accelerationStructBuildInfo.scratchData = device->GetBufferDeviceOrHostAddress(scratchBuffer.buffer);
			device->CmdBuildAccelerationStructureKHR(cmdBuffer, 1, &accelerationStructBuildInfo, &pGeometriesOffsets);
			
			// VkMemoryBarrier memoryBarrier {
			// 	VK_STRUCTURE_TYPE_MEMORY_BARRIER,
			// 	nullptr,// pNext
			// 	VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR | VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR,// VkAccessFlags srcAccessMask
			// 	VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR | VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR,// VkAccessFlags dstAccessMask
			// };
			// device->CmdPipelineBarrier(
			// 	cmdBuffer, 
			// 	VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, 
			// 	VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, 
			// 	0, 
			// 	1, &memoryBarrier, 
			// 	0, 0, 
			// 	0, 0
			// );
			
			if (!dynamicMesh) {
				scratchBuffer.Free(device);
				scratchBufferAllocated = false;
			}
			
			built = true;
		}
		
		void Create(Device* device) {
			uint32_t flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
			if (allowUpdate) flags |= VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR;
			
			const auto* rayTracingGeometriesCreates = rayTracingGeometriesCreate.data();
			createInfo = {
				VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
				nullptr,// pNext
				0,// VkDeviceSize compactedSize
				VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
				flags,
				(uint)rayTracingGeometries.size(),
				rayTracingGeometriesCreates,
				0 // VkDeviceAddress deviceAddress
			};
			
			if (device->CreateAccelerationStructureKHR(&createInfo, nullptr, &accelerationStructure) != VK_SUCCESS)
				throw std::runtime_error("Failed to create bottom level acceleration structure");
				
			// Allocate and bind memory for bottom level acceleration structure
			VkMemoryRequirements2 memoryRequirementsAS {};
			{
				VkAccelerationStructureMemoryRequirementsInfoKHR memoryRequirementsInfo {};
				memoryRequirementsInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_INFO_KHR;
				memoryRequirementsInfo.type = VK_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_TYPE_OBJECT_KHR;
				memoryRequirementsInfo.buildType = VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR;
				memoryRequirementsInfo.accelerationStructure = accelerationStructure;
				device->GetAccelerationStructureMemoryRequirementsKHR(&memoryRequirementsInfo, &memoryRequirementsAS);
			}
			VkMemoryAllocateInfo memoryAllocateInfo {
				VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
				nullptr,// pNext
				memoryRequirementsAS.memoryRequirements.size,// VkDeviceSize allocationSize
				device->GetPhysicalDevice()->FindMemoryType(memoryRequirementsAS.memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)// memoryTypeIndex
			};
			if (device->AllocateMemory(&memoryAllocateInfo, nullptr, &memory) != VK_SUCCESS)
				throw std::runtime_error("Failed to allocate memory for bottom level acceleration structure");
			VkBindAccelerationStructureMemoryInfoKHR accelerationStructureMemoryInfo {
				VK_STRUCTURE_TYPE_BIND_ACCELERATION_STRUCTURE_MEMORY_INFO_KHR,
				nullptr,// pNext
				accelerationStructure,// accelerationStructure
				memory,// memory
				0,// VkDeviceSize memoryOffset
				0,// uint32_t deviceIndexCount
				nullptr// pDeviceIndices
			};
			if (device->BindAccelerationStructureMemoryKHR(1, &accelerationStructureMemoryInfo) != VK_SUCCESS)
				throw std::runtime_error("Failed to bind bottom level acceleration structure memory");
				
			// Get bottom level acceleration structure handle for use in top level instances
			VkAccelerationStructureDeviceAddressInfoKHR devAddrInfo {};
			devAddrInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
			devAddrInfo.accelerationStructure = accelerationStructure;
			handle = device->GetAccelerationStructureDeviceAddressKHR(&devAddrInfo);
		}
		
		void Destroy(Device* device) {
			if (memory) device->FreeMemory(memory, nullptr);
			if (accelerationStructure) device->DestroyAccelerationStructureKHR(accelerationStructure, nullptr);
			accelerationStructure = VK_NULL_HANDLE;
			built = false;
			if (scratchBufferAllocated) {
				scratchBuffer.Free(device);
				scratchBufferAllocated = false;
			}
		}
		
	};

	struct RayTracingBLASInstance { // VkAccelerationStructureInstanceKHR
		glm::mat3x4 transform;
		uint32_t instanceId : 24;
		uint32_t mask : 8;
		uint32_t shaderInstanceOffset : 24;
		VkGeometryInstanceFlagsKHR flags : 8;
		uint64_t accelerationStructureHandle;
	};

	struct TopLevelAccelerationStructure : public AccelerationStructure {
		std::vector<RayTracingBLASInstance> instances {};
		
		StagedBuffer instanceBuffer {VK_BUFFER_USAGE_RAY_TRACING_BIT_KHR};
		bool instanceBufferAllocated = false;
		uint32_t nbInstances = 0;
		uint32_t maxInstances = RAY_TRACING_TLAS_INITIAL_INSTANCES;
		
		bool handleDirty = false; // to know when to update the descriptor set
		
		void Build(Device* device, VkCommandBuffer cmdBuffer, bool update = false) {
			if (instances.size() == 0) return;
			
			if (instances.size() > maxInstances) {
				LOG_WARN("Max Instances reached, recreating TLAS")
				maxInstances = instances.size();
				Destroy(device);
				Create(device);
				handleDirty = true;
			}
			
			// Scratch buffer
			if (!scratchBufferAllocated) {
				scratchBuffer.size = GetMemoryRequirementsForScratchBuffer(device);
				scratchBuffer.Allocate(device, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
				scratchBufferAllocated = true;
			}
			
			// Instance buffer
			if (instanceBufferAllocated && nbInstances != instances.size()) {
				instanceBuffer.Free(device);
				instanceBufferAllocated = false;
				built = false;
			}
			nbInstances = instances.size();
			instanceBuffer.ResetSrcData();
			instanceBuffer.AddSrcDataPtr(&instances);
			if (!instanceBufferAllocated) {
				instanceBuffer.Allocate(device);
				instanceBufferAllocated = true;
				rayTracingGeometries.resize(1);
				rayTracingGeometriesBuild.resize(1);
				rayTracingGeometriesBuild[0] = {
					nbInstances, // primitiveCount
					0, // primitiveOffset
					0, // firstVertex
					0 // transformOffset
				};
				VkAccelerationStructureGeometryKHR geometryInfo {};
					geometryInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
					geometryInfo.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
					geometryInfo.geometry.instances.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
					geometryInfo.geometry.instances.arrayOfPointers = VK_FALSE;
					geometryInfo.geometry.instances.data = device->GetBufferDeviceOrHostAddressConst(instanceBuffer.deviceLocalBuffer.buffer);
				rayTracingGeometries[0] = geometryInfo;
			}
		
			instanceBuffer.Update(device, cmdBuffer);
		
			const VkAccelerationStructureGeometryKHR* ppGeometries = rayTracingGeometries.data();
			const VkAccelerationStructureBuildOffsetInfoKHR* pGeometriesOffsets = rayTracingGeometriesBuild.data();
		
			VkAccelerationStructureBuildGeometryInfoKHR accelerationStructBuildInfo {};
				accelerationStructBuildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
				accelerationStructBuildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
				accelerationStructBuildInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
				if (allowUpdate) accelerationStructBuildInfo.flags |= VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR;
				accelerationStructBuildInfo.update = (allowUpdate && built && update)? VK_TRUE : VK_FALSE;
				accelerationStructBuildInfo.srcAccelerationStructure = accelerationStructure;
				accelerationStructBuildInfo.dstAccelerationStructure = accelerationStructure;
				accelerationStructBuildInfo.geometryCount = 1;
				accelerationStructBuildInfo.ppGeometries = &ppGeometries;
				accelerationStructBuildInfo.geometryArrayOfPointers = VK_FALSE;
				accelerationStructBuildInfo.scratchData = device->GetBufferDeviceOrHostAddress(scratchBuffer.buffer);
			device->CmdBuildAccelerationStructureKHR(cmdBuffer, 1, &accelerationStructBuildInfo, &pGeometriesOffsets);
			
			// VkMemoryBarrier memoryBarrier {
			// 	VK_STRUCTURE_TYPE_MEMORY_BARRIER,
			// 	nullptr,// pNext
			// 	VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR | VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR,// VkAccessFlags srcAccessMask
			// 	VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR | VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR,// VkAccessFlags dstAccessMask
			// };
			// device->CmdPipelineBarrier(
			// 	cmdBuffer, 
			// 	VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, 
			// 	VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, 
			// 	0, 
			// 	1, &memoryBarrier, 
			// 	0, 0, 
			// 	0, 0
			// );
		
			built = true;
		}
		
		void Create(Device* device) {
			uint32_t flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
			if (allowUpdate) flags |= VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR;
			
			rayTracingGeometriesCreate.resize(1);
			for (auto& c : rayTracingGeometriesCreate) {
				c.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_GEOMETRY_TYPE_INFO_KHR;
				c.pNext = nullptr;
				c.maxPrimitiveCount = maxInstances;
				c.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
				// c.maxVertexCount = 0;
				// c.indexType = VK_INDEX_TYPE_NONE_KHR;
				// c.vertexFormat = VK_FORMAT_UNDEFINED;
				// c.allowsTransforms = VK_TRUE;
			}
			const auto* rayTracingGeometriesCreates = rayTracingGeometriesCreate.data();
			
			createInfo = {
				VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
				nullptr,// pNext
				0,// VkDeviceSize compactedSize
				VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
				flags,
				1, // maxGeometryCount
				rayTracingGeometriesCreates, // pGeometryInfos
				0 // VkDeviceAddress deviceAddress
			};
			
			if (device->CreateAccelerationStructureKHR(&createInfo, nullptr, &accelerationStructure) != VK_SUCCESS)
				throw std::runtime_error("Failed to create top level acceleration structure");
				
				
			// Allocate and bind memory for top level acceleration structure
			VkMemoryRequirements2 memoryRequirementsAS {};
			{
				VkAccelerationStructureMemoryRequirementsInfoKHR memoryRequirementsInfo {};
				memoryRequirementsInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_INFO_KHR;
				memoryRequirementsInfo.type = VK_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_TYPE_OBJECT_KHR;
				memoryRequirementsInfo.buildType = VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR;
				memoryRequirementsInfo.accelerationStructure = accelerationStructure;
				device->GetAccelerationStructureMemoryRequirementsKHR(&memoryRequirementsInfo, &memoryRequirementsAS);
			}
			VkMemoryAllocateInfo memoryAllocateInfo {
				VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
				nullptr,// pNext
				memoryRequirementsAS.memoryRequirements.size,// VkDeviceSize allocationSize
				device->GetPhysicalDevice()->FindMemoryType(memoryRequirementsAS.memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)// memoryTypeIndex
			};
			if (device->AllocateMemory(&memoryAllocateInfo, nullptr, &memory) != VK_SUCCESS)
				throw std::runtime_error("Failed to allocate memory for top level acceleration structure");
			VkBindAccelerationStructureMemoryInfoKHR accelerationStructureMemoryInfo {
				VK_STRUCTURE_TYPE_BIND_ACCELERATION_STRUCTURE_MEMORY_INFO_KHR,
				nullptr,// pNext
				accelerationStructure,// accelerationStructure
				memory,// memory
				0,// VkDeviceSize memoryOffset
				0,// uint32_t deviceIndexCount
				nullptr// pDeviceIndices
			};
			if (device->BindAccelerationStructureMemoryKHR(1, &accelerationStructureMemoryInfo) != VK_SUCCESS)
				throw std::runtime_error("Failed to bind top level acceleration structure memory");
				
			// Get top level acceleration structure handle for use in top level instances
			VkAccelerationStructureDeviceAddressInfoKHR devAddrInfo {};
			devAddrInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
			devAddrInfo.accelerationStructure = accelerationStructure;
			handle = device->GetAccelerationStructureDeviceAddressKHR(&devAddrInfo);
		}
		
		void Destroy(Device* device) {
			// Acceleration Structures
			if (accelerationStructure != VK_NULL_HANDLE) {
				if (memory != VK_NULL_HANDLE) device->FreeMemory(memory, nullptr);
				device->DestroyAccelerationStructureKHR(accelerationStructure, nullptr);
				accelerationStructure = VK_NULL_HANDLE;
				built = false;
			}
			if (scratchBufferAllocated) {
				scratchBuffer.Free(device);
				scratchBufferAllocated = false;
			}
			if (instanceBufferAllocated) {
				instanceBuffer.Free(device);
				instanceBufferAllocated = false;
			}
		}
	};

	#pragma endregion

	#pragma region Shaders
	
	PipelineLayout 
		rayTracingPipelineLayout,
		thumbnailPipelineLayout, 
		postProcessingPipelineLayout, 
		uiPipelineLayout, 
		histogramComputeLayout;
		
	ShaderBindingTable shaderBindingTable {rayTracingPipelineLayout, "incubator_rendering/assets/shaders/rtx.rgen"};
	RasterShaderPipeline thumbnailShader {thumbnailPipelineLayout, {
		"incubator_rendering/assets/shaders/v4d_thumbnail.vert",
		"incubator_rendering/assets/shaders/v4d_thumbnail.frag",
	}};
	RasterShaderPipeline postProcessingShader_txaa {postProcessingPipelineLayout, {
		"incubator_rendering/assets/shaders/v4d_postProcessing.vert",
		"incubator_rendering/assets/shaders/v4d_postProcessing.txaa.frag",
	}};
	RasterShaderPipeline postProcessingShader_history {postProcessingPipelineLayout, {
		"incubator_rendering/assets/shaders/v4d_postProcessing.vert",
		"incubator_rendering/assets/shaders/v4d_postProcessing.history.frag",
	}};
	RasterShaderPipeline postProcessingShader_hdr {postProcessingPipelineLayout, {
		"incubator_rendering/assets/shaders/v4d_postProcessing.vert",
		"incubator_rendering/assets/shaders/v4d_postProcessing.hdr.frag",
	}};
	RasterShaderPipeline postProcessingShader_ui {postProcessingPipelineLayout, {
		"incubator_rendering/assets/shaders/v4d_postProcessing.vert",
		"incubator_rendering/assets/shaders/v4d_postProcessing.ui.frag",
	}};
	ComputeShaderPipeline histogramComputeShader {histogramComputeLayout, 
		"incubator_rendering/assets/shaders/v4d_histogram.comp"
	};
	
	std::unordered_map<std::string, std::vector<RasterShaderPipeline*>> shaders {
		/* RenderPass_SubPass => ShadersList */
		{"thumbnail", {&thumbnailShader}},
		{"postProcessing_0", {&postProcessingShader_txaa}},
		{"postProcessing_1", {&postProcessingShader_history}},
		{"postProcessing_2", {&postProcessingShader_hdr, &postProcessingShader_ui}},
		{"ui", {}},
	};
	
	#pragma endregion
	
protected:
	Scene scene {};
	RenderTargetGroup renderTargetGroup {};
	std::unordered_map<std::string, Image*> images {
		{"depthImage", &renderTargetGroup.GetDepthImage()},
		// {"gBuffer_albedo", &renderTargetGroup.GetGBuffer(0)},
		// {"gBuffer_normal", &renderTargetGroup.GetGBuffer(1)},
		// {"gBuffer_emission", &renderTargetGroup.GetGBuffer(2)},
		// {"gBuffer_position", &renderTargetGroup.GetGBuffer(3)},
		{"litImage", &renderTargetGroup.GetLitImage()},
		{"thumbnail", &thumbnailImage},
		{"historyImage", &renderTargetGroup.GetHistoryImage()},
		{"ui", &uiImage},
	};
	
private: // Ray tracing stuff
	// std::vector<Geometry*> geometries {};
	std::vector<BottomLevelAccelerationStructure> rayTracingBottomLevelAccelerationStructures {};
	TopLevelAccelerationStructure rayTracingTopLevelAccelerationStructure {};
	Buffer rayTracingShaderBindingTableBuffer {VK_BUFFER_USAGE_RAY_TRACING_BIT_KHR};
	VkPhysicalDeviceRayTracingPropertiesKHR rayTracingProperties{};
	DescriptorSet* rayTracingDescriptorSet_1 = nullptr;
	
	void CreateRayTracingAccelerationStructures() {
		rayTracingTopLevelAccelerationStructure.Create(renderingDevice);
	}
	
	void DestroyRayTracingAccelerationStructures() {
		rayTracingTopLevelAccelerationStructure.Destroy(renderingDevice);
	}
	
	void CreateRayTracingPipeline() {
		rayTracingPipelineLayout.Create(renderingDevice);
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
		rayTracingPipelineLayout.Destroy(renderingDevice);
	}
	
private: // Init
	void Init() override {
		// Ray Tracing
		RequiredDeviceExtension(VK_KHR_RAY_TRACING_EXTENSION_NAME); // NVidia's RayTracing extension
		RequiredDeviceExtension(VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME); // Needed for RayTracing extension
		
		cameraUniformBuffer.AddSrcDataPtr(&scene.camera, sizeof(Camera));
		activeLightsUniformBuffer.AddSrcDataPtr(&nbActiveLights, sizeof(uint32_t));
		activeLightsUniformBuffer.AddSrcDataPtr(&activeLights, sizeof(uint32_t)*MAX_ACTIVE_LIGHTS);
		
		// Submodules
		renderingSubmodules = v4d::modules::GetSubmodules<v4d::modules::Rendering>();
		std::sort(renderingSubmodules.begin(), renderingSubmodules.end(), [](auto* a, auto* b){return a->OrderIndex() < b->OrderIndex();});
		for (auto* submodule : renderingSubmodules) {
			submodule->SetRenderer(this);
			submodule->Init();
		}
	}
	void ScorePhysicalDeviceSelection(int& score, PhysicalDevice* physicalDevice) override {
		// Submodules
		for (auto* submodule : renderingSubmodules) {
			submodule->ScorePhysicalDeviceSelection(score, physicalDevice);
		}
	}
	void Info() override {
		// Query the ray tracing properties of the current implementation
		rayTracingProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PROPERTIES_KHR;
		VkPhysicalDeviceProperties2 deviceProps2{};
		deviceProps2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
		deviceProps2.pNext = &rayTracingProperties;
		GetPhysicalDeviceProperties2(renderingDevice->GetPhysicalDevice()->GetHandle(), &deviceProps2);
		
		// Submodules
		for (auto* submodule : renderingSubmodules) {
			submodule->SetRenderingDevice(renderingDevice);
			submodule->SetGraphicsQueue(&graphicsQueue);
			submodule->SetLowPriorityGraphicsQueue(&lowPriorityGraphicsQueue);
			submodule->SetLowPriorityComputeQueue(&lowPriorityComputeQueue);
			submodule->SetTransferQueue(&transferQueue);
			submodule->Info();
		}
	}

	void InitLayouts() override {
		
		// Base descriptor set containing CameraUBO and such, almost all shaders should use it
		auto* baseDescriptorSet_0 = descriptorSets.emplace_back(new DescriptorSet(0));
		baseDescriptorSet_0->AddBinding_uniformBuffer(0, &cameraUniformBuffer.deviceLocalBuffer, VK_SHADER_STAGE_ALL_GRAPHICS | VK_SHADER_STAGE_INTERSECTION_BIT_KHR | VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR);
		baseDescriptorSet_0->AddBinding_storageBuffer(1, &Geometry::globalBuffers.objectBuffer.deviceLocalBuffer, VK_SHADER_STAGE_INTERSECTION_BIT_KHR | VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR);
		baseDescriptorSet_0->AddBinding_storageBuffer(2, &Geometry::globalBuffers.lightBuffer.deviceLocalBuffer, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR);
		baseDescriptorSet_0->AddBinding_storageBuffer(3, &activeLightsUniformBuffer.deviceLocalBuffer, VK_SHADER_STAGE_ALL_GRAPHICS | VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR);
		
		rayTracingDescriptorSet_1 = descriptorSets.emplace_back(new DescriptorSet(1));
		rayTracingDescriptorSet_1->AddBinding_accelerationStructure(0, &rayTracingTopLevelAccelerationStructure.accelerationStructure, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR);
		rayTracingDescriptorSet_1->AddBinding_imageView(1, &renderTargetGroup.GetLitImage(), VK_SHADER_STAGE_RAYGEN_BIT_KHR);
		rayTracingDescriptorSet_1->AddBinding_imageView(2, &renderTargetGroup.GetDepthImage(), VK_SHADER_STAGE_RAYGEN_BIT_KHR);
		rayTracingDescriptorSet_1->AddBinding_storageBuffer(3, &Geometry::globalBuffers.geometryBuffer.deviceLocalBuffer, VK_SHADER_STAGE_INTERSECTION_BIT_KHR | VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR | VK_SHADER_STAGE_COMPUTE_BIT);
		rayTracingDescriptorSet_1->AddBinding_storageBuffer(4, &Geometry::globalBuffers.indexBuffer.deviceLocalBuffer, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR | VK_SHADER_STAGE_COMPUTE_BIT);
		rayTracingDescriptorSet_1->AddBinding_storageBuffer(5, &Geometry::globalBuffers.vertexBuffer.deviceLocalBuffer, VK_SHADER_STAGE_INTERSECTION_BIT_KHR | VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR | VK_SHADER_STAGE_COMPUTE_BIT);
		rayTracingPipelineLayout.AddDescriptorSet(baseDescriptorSet_0);
		rayTracingPipelineLayout.AddDescriptorSet(rayTracingDescriptorSet_1);
		
		// Thumbnail Gen
		auto* thumbnailDescriptorSet_1 = descriptorSets.emplace_back(new DescriptorSet(1));
		thumbnailDescriptorSet_1->AddBinding_combinedImageSampler(0, &renderTargetGroup.GetLitImage(), VK_SHADER_STAGE_FRAGMENT_BIT);
		thumbnailPipelineLayout.AddDescriptorSet(baseDescriptorSet_0);
		thumbnailPipelineLayout.AddDescriptorSet(thumbnailDescriptorSet_1);
		
		// Post-Processing
		auto* postProcessingDescriptorSet_1 = descriptorSets.emplace_back(new DescriptorSet(1));
		postProcessingDescriptorSet_1->AddBinding_combinedImageSampler(0, &renderTargetGroup.GetLitImage(), VK_SHADER_STAGE_FRAGMENT_BIT);
		postProcessingDescriptorSet_1->AddBinding_combinedImageSampler(1, &uiImage, VK_SHADER_STAGE_FRAGMENT_BIT);
		postProcessingDescriptorSet_1->AddBinding_inputAttachment(2, &renderTargetGroup.GetPpImage(), VK_SHADER_STAGE_FRAGMENT_BIT);
		postProcessingDescriptorSet_1->AddBinding_combinedImageSampler(3, &renderTargetGroup.GetHistoryImage(), VK_SHADER_STAGE_FRAGMENT_BIT);
		postProcessingDescriptorSet_1->AddBinding_combinedImageSampler(4, &renderTargetGroup.GetDepthImage(), VK_SHADER_STAGE_FRAGMENT_BIT);
		postProcessingPipelineLayout.AddDescriptorSet(baseDescriptorSet_0);
		postProcessingPipelineLayout.AddDescriptorSet(postProcessingDescriptorSet_1);
		
		// UI
		//TODO uiPipelineLayout
		
		// Compute
		auto* histogramComputeDescriptorSet_0 = descriptorSets.emplace_back(new DescriptorSet(0));
		histogramComputeDescriptorSet_0->AddBinding_imageView(0, &thumbnailImage, VK_SHADER_STAGE_COMPUTE_BIT);
		histogramComputeDescriptorSet_0->AddBinding_storageBuffer(1, &totalLuminance, VK_SHADER_STAGE_COMPUTE_BIT);
		histogramComputeLayout.AddDescriptorSet(histogramComputeDescriptorSet_0);
		
		// Submodules
		for (auto* submodule : renderingSubmodules) {
			submodule->InitLayouts(descriptorSets, images, &rayTracingPipelineLayout);
		}
	}
	
	void ConfigureShaders() override {
		shaderBindingTable.AddMissShader("incubator_rendering/assets/shaders/rtx.rmiss");
		shaderBindingTable.AddMissShader("incubator_rendering/assets/shaders/rtx.shadow.rmiss");
		// Base ray tracing shaders
		Geometry::rayTracingShaderOffsets["standard"] = shaderBindingTable.AddHitShader("incubator_rendering/assets/shaders/rtx.rchit" /*, "incubator_rendering/assets/shaders/rtx.rahit"*/ );
		Geometry::rayTracingShaderOffsets["sphere"] = shaderBindingTable.AddHitShader("incubator_rendering/assets/shaders/rtx.sphere.rchit", "", "incubator_rendering/assets/shaders/rtx.sphere.rint");
		Geometry::rayTracingShaderOffsets["light"] = shaderBindingTable.AddHitShader("incubator_rendering/assets/shaders/rtx.light.rchit", "", "incubator_rendering/assets/shaders/rtx.sphere.rint");
		
		// Thumbnail Gen
		thumbnailShader.inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
		thumbnailShader.depthStencilState.depthTestEnable = VK_FALSE;
		thumbnailShader.depthStencilState.depthWriteEnable = VK_FALSE;
		thumbnailShader.rasterizer.cullMode = VK_CULL_MODE_NONE;
		thumbnailShader.SetData(3);
		
		// Post-Processing
		for (auto[rp, ss] : shaders) if (rp.substr(0, 14) == "postProcessing") {
			for (auto* s : ss) {
				s->inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
				s->depthStencilState.depthTestEnable = VK_FALSE;
				s->depthStencilState.depthWriteEnable = VK_FALSE;
				s->rasterizer.cullMode = VK_CULL_MODE_NONE;
				s->SetData(3);
			}
		}
		
		// UI
		//TODO uiShaders
		
		// Submodules
		for (auto* submodule : renderingSubmodules) {
			submodule->ConfigureShaders(shaders, &shaderBindingTable);
		}
	}
	
private: // Resources

	void CreateResources() override {
		float screenWidth = (float)swapChain->extent.width;
		float screenHeight = (float)swapChain->extent.height;
		uint uiWidth = (uint)(screenWidth * uiImageScale);
		uint uiHeight = (uint)(screenHeight * uiImageScale);
		uint thumbnailWidth = (uint)((float)swapChain->extent.width * thumbnailScale);
		uint thumbnailHeight = (uint)((float)swapChain->extent.height * thumbnailScale);
		
		// Create images
		uiImage.Create(renderingDevice, uiWidth, uiHeight);
		thumbnailImage.Create(renderingDevice, thumbnailWidth, thumbnailHeight, {renderTargetGroup.GetLitImage().format});
		renderTargetGroup.SetRenderTarget(swapChain);
		renderTargetGroup.GetLitImage().usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		renderTargetGroup.CreateResources(renderingDevice);
		
		// Transition images
		TransitionImageLayout(renderTargetGroup.GetLitImage(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
		TransitionImageLayout(uiImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
		TransitionImageLayout(thumbnailImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
		TransitionImageLayout(renderTargetGroup.GetHistoryImage(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
		TransitionImageLayout(renderTargetGroup.GetDepthImage(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
		
		// Submodules
		for (auto* submodule : renderingSubmodules) {
			submodule->SetSwapChain(swapChain);
			submodule->CreateResources();
		}
		
		CreateRayTracingAccelerationStructures();
	}
	
	void DestroyResources() override {
		// Destroy images
		uiImage.Destroy(renderingDevice);
		thumbnailImage.Destroy(renderingDevice);
		renderTargetGroup.DestroyResources(renderingDevice);
		
		// Submodules
		for (auto* submodule : renderingSubmodules) {
			submodule->DestroyResources();
		}
		
		DestroyRayTracingAccelerationStructures();
	}
	
	void AllocateBuffers() override {
		// Uniform Buffers
		cameraUniformBuffer.Allocate(renderingDevice);
		activeLightsUniformBuffer.Allocate(renderingDevice);
		
		// Compute histogram
		totalLuminance.Allocate(renderingDevice, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, false);
		totalLuminance.MapMemory(renderingDevice);
		glm::vec4 luminance {0,0,0,1};
		totalLuminance.WriteToMappedData(&luminance);
		
		// Submodules
		for (auto* submodule : renderingSubmodules) {
			submodule->AllocateBuffers();
		}
		
		Geometry::globalBuffers.Allocate(renderingDevice, {lowPriorityComputeQueue.familyIndex, graphicsQueue.familyIndex});
	}
	
	void FreeBuffers() override {
	
		// Scene Objects
		scene.Lock();
		for (auto* obj : scene.objectInstances) if (obj) {
			obj->ClearGeometries();
		}
		scene.Unlock();
		
		// Ray Tracing
		rayTracingTopLevelAccelerationStructure.instances.clear();
		for (auto blas : rayTracingBottomLevelAccelerationStructures) {
			blas.Destroy(renderingDevice);
		}
		rayTracingBottomLevelAccelerationStructures.clear();

		Geometry::globalBuffers.Free(renderingDevice);
		
		// Uniform Buffers
		cameraUniformBuffer.Free(renderingDevice);
		activeLightsUniformBuffer.Free(renderingDevice);
		
		// Compute histogram
		totalLuminance.UnmapMemory(renderingDevice);
		totalLuminance.Free(renderingDevice);
		
		// Submodules
		for (auto* submodule : renderingSubmodules) {
			submodule->FreeBuffers();
		}
	}

private: // Pipelines

	void CreatePipelines() override {
		
		// Pipeline layouts
		thumbnailPipelineLayout.Create(renderingDevice);
		postProcessingPipelineLayout.Create(renderingDevice);
		uiPipelineLayout.Create(renderingDevice);
		histogramComputeLayout.Create(renderingDevice);
		
		// Submodules
		for (auto* submodule : renderingSubmodules) {
			submodule->CreatePipelines(images);
		}
		
		{// Thumbnail Gen render pass
			VkAttachmentDescription colorAttachment = {};
				colorAttachment.format = thumbnailImage.format;
				colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
				colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
				colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
				colorAttachment.initialLayout = VK_IMAGE_LAYOUT_GENERAL;
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
			thumbnailRenderPass.CreateFrameBuffers(renderingDevice, thumbnailImage);
			
			// Shaders
			for (auto* shaderPipeline : shaders["thumbnail"]) {
				shaderPipeline->SetRenderPass(&thumbnailImage, thumbnailRenderPass.handle, 0);
				shaderPipeline->AddColorBlendAttachmentState();
				shaderPipeline->CreatePipeline(renderingDevice);
			}
		}
		
		{// Post Processing render pass
			std::array<VkAttachmentDescription, 3> attachments {};
			// std::array<VkAttachmentReference, 0> inputAttachmentRefs {};
			
			// PP image
			attachments[0].format = renderTargetGroup.GetPpImage().format;
			attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
			attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			attachments[0].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			uint32_t ppAttachmentIndex = postProcessingRenderPass.AddAttachment(attachments[0]);
			
			// History image
			attachments[1].format = renderTargetGroup.GetHistoryImage().format;
			attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
			attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			attachments[1].initialLayout = VK_IMAGE_LAYOUT_GENERAL;
			attachments[1].finalLayout = VK_IMAGE_LAYOUT_GENERAL;
			uint32_t historyAttachmentIndex = postProcessingRenderPass.AddAttachment(attachments[1]);
			
			// SwapChain image
			attachments[2].format = swapChain->format.format;
			attachments[2].samples = VK_SAMPLE_COUNT_1_BIT;
			attachments[2].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			attachments[2].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			attachments[2].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			attachments[2].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
			uint32_t presentAttachmentIndex = postProcessingRenderPass.AddAttachment(attachments[2]);
			
			// SubPasses
			VkAttachmentReference colorAttRef_0 { ppAttachmentIndex, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
			{
				VkSubpassDescription subpass {};
				subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
				subpass.colorAttachmentCount = 1;
				subpass.pColorAttachments = &colorAttRef_0;
				// subpass.inputAttachmentCount = inputAttachmentRefs.size();
				// subpass.pInputAttachments = inputAttachmentRefs.data();
				postProcessingRenderPass.AddSubpass(subpass);
			}
			VkAttachmentReference colorAttRef_1 { historyAttachmentIndex, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
			VkAttachmentReference inputAtt_1 { ppAttachmentIndex, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
			{
				VkSubpassDescription subpass {};
				subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
				subpass.colorAttachmentCount = 1;
				subpass.pColorAttachments = &colorAttRef_1;
				subpass.inputAttachmentCount = 1;
				subpass.pInputAttachments = &inputAtt_1;
				postProcessingRenderPass.AddSubpass(subpass);
			}
			VkAttachmentReference colorAttRef_2 { presentAttachmentIndex, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
			VkAttachmentReference inputAtt_2 { ppAttachmentIndex, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
			uint32_t preserverAtt_2 = historyAttachmentIndex;
			{
				VkSubpassDescription subpass {};
				subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
				subpass.colorAttachmentCount = 1;
				subpass.pColorAttachments = &colorAttRef_2;
				subpass.preserveAttachmentCount = 1;
				subpass.pPreserveAttachments = &preserverAtt_2;
				subpass.inputAttachmentCount = 1;
				subpass.pInputAttachments = &inputAtt_2;
				postProcessingRenderPass.AddSubpass(subpass);
			}
			
			std::array<VkSubpassDependency, 3> subPassDependencies {
				VkSubpassDependency{
					VK_SUBPASS_EXTERNAL,// srcSubpass;
					0,// dstSubpass;
					VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,// srcStageMask;
					VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,// dstStageMask;
					VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,// srcAccessMask;
					VK_ACCESS_COLOR_ATTACHMENT_READ_BIT|VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,// dstAccessMask;
					0// dependencyFlags;
				},
				VkSubpassDependency{
					0,// srcSubpass;
					1,// dstSubpass;
					VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,// srcStageMask;
					VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,// dstStageMask;
					VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,// srcAccessMask;
					VK_ACCESS_COLOR_ATTACHMENT_READ_BIT|VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,// dstAccessMask;
					0// dependencyFlags;
				},
				VkSubpassDependency{
					0,// srcSubpass;
					2,// dstSubpass;
					VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,// srcStageMask;
					VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,// dstStageMask;
					VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,// srcAccessMask;
					VK_ACCESS_COLOR_ATTACHMENT_READ_BIT|VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,// dstAccessMask;
					0// dependencyFlags;
				}
			};
			postProcessingRenderPass.renderPassInfo.dependencyCount = subPassDependencies.size();
			postProcessingRenderPass.renderPassInfo.pDependencies = subPassDependencies.data();
			
			// Create the render pass
			postProcessingRenderPass.Create(renderingDevice);
			postProcessingRenderPass.CreateFrameBuffers(renderingDevice, swapChain, {
				renderTargetGroup.GetPpImage().view, 
				renderTargetGroup.GetHistoryImage().view, 
				VK_NULL_HANDLE
			});
			
			// Shaders
			for (auto* shaderPipeline : shaders["postProcessing_0"]) {
				shaderPipeline->SetRenderPass(swapChain, postProcessingRenderPass.handle, 0);
				shaderPipeline->AddColorBlendAttachmentState();
				shaderPipeline->CreatePipeline(renderingDevice);
			}
			for (auto* shaderPipeline : shaders["postProcessing_1"]) {
				shaderPipeline->SetRenderPass(swapChain, postProcessingRenderPass.handle, 1);
				shaderPipeline->AddColorBlendAttachmentState(VK_FALSE);
				shaderPipeline->CreatePipeline(renderingDevice);
			}
			for (auto* shaderPipeline : shaders["postProcessing_2"]) {
				shaderPipeline->SetRenderPass(swapChain, postProcessingRenderPass.handle, 2);
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
			uiRenderPass.AddSubpass(subpass);
			
			// Create the render pass
			uiRenderPass.Create(renderingDevice);
			uiRenderPass.CreateFrameBuffers(renderingDevice, uiImage);
			
			// Shader pipeline
			for (auto* shader : shaders["ui"]) {
				shader->SetRenderPass(&uiImage, uiRenderPass.handle, 0);
				shader->AddColorBlendAttachmentState();
				shader->CreatePipeline(renderingDevice);
			}
		}
		
		#ifdef _ENABLE_IMGUI
			{// ImGui
				ImGui_ImplVulkan_InitInfo init_info {};
				init_info.Instance = GetHandle();
				init_info.PhysicalDevice = renderingDevice->GetPhysicalDevice()->GetHandle();
				init_info.Device = renderingDevice->GetHandle();
				init_info.QueueFamily = graphicsQueue.familyIndex;
				init_info.Queue = graphicsQueue.handle;
				init_info.DescriptorPool = descriptorPool;
				init_info.MinImageCount = swapChain->images.size();
				init_info.ImageCount = swapChain->images.size();
				ImGui_ImplVulkan_Init(&init_info, uiRenderPass.handle);
				// Font Upload
				auto cmdBuffer = BeginSingleTimeCommands(graphicsQueue);
				ImGui_ImplVulkan_CreateFontsTexture(cmdBuffer);
				EndSingleTimeCommands(graphicsQueue, cmdBuffer);
				ImGui_ImplVulkan_DestroyFontUploadObjects();
			}
		#endif
		
		// Compute
		histogramComputeShader.CreatePipeline(renderingDevice);
		
		CreateRayTracingPipeline();
	}
	
	void DestroyPipelines() override {
		DestroyRayTracingPipeline();
		
		#ifdef _ENABLE_IMGUI
			// ImGui
			ImGui_ImplVulkan_Shutdown();
		#endif
		
		// Thumbnail Gen
		for (auto* shaderPipeline : shaders["thumbnail"]) {
			shaderPipeline->DestroyPipeline(renderingDevice);
		}
		thumbnailRenderPass.DestroyFrameBuffers(renderingDevice);
		thumbnailRenderPass.Destroy(renderingDevice);
		
		// Post-processing
		for (auto[rp, ss] : shaders) if (rp.substr(0, 14) == "postProcessing") {
			for (auto* s : ss) {
				s->DestroyPipeline(renderingDevice);
			}
		}
		postProcessingRenderPass.DestroyFrameBuffers(renderingDevice);
		postProcessingRenderPass.Destroy(renderingDevice);
		
		// Compute
		histogramComputeShader.DestroyPipeline(renderingDevice);
		
		// UI
		for (auto* shaderPipeline : shaders["ui"]) {
			shaderPipeline->DestroyPipeline(renderingDevice);
		}
		uiRenderPass.DestroyFrameBuffers(renderingDevice);
		uiRenderPass.Destroy(renderingDevice);
		
		////////////////////////////
		// Pipeline layouts
		thumbnailPipelineLayout.Destroy(renderingDevice);
		postProcessingPipelineLayout.Destroy(renderingDevice);
		uiPipelineLayout.Destroy(renderingDevice);
		histogramComputeLayout.Destroy(renderingDevice);
		
		// Submodules
		for (auto* submodule : renderingSubmodules) {
			submodule->DestroyPipelines();
		}
	}
	
private: // Commands

	void RecordGraphicsCommandBuffer(VkCommandBuffer commandBuffer, int imageIndex) override {
		
		// Ray Tracing
		renderingDevice->CmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, shaderBindingTable.GetPipeline());
		shaderBindingTable.GetPipelineLayout()->Bind(renderingDevice, commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR);
		VkDeviceSize bindingStride = rayTracingProperties.shaderGroupHandleSize;
		VkDeviceSize bindingOffsetMissShader = bindingStride * shaderBindingTable.GetMissGroupOffset();
		VkDeviceSize bindingOffsetHitShader = bindingStride * shaderBindingTable.GetHitGroupOffset();
		int width = (int)((float)swapChain->extent.width);
		int height = (int)((float)swapChain->extent.height);
		VkStridedBufferRegionKHR rayGen {rayTracingShaderBindingTableBuffer.buffer, 0, bindingStride, rayTracingShaderBindingTableBuffer.size};
		VkStridedBufferRegionKHR rayMiss {rayTracingShaderBindingTableBuffer.buffer, bindingOffsetMissShader, bindingStride, rayTracingShaderBindingTableBuffer.size};
		VkStridedBufferRegionKHR rayHit {rayTracingShaderBindingTableBuffer.buffer, bindingOffsetHitShader, bindingStride, rayTracingShaderBindingTableBuffer.size};
		VkStridedBufferRegionKHR callable {VK_NULL_HANDLE, 0, bindingStride, rayTracingShaderBindingTableBuffer.size};
		renderingDevice->CmdTraceRaysKHR(
			commandBuffer, 
			&rayGen,
			&rayMiss,
			&rayHit,
			&callable,
			width, height, 1
		);
		
		// Gen Thumbnail
		thumbnailRenderPass.Begin(renderingDevice, commandBuffer, thumbnailImage, {{.0,.0,.0,.0}});
			for (auto* shaderPipeline : shaders["thumbnail"]) {
				shaderPipeline->Execute(renderingDevice, commandBuffer);
			}
		thumbnailRenderPass.End(renderingDevice, commandBuffer);
		
		// Post Processing
		postProcessingRenderPass.Begin(renderingDevice, commandBuffer, swapChain, {{.0,.0,.0,.0}, {.0,.0,.0,.0}, {.0,.0,.0,.0}}, imageIndex);
			for (auto* shaderPipeline : shaders["postProcessing_0"]) {
				shaderPipeline->Execute(renderingDevice, commandBuffer);
			}
			renderingDevice->CmdNextSubpass(commandBuffer, VK_SUBPASS_CONTENTS_INLINE);
			for (auto* shaderPipeline : shaders["postProcessing_1"]) {
				shaderPipeline->Execute(renderingDevice, commandBuffer);
			}
			renderingDevice->CmdNextSubpass(commandBuffer, VK_SUBPASS_CONTENTS_INLINE);
			for (auto* shaderPipeline : shaders["postProcessing_2"]) {
				shaderPipeline->Execute(renderingDevice, commandBuffer);
			}
		postProcessingRenderPass.End(renderingDevice, commandBuffer);
	}
	
	void RunDynamicGraphics(VkCommandBuffer commandBuffer) override {
		
		// VkMemoryBarrier memoryBarrier {
		// 	VK_STRUCTURE_TYPE_MEMORY_BARRIER,
		// 	nullptr,// pNext
		// 	VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR | VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR,// VkAccessFlags srcAccessMask
		// 	VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR | VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR,// VkAccessFlags dstAccessMask
		// };
		// renderingDevice->CmdPipelineBarrier(
		// 	commandBuffer, 
		// 	VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, 
		// 	VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, 
		// 	0, 
		// 	1, &memoryBarrier, 
		// 	0, 0, 
		// 	0, 0
		// );
		
		rayTracingTopLevelAccelerationStructure.Build(renderingDevice, commandBuffer);
		// if (rayTracingTopLevelAccelerationStructure.handleDirty) {
		// 	UpdateDescriptorSet(rayTracingDescriptorSet_1, {0}); //TODO fix this...   "You are adding vkQueueSubmit() to VkCommandBuffer *** that is invalid because bound VkDescriptorSet *** was destroyed or updated."
		// }
		
		// Submodules
		for (auto* submodule : renderingSubmodules) {
			submodule->RunDynamicGraphicsTop(commandBuffer, images);
		}
		
		// Submodules
		for (auto* submodule : renderingSubmodules) {
			submodule->RunDynamicGraphicsMiddle(commandBuffer, images);
		}
		
		// Submodules
		for (auto* submodule : renderingSubmodules) {
			submodule->RunDynamicGraphicsBottom(commandBuffer, images);
		}
		
	}
	
	void RunDynamicLowPriorityCompute(VkCommandBuffer commandBuffer) override {
		// Compute
		histogramComputeShader.SetGroupCounts(1, 1, 1);
		histogramComputeShader.Execute(renderingDevice, commandBuffer);
		
		// Submodules
		for (auto* submodule : renderingSubmodules) {
			submodule->RunDynamicLowPriorityCompute(commandBuffer);
		}
		
	}
	
	void RunDynamicLowPriorityGraphics(VkCommandBuffer commandBuffer) override {
		
		// UI
		uiRenderPass.Begin(renderingDevice, commandBuffer, uiImage, {{.0,.0,.0,.0}});
			for (auto* shaderPipeline : shaders["ui"]) {
				shaderPipeline->Execute(renderingDevice, commandBuffer);
			}
			#ifdef _ENABLE_IMGUI
				ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer);
			#endif
		uiRenderPass.End(renderingDevice, commandBuffer);
		
		// Submodules
		for (auto* submodule : renderingSubmodules) {
			submodule->RunDynamicLowPriorityGraphics(commandBuffer);
		}
		
	}
	
public: // Scene configuration
	
	void ReadShaders() override {
		// All shaders
		for (auto&[t, shaderList] : shaders) {
			for (auto* shader : shaderList) {
				shader->ReadShaders();
			}
		}
		
		// Ray Tracing
		shaderBindingTable.ReadShaders();
		
		// Compute
		histogramComputeShader.ReadShaders();
		
		// Submodules
		for (auto* submodule : renderingSubmodules) {
			submodule->ReadShaders();
		}
	}
	
	void LoadScene() override {
		// Submodules
		for (auto* submodule : renderingSubmodules) {
			submodule->LoadScene(scene);
		}
		
		auto objCount = scene.GetObjectCount();
		rayTracingBottomLevelAccelerationStructures.reserve(objCount);
		rayTracingTopLevelAccelerationStructure.instances.reserve(objCount);
	}
	
	void UnloadScene() override {
		// Submodules
		for (auto* submodule : renderingSubmodules) {
			submodule->UnloadScene(scene);
		}
	}
	
public: // Update

	void FrameUpdate(uint imageIndex) override {
		
		// Reset camera information
		scene.camera.width = swapChain->extent.width;
		scene.camera.height = swapChain->extent.height;
		scene.camera.RefreshProjectionMatrix();
		
		// Submodules
		for (auto* submodule : renderingSubmodules) {
			submodule->FrameUpdate(scene);
		}
		
		scene.Lock();
		
		// Update object transforms and light sources (Use all lights for now)
		nbActiveLights = 0;
		for (auto* obj : scene.objectInstances) if (obj && obj->IsActive()) {
			obj->WriteMatrices(scene.camera.viewMatrix);
			for (auto* lightSource : obj->GetLightSources()) {
				activeLights[nbActiveLights++] = lightSource->lightOffset;
			}
		}
		
		// Transfer data to rendering device
		auto cmdBuffer = BeginSingleTimeCommands(graphicsQueue);
			cameraUniformBuffer.Update(renderingDevice, cmdBuffer);
			activeLightsUniformBuffer.Update(renderingDevice, cmdBuffer, sizeof(uint32_t)/*lightCount*/ + sizeof(uint32_t)*nbActiveLights/*lightIndices*/);
			Geometry::globalBuffers.PushObjects(renderingDevice, cmdBuffer);
			Geometry::globalBuffers.PushLights(renderingDevice, cmdBuffer);
		EndSingleTimeCommands(graphicsQueue, cmdBuffer);
		
		// Ray Tracing Bottom Level Acceleration Structures
		// Add/Remove/Update geometries
		for (auto* obj : scene.objectInstances) if (obj) {
			// Geometry
			if (obj->IsActive() && obj->HasActiveGeometries()) {
				// If object is active, has a BLAS ready but not yet in TLAS
				if (obj->GetRayTracingBlasIndex() != -1 && obj->GetRayTracingInstanceIndex() == -1) {
					// Add Instance
					obj->SetRayTracingInstanceIndex(rayTracingTopLevelAccelerationStructure.instances.size());
					rayTracingTopLevelAccelerationStructure.instances.emplace_back(RayTracingBLASInstance{
						obj->GetRayTracingModelViewMatrix(),
						(uint32_t)obj->GetFirstGeometryOffset(), // gl_InstanceCustomIndexKHR
						obj->GetRayTracingMask(),
						Geometry::rayTracingShaderOffsets[obj->GetObjectType()], // shaderInstanceOffset
						VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR, // VkGeometryInstanceFlagsKHR flags
						rayTracingBottomLevelAccelerationStructures[obj->GetRayTracingBlasIndex()].handle
					});
				}
			} else {
				// Object is Inactive, remove instance
				if (int currIndex = obj->GetRayTracingInstanceIndex(); currIndex != -1) {
					// Swap element position with last element, then erase last instance element
					int lastIndex = rayTracingTopLevelAccelerationStructure.instances.size() - 1;
					if (currIndex != lastIndex) {
						auto lastObj = std::find_if(scene.objectInstances.begin(), scene.objectInstances.end(), [lastIndex](auto* o){return o && o->GetRayTracingInstanceIndex() == lastIndex;});
						if (lastObj != scene.objectInstances.end()) {
							(*lastObj)->SetRayTracingInstanceIndex(currIndex);
						}
						rayTracingTopLevelAccelerationStructure.instances[currIndex] = rayTracingTopLevelAccelerationStructure.instances[lastIndex];
					}
					rayTracingTopLevelAccelerationStructure.instances.pop_back();
					obj->SetRayTracingInstanceIndex(-1);
				}
			}
		}
		
		// Ray Tracing Top Level Acceleration Structure
		// Update object transforms
		for (auto* obj : scene.objectInstances) if (obj && obj->IsActive()) {
			if (obj->GetRayTracingInstanceIndex() != -1) {
				rayTracingTopLevelAccelerationStructure.instances[obj->GetRayTracingInstanceIndex()].transform = obj->GetRayTracingModelViewMatrix();
			}
		}
		
		scene.CollectGarbage();
		scene.Unlock();
	}
	
	void LowPriorityFrameUpdate() override {
		
		// Histogram
		glm::vec4 luminance;
		totalLuminance.ReadFromMappedData(&luminance);
		if (luminance.a > 0) {
			scene.camera.luminance = glm::vec4(glm::vec3(luminance) / luminance.a, exposureFactor);
		}
		
		// Submodules
		for (auto* submodule : renderingSubmodules) {
			submodule->LowPriorityFrameUpdate();
		}
		
		scene.Lock();
		
		// Ray Tracing Bottom Level Acceleration Structures
		// Add/Remove/Update geometries
		for (auto* obj : scene.objectInstances) if (obj) {
			// Geometry
			if (obj->IsGeometriesDirty() && obj->IsActive()) {
				obj->GenerateGeometries();
				
				auto cmdBuffer = BeginSingleTimeCommands(lowPriorityComputeQueue);
					obj->PushGeometries(renderingDevice, cmdBuffer);
			
					// int totalVertices, totalIndices;
					// obj->GetGeometriesTotals(&totalVertices, &totalIndices);
					// VkBufferMemoryBarrier barriers[3] {{},{},{}};
					// 	// vertices
					// 	barriers[0].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
					// 	barriers[0].srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
					// 	barriers[0].dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
					// 	barriers[0].offset = obj->GetFirstGeometryVertexOffset() * sizeof(Geometry::VertexBuffer_T);
					// 	barriers[0].size = totalVertices * sizeof(Geometry::VertexBuffer_T);
					// 	barriers[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
					// 	barriers[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
					// 	barriers[0].buffer = Geometry::globalBuffers.vertexBuffer.deviceLocalBuffer.buffer;
					// 	// indices
					// 	barriers[1].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
					// 	barriers[1].srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
					// 	barriers[1].dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
					// 	barriers[1].offset = obj->GetFirstGeometryIndexOffset() * sizeof(Geometry::IndexBuffer_T);
					// 	barriers[1].size = totalIndices * sizeof(Geometry::IndexBuffer_T);
					// 	barriers[1].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
					// 	barriers[1].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
					// 	barriers[1].buffer = Geometry::globalBuffers.indexBuffer.deviceLocalBuffer.buffer;
					// 	// geometryInfos
					// 	barriers[2].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
					// 	barriers[2].srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
					// 	barriers[2].dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
					// 	barriers[2].offset = obj->GetFirstGeometryOffset() * sizeof(Geometry::GeometryBuffer_T);
					// 	barriers[2].size = sizeof(Geometry::GeometryBuffer_T);
					// 	barriers[2].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
					// 	barriers[2].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
					// 	barriers[2].buffer = Geometry::globalBuffers.geometryBuffer.deviceLocalBuffer.buffer;
					// renderingDevice->CmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, 0, 0, nullptr, 3, barriers, 0, nullptr);
					
					BottomLevelAccelerationStructure* blas;
					if (obj->GetRayTracingBlasIndex() == -1) {
						obj->SetRayTracingBlasIndex(rayTracingBottomLevelAccelerationStructures.size());
						blas = &rayTracingBottomLevelAccelerationStructures.emplace_back();
					} else {
						blas = &rayTracingBottomLevelAccelerationStructures[obj->GetRayTracingBlasIndex()];
					}
					blas->Build(renderingDevice, cmdBuffer, obj->GetGeometries());
				EndSingleTimeCommands(lowPriorityComputeQueue, cmdBuffer);
			}
		}

		scene.Unlock();
		
	}
	
	#ifdef _ENABLE_IMGUI
		void RunImGui() {
			// Submodules
			ImGui::SetNextWindowSize({405, 285});
			ImGui::Begin("Settings and Modules");
			ImGui::Checkbox("TXAA", &scene.camera.txaa);
			ImGui::Checkbox("Debug G-Buffers", &scene.camera.debug);
			ImGui::SliderFloat("HDR Exposure", &exposureFactor, 0, 8);
			for (auto* submodule : renderingSubmodules) {
				ImGui::Separator();
				submodule->RunImGui();
			}
			ImGui::End();
		}
	#endif

};
