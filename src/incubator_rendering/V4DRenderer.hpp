#pragma once
#include <v4d.h>

#include "RenderTargetGroup.hpp"

using namespace v4d::graphics;
using namespace v4d::graphics::vulkan::rtx;

const uint32_t MAX_RAY_TRACING_GEOMETRIES = 100000;
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
		VkAccelerationStructureNV accelerationStructure = VK_NULL_HANDLE;
		VkDeviceMemory memory = VK_NULL_HANDLE;
		uint64_t handle = 0;
		VkAccelerationStructureCreateInfoNV createInfo {};
	};

	struct BottomLevelAccelerationStructure : public AccelerationStructure {
		std::vector<VkGeometryNV> rayTracingGeometries {};
		
		bool built = false;
		
		void Build(Device* device, Queue& queue, const std::vector<Geometry*>& geometries) {
			
			rayTracingGeometries.clear();
			rayTracingGeometries.reserve(geometries.size());
			for (auto* geometry : geometries) if (geometry->active) {
				rayTracingGeometries.push_back(geometry->GetRayTracingGeometry());
			}
			
			if (accelerationStructure == VK_NULL_HANDLE) Create(device);
			
			// Allocate and bind memory for bottom level acceleration structure
			VkMemoryRequirements2 memoryRequirementsBlasObject {};
			{
				VkAccelerationStructureMemoryRequirementsInfoNV memoryRequirementsInfo {};
				memoryRequirementsInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_INFO_NV;
				memoryRequirementsInfo.type = VK_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_TYPE_OBJECT_NV;
				memoryRequirementsInfo.accelerationStructure = accelerationStructure;
				device->GetAccelerationStructureMemoryRequirementsNV(&memoryRequirementsInfo, &memoryRequirementsBlasObject);
			}
			VkMemoryAllocateInfo memoryAllocateInfo {
				VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
				nullptr,// pNext
				memoryRequirementsBlasObject.memoryRequirements.size,// VkDeviceSize allocationSize
				device->GetPhysicalDevice()->FindMemoryType(memoryRequirementsBlasObject.memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)// memoryTypeIndex
			};
			if (device->AllocateMemory(&memoryAllocateInfo, nullptr, &memory) != VK_SUCCESS)
				throw std::runtime_error("Failed to allocate memory for bottom level acceleration structure");
			VkBindAccelerationStructureMemoryInfoNV accelerationStructureMemoryInfo {
				VK_STRUCTURE_TYPE_BIND_ACCELERATION_STRUCTURE_MEMORY_INFO_NV,
				nullptr,// pNext
				accelerationStructure,// accelerationStructure
				memory,// memory
				0,// VkDeviceSize memoryOffset
				0,// uint32_t deviceIndexCount
				nullptr// pDeviceIndices
			};
			if (device->BindAccelerationStructureMemoryNV(1, &accelerationStructureMemoryInfo) != VK_SUCCESS)
				throw std::runtime_error("Failed to bind bottom level acceleration structure memory");
				
			// Get bottom level acceleration structure handle for use in top level instances
			if (device->GetAccelerationStructureHandleNV(accelerationStructure, sizeof(uint64_t), &handle))
				throw std::runtime_error("Failed to get bottom level acceleration structure handle");
				
			// Scratch buffer
			VkMemoryRequirements2 memoryRequirementsBlasScratchBuffer {};
			{
				VkAccelerationStructureMemoryRequirementsInfoNV memoryRequirementsInfo {};
				memoryRequirementsInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_INFO_NV;
				memoryRequirementsInfo.type = built? VK_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_TYPE_UPDATE_SCRATCH_NV : VK_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_TYPE_BUILD_SCRATCH_NV;
				memoryRequirementsInfo.accelerationStructure = accelerationStructure;
				device->GetAccelerationStructureMemoryRequirementsNV(&memoryRequirementsInfo, &memoryRequirementsBlasScratchBuffer);
			}
			Buffer scratchBuffer(VK_BUFFER_USAGE_RAY_TRACING_BIT_NV, memoryRequirementsBlasScratchBuffer.memoryRequirements.size);
			scratchBuffer.Allocate(device, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
			
			// Build bottom level acceleration structure
			auto cmdBuffer = device->BeginSingleTimeCommands(queue);
				
				VkAccelerationStructureInfoNV accelerationStructBuildInfo {};
				accelerationStructBuildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_INFO_NV;
				accelerationStructBuildInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_NV | VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_NV;
				accelerationStructBuildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_NV;
				accelerationStructBuildInfo.geometryCount = (uint)rayTracingGeometries.size();
				accelerationStructBuildInfo.pGeometries = rayTracingGeometries.data();
				accelerationStructBuildInfo.instanceCount = 0;
				
				device->CmdBuildAccelerationStructureNV(
					cmdBuffer, 
					&accelerationStructBuildInfo, 
					VK_NULL_HANDLE, 
					0, 
					built? VK_TRUE : VK_FALSE, 
					accelerationStructure, 
					accelerationStructure, 
					scratchBuffer.buffer, 
					0
				);
				
				// VkMemoryBarrier memoryBarrier {
				// 	VK_STRUCTURE_TYPE_MEMORY_BARRIER,
				// 	nullptr,// pNext
				// 	VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_NV | VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_NV,// VkAccessFlags srcAccessMask
				// 	VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_NV | VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_NV,// VkAccessFlags dstAccessMask
				// };
				// device->CmdPipelineBarrier(
				// 	cmdBuffer, 
				// 	VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_NV, 
				// 	VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_NV, 
				// 	0, 
				// 	1, &memoryBarrier, 
				// 	0, 0, 
				// 	0, 0
				// );
			
			device->EndSingleTimeCommands(queue, cmdBuffer);
			scratchBuffer.Free(device);
			
			built = true;
		}
		
		void Create(Device* device) {
			createInfo = {
				VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_NV,
				nullptr,// pNext
				0,// VkDeviceSize compactedSize
				{// VkAccelerationStructureInfoNV info
					VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_INFO_NV,
					nullptr,// pNext
					VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_NV,// type
					VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_NV | VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_NV,// flags
					0,// instanceCount
					(uint)rayTracingGeometries.size(),// geometryCount
					rayTracingGeometries.data()// VkGeometryNV pGeometries
				}
			};
			
			if (device->CreateAccelerationStructureNV(&createInfo, nullptr, &accelerationStructure) != VK_SUCCESS)
				throw std::runtime_error("Failed to create bottom level acceleration structure");
		}
		
		void Destroy(Device* device) {
			if (memory) device->FreeMemory(memory, nullptr);
			if (accelerationStructure) device->DestroyAccelerationStructureNV(accelerationStructure, nullptr);
			rayTracingGeometries.clear();
			built = false;
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
		
		bool built = false;
		
		void Build(Device* device, Queue& queue) {
			// if (instances.size() == 0) return;
			
			// Instance buffer
			StagedBuffer instanceBuffer(VK_BUFFER_USAGE_RAY_TRACING_BIT_NV);
			instanceBuffer.AddSrcDataPtr(&instances);
			instanceBuffer.Allocate(device);
			auto cmdBuffer = device->BeginSingleTimeCommands(queue);
				instanceBuffer.Update(device, cmdBuffer);
			device->EndSingleTimeCommands(queue, cmdBuffer);
			
			VkAccelerationStructureMemoryRequirementsInfoNV memoryRequirementsInfo {};
			memoryRequirementsInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_INFO_NV;
			memoryRequirementsInfo.type = built? VK_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_TYPE_UPDATE_SCRATCH_NV : VK_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_TYPE_BUILD_SCRATCH_NV;
			VkMemoryRequirements2 memoryRequirementsTopLevel {};
			memoryRequirementsInfo.accelerationStructure = accelerationStructure;
			device->GetAccelerationStructureMemoryRequirementsNV(&memoryRequirementsInfo, &memoryRequirementsTopLevel);
			// Send scratch buffer
			Buffer scratchBuffer(VK_BUFFER_USAGE_RAY_TRACING_BIT_NV, memoryRequirementsTopLevel.memoryRequirements.size);
			scratchBuffer.Allocate(device, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
			
			{
				auto cmdBuffer = device->BeginSingleTimeCommands(queue);
					
					VkAccelerationStructureInfoNV accelerationStructBuildInfo {};
					accelerationStructBuildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_INFO_NV;
					accelerationStructBuildInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_NV | VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_NV;
					accelerationStructBuildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_NV;
					accelerationStructBuildInfo.geometryCount = 0;
					accelerationStructBuildInfo.pGeometries = nullptr;
					accelerationStructBuildInfo.instanceCount = (uint)instances.size();
					
					device->CmdBuildAccelerationStructureNV(
						cmdBuffer, 
						&accelerationStructBuildInfo, 
						instanceBuffer.deviceLocalBuffer.buffer, 
						0, 
						built ? VK_TRUE : VK_FALSE, 
						accelerationStructure, 
						accelerationStructure, 
						scratchBuffer.buffer, 
						0
					);
					
					// VkMemoryBarrier memoryBarrier {
					// 	VK_STRUCTURE_TYPE_MEMORY_BARRIER,
					// 	nullptr,// pNext
					// 	VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_NV | VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_NV,// VkAccessFlags srcAccessMask
					// 	VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_NV | VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_NV,// VkAccessFlags dstAccessMask
					// };
					
					// device->CmdPipelineBarrier(
					// 	cmdBuffer, 
					// 	VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_NV, 
					// 	VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_NV, 
					// 	0, 
					// 	1, &memoryBarrier, 
					// 	0, 0, 
					// 	0, 0
					// );
				
				device->EndSingleTimeCommands(queue, cmdBuffer);
			}
			
			built = true;
			
			scratchBuffer.Free(device);
			instanceBuffer.Free(device);
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
	Buffer rayTracingShaderBindingTableBuffer {VK_BUFFER_USAGE_RAY_TRACING_BIT_NV};
	VkPhysicalDeviceRayTracingPropertiesNV rayTracingProperties{};
	
	void CreateRayTracingAccelerationStructures() {
		// Create Top Level acceleration structure
		VkAccelerationStructureCreateInfoNV accStructCreateInfo {
			VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_NV,
			nullptr,// pNext
			0,// VkDeviceSize compactedSize
			{// VkAccelerationStructureInfoNV info
				VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_INFO_NV,
				nullptr,// pNext
				VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_NV,// type
				VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_NV | VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_NV, // VkGeometryInstanceFlagBitsNV flags
				std::min(MAX_RAY_TRACING_GEOMETRIES, (uint32_t)rayTracingProperties.maxInstanceCount),// instanceCount
				0,// geometryCount
				nullptr// VkGeometryNV pGeometries
			}
		};
		if (renderingDevice->CreateAccelerationStructureNV(&accStructCreateInfo, nullptr, &rayTracingTopLevelAccelerationStructure.accelerationStructure) != VK_SUCCESS)
			throw std::runtime_error("Failed to create top level acceleration structure");
		
		VkAccelerationStructureMemoryRequirementsInfoNV memoryRequirementsInfo {};
		memoryRequirementsInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_INFO_NV;
		memoryRequirementsInfo.type = VK_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_TYPE_OBJECT_NV;
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
	
	void DestroyRayTracingAccelerationStructures() {
		// Geometry instances
		for (auto& instance : rayTracingTopLevelAccelerationStructure.instances) {
			instance.accelerationStructureHandle = VK_NULL_HANDLE;
		}
		// Acceleration Structures
		if (rayTracingTopLevelAccelerationStructure.accelerationStructure != VK_NULL_HANDLE) {
			renderingDevice->FreeMemory(rayTracingTopLevelAccelerationStructure.memory, nullptr);
			renderingDevice->DestroyAccelerationStructureNV(rayTracingTopLevelAccelerationStructure.accelerationStructure, nullptr);
			rayTracingTopLevelAccelerationStructure.accelerationStructure = VK_NULL_HANDLE;
			rayTracingTopLevelAccelerationStructure.built = false;
		}
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
		RequiredDeviceExtension(VK_NV_RAY_TRACING_EXTENSION_NAME); // NVidia's RayTracing extension
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
		rayTracingProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PROPERTIES_NV;
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
		baseDescriptorSet_0->AddBinding_uniformBuffer(0, &cameraUniformBuffer.deviceLocalBuffer, VK_SHADER_STAGE_ALL_GRAPHICS | VK_SHADER_STAGE_INTERSECTION_BIT_NV | VK_SHADER_STAGE_RAYGEN_BIT_NV | VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV | VK_SHADER_STAGE_ANY_HIT_BIT_NV);
		baseDescriptorSet_0->AddBinding_storageBuffer(1, &Geometry::globalBuffers.objectBuffer.deviceLocalBuffer, VK_SHADER_STAGE_INTERSECTION_BIT_NV | VK_SHADER_STAGE_RAYGEN_BIT_NV | VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV | VK_SHADER_STAGE_ANY_HIT_BIT_NV);
		baseDescriptorSet_0->AddBinding_storageBuffer(2, &Geometry::globalBuffers.lightBuffer.deviceLocalBuffer, VK_SHADER_STAGE_RAYGEN_BIT_NV | VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV | VK_SHADER_STAGE_ANY_HIT_BIT_NV);
		baseDescriptorSet_0->AddBinding_storageBuffer(3, &activeLightsUniformBuffer.deviceLocalBuffer, VK_SHADER_STAGE_ALL_GRAPHICS | VK_SHADER_STAGE_RAYGEN_BIT_NV | VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV | VK_SHADER_STAGE_ANY_HIT_BIT_NV);
		
		auto* rayTracingDescriptorSet_1 = descriptorSets.emplace_back(new DescriptorSet(1));
		rayTracingDescriptorSet_1->AddBinding_accelerationStructure(0, &rayTracingTopLevelAccelerationStructure.accelerationStructure, VK_SHADER_STAGE_RAYGEN_BIT_NV | VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV);
		rayTracingDescriptorSet_1->AddBinding_imageView(1, &renderTargetGroup.GetLitImage(), VK_SHADER_STAGE_RAYGEN_BIT_NV);
		rayTracingDescriptorSet_1->AddBinding_imageView(2, &renderTargetGroup.GetDepthImage(), VK_SHADER_STAGE_RAYGEN_BIT_NV);
		rayTracingDescriptorSet_1->AddBinding_storageBuffer(3, &Geometry::globalBuffers.geometryBuffer.deviceLocalBuffer, VK_SHADER_STAGE_INTERSECTION_BIT_NV | VK_SHADER_STAGE_RAYGEN_BIT_NV | VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV | VK_SHADER_STAGE_ANY_HIT_BIT_NV);
		rayTracingDescriptorSet_1->AddBinding_storageBuffer(4, &Geometry::globalBuffers.indexBuffer.deviceLocalBuffer, VK_SHADER_STAGE_RAYGEN_BIT_NV | VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV | VK_SHADER_STAGE_ANY_HIT_BIT_NV);
		rayTracingDescriptorSet_1->AddBinding_storageBuffer(5, &Geometry::globalBuffers.vertexBuffer.deviceLocalBuffer, VK_SHADER_STAGE_INTERSECTION_BIT_NV | VK_SHADER_STAGE_RAYGEN_BIT_NV | VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV | VK_SHADER_STAGE_ANY_HIT_BIT_NV);
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
		
		Geometry::globalBuffers.Allocate(renderingDevice);
	}
	
	void FreeBuffers() override {
	
		// Scene Objects
		for (auto* obj : scene.objectInstances) {
			obj->ClearGeometries();
		}
		
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
		renderingDevice->CmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_NV, shaderBindingTable.GetPipeline());
		shaderBindingTable.GetPipelineLayout()->Bind(renderingDevice, commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_NV);
		VkDeviceSize bindingStride = rayTracingProperties.shaderGroupHandleSize;
		VkDeviceSize bindingOffsetMissShader = bindingStride * shaderBindingTable.GetMissGroupOffset();
		VkDeviceSize bindingOffsetHitShader = bindingStride * shaderBindingTable.GetHitGroupOffset();
		int width = (int)((float)swapChain->extent.width);
		int height = (int)((float)swapChain->extent.height);
		renderingDevice->CmdTraceRaysNV(
			commandBuffer, 
			rayTracingShaderBindingTableBuffer.buffer, 0,
			rayTracingShaderBindingTableBuffer.buffer, bindingOffsetMissShader, bindingStride,
			rayTracingShaderBindingTableBuffer.buffer, bindingOffsetHitShader, bindingStride,
			VK_NULL_HANDLE, 0, 0,
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
		
		rayTracingBottomLevelAccelerationStructures.reserve(scene.objectInstances.size());
		rayTracingTopLevelAccelerationStructure.instances.reserve(scene.objectInstances.size());
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
		
		// Update object transforms and light sources (Use all lights for now)
		nbActiveLights = 0;
		for (auto* obj : scene.objectInstances) if (obj->IsActive()) {
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
		
		// Ray Tracing Acceleration Structure
		bool accelerationStructureDirty = false;
		// Add/Remove/Update geometries
		for (auto* obj : scene.objectInstances) {
			// Geometry
			if (obj->IsGeometriesDirty()) {
				if (obj->IsActive()) obj->GenerateGeometries();
				if (obj->IsActive() && obj->HasActiveGeometries()) {
					BottomLevelAccelerationStructure* blas;
					if (obj->GetRayTracingBlasIndex() == -1) {
						obj->SetRayTracingBlasIndex(rayTracingBottomLevelAccelerationStructures.size());
						blas = &rayTracingBottomLevelAccelerationStructures.emplace_back();
					} else {
						blas = &rayTracingBottomLevelAccelerationStructures[obj->GetRayTracingBlasIndex()];
					}
					
					auto cmdBuffer = BeginSingleTimeCommands(transferQueue);
						obj->PushGeometries(renderingDevice, cmdBuffer);
					EndSingleTimeCommands(transferQueue, cmdBuffer);
					
					blas->Build(renderingDevice, graphicsQueue, obj->GetGeometries());
					accelerationStructureDirty = true;
					
					// Instance
					if (obj->GetRayTracingInstanceIndex() == -1) {
						obj->SetRayTracingInstanceIndex(rayTracingTopLevelAccelerationStructure.instances.size());
						rayTracingTopLevelAccelerationStructure.instances.emplace_back(RayTracingGeometryInstance{
							obj->GetRayTracingModelViewMatrix(),
							(uint32_t)obj->GetFirstGeometryOffset(), // gl_InstanceCustomIndexNV
							obj->GetRayTracingMask(), //TODO mask
							Geometry::rayTracingShaderOffsets[obj->GetObjectType()], // instanceOffset
							VK_GEOMETRY_INSTANCE_TRIANGLE_CULL_DISABLE_BIT_NV, // VkGeometryInstanceFlagBitsNV flags
							blas->handle
						});
					}
				} else {
					// Object is Inactive
					if (int currIndex = obj->GetRayTracingInstanceIndex(); currIndex != -1) {
						// Swap element position with last element, then erase last instance element
						int lastIndex = rayTracingTopLevelAccelerationStructure.instances.size() - 1;
						if (currIndex != lastIndex) {
							auto lastObj = std::find_if(scene.objectInstances.begin(), scene.objectInstances.end(), [lastIndex](auto* o){return o->GetRayTracingInstanceIndex() == lastIndex;});
							if (lastObj != scene.objectInstances.end()) {
								(*lastObj)->SetRayTracingInstanceIndex(currIndex);
							}
							rayTracingTopLevelAccelerationStructure.instances[currIndex] = rayTracingTopLevelAccelerationStructure.instances[lastIndex];
						}
						rayTracingTopLevelAccelerationStructure.instances.pop_back();
						obj->SetRayTracingInstanceIndex(-1);
						accelerationStructureDirty = true;
					}
				}
			}
		}
		// Update object transforms
		for (auto* obj : scene.objectInstances) if (obj->IsActive()) {
			if (obj->GetRayTracingInstanceIndex() != -1) {
				rayTracingTopLevelAccelerationStructure.instances[obj->GetRayTracingInstanceIndex()].transform = obj->GetRayTracingModelViewMatrix();
				accelerationStructureDirty = true;
			}
		}
		if (accelerationStructureDirty) {
			rayTracingTopLevelAccelerationStructure.Build(renderingDevice, graphicsQueue);
		}
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
