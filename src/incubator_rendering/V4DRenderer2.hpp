#pragma once
#include <v4d.h>

using namespace v4d::graphics;
using namespace v4d::graphics::vulkan::rtx;

const uint32_t MAX_ACTIVE_LIGHTS = 256;

class V4DRenderer2 : public v4d::graphics::Renderer {
	using v4d::graphics::Renderer::Renderer;
	
	std::vector<v4d::modules::Rendering*> renderingSubmodules {};
	
	DescriptorSet baseDescriptorSet_0;
	
private: // UI
	float uiImageScale = 1.0;
	Image uiImage { VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT ,1,1, { VK_FORMAT_R8G8B8A8_SNORM }};
	RenderPass uiRenderPass;
	PipelineLayout uiPipelineLayout;
	DescriptorSet uiDescriptorSet_1;
	
	void CreateUiResources() {
		uiImage.Create(renderingDevice, 
			(uint)((float)swapChain->extent.width * uiImageScale), 
			(uint)((float)swapChain->extent.height * uiImageScale)
		);
		TransitionImageLayout(uiImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
	}
	void DestroyUiResources() {
		uiImage.Destroy(renderingDevice);
	}
	
	void CreateUiPipeline() {
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
		for (auto* shader : rasterShaders["ui"]) {
			shader->SetRenderPass(&uiImage, uiRenderPass.handle, 0);
			shader->AddColorBlendAttachmentState();
			shader->CreatePipeline(renderingDevice);
		}
	}
	void DestroyUiPipeline() {
		for (auto* shader : rasterShaders["ui"]) {
			shader->DestroyPipeline(renderingDevice);
		}
		uiRenderPass.DestroyFrameBuffers(renderingDevice);
		uiRenderPass.Destroy(renderingDevice);
	}
	
	void ConfigureUiShaders() {
		//...
	}
	
	#ifdef _ENABLE_IMGUI
		void LoadImGui() {
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
		void UnloadImGui() {
			ImGui_ImplVulkan_Shutdown();
		}
		void DrawImGui(VkCommandBuffer commandBuffer) {
			ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer);
		}
	#endif


private: // Main Rendering
	Image litImage { VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT ,1,1, { VK_FORMAT_R16G16B16A16_SFLOAT }};
	Image depthImage { VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT ,1,1, { VK_FORMAT_R32_SFLOAT } };
	DescriptorSet rasterDescriptorSet_1;
	
	void CreateRenderingResources() {
		uint rasterWidth = (uint)((float)swapChain->extent.width);
		uint rasterHeight = (uint)((float)swapChain->extent.height);
		
		litImage.Create(renderingDevice, rasterWidth, rasterHeight);
		depthImage.Create(renderingDevice, rasterWidth, rasterHeight);
		
		TransitionImageLayout(litImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
		TransitionImageLayout(depthImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
	}
	void DestroyRenderingResources() {
		litImage.Destroy(renderingDevice);
		depthImage.Destroy(renderingDevice);
	}
	
private: // Ray Tracing
	VkPhysicalDeviceRayTracingPropertiesKHR rayTracingProperties{};
	VkPhysicalDeviceRayTracingFeaturesKHR rayTracingFeatures {};
	PipelineLayout rayTracingPipelineLayout;
	ShaderBindingTable shaderBindingTable {rayTracingPipelineLayout, "incubator_rendering/assets/shaders/rtx.rgen"};
	DescriptorSet rayTracingDescriptorSet_1;
	AccelerationStructure topLevelAccelerationStructure {};
	Buffer rayTracingShaderBindingTableBuffer {VK_BUFFER_USAGE_RAY_TRACING_BIT_KHR};
	std::mutex rayTracingInstanceMutex, blasBuildQueueMutex;
	std::vector<VkAccelerationStructureBuildGeometryInfoKHR> blasQueueBuildGeometryInfos {};
	std::vector<VkAccelerationStructureBuildOffsetInfoKHR*> blasQueueBuildOffsetInfos {};
	Buffer rayTracingInstanceBuffer {VK_BUFFER_USAGE_RAY_TRACING_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, sizeof(RayTracingBLASInstance)*RAY_TRACING_TLAS_MAX_INSTANCES};
	RayTracingBLASInstance* rayTracingInstances = nullptr;
	uint32_t nbRayTracingInstances = 0;
	Buffer globalScratchBuffer {VK_BUFFER_USAGE_RAY_TRACING_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, 16*1024*1024/* 16 MB */};
	static const bool globalScratchDynamicSize = false;
	static const int maxBlasBuildsPerFrame = 8;
	std::map<int/*instance index*/, std::shared_ptr<Geometry>> activeGeometries {};
	std::vector<std::shared_ptr<AccelerationStructure>> inactiveBlas {};
	std::vector<std::shared_ptr<AccelerationStructure>> blasBuildsForGlobalScratchBufferReallocation {};
	
	void CreateRayTracingResources() {
		topLevelAccelerationStructure.AssignTopLevel();
		topLevelAccelerationStructure.Create(renderingDevice, true);
		topLevelAccelerationStructure.Allocate(renderingDevice);
		topLevelAccelerationStructure.SetInstanceBuffer(renderingDevice, rayTracingInstanceBuffer.buffer);
	}
	void DestroyRayTracingResources() {
		topLevelAccelerationStructure.Free(renderingDevice);
		topLevelAccelerationStructure.Destroy(renderingDevice);
		
		activeGeometries.clear();
		inactiveBlas.clear();
		blasBuildsForGlobalScratchBufferReallocation.clear();
		ResetRayTracingBlasBuilds();
	}
	
	void AllocateRayTracingBuffers() {
		rayTracingInstanceBuffer.Allocate(renderingDevice, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, false);
		rayTracingInstanceBuffer.MapMemory(renderingDevice);
		rayTracingInstances = (RayTracingBLASInstance*)rayTracingInstanceBuffer.data;
		
		// LOG_VERBOSE("Allocated instance buffer " << std::hex << renderingDevice->GetBufferDeviceAddress(rayTracingInstanceBuffer.buffer))
		
		if (AccelerationStructure::useGlobalScratchBuffer) {
			globalScratchBuffer.Allocate(renderingDevice, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, false);
			topLevelAccelerationStructure.SetGlobalScratchBuffer(renderingDevice, globalScratchBuffer.buffer);
		}
		
		// LOG_VERBOSE("Allocated global scratch buffer " << std::hex << renderingDevice->GetBufferDeviceAddress(globalScratchBuffer.buffer))
	}
	void FreeRayTracingBuffers() {
		rayTracingInstances = nullptr;
		nbRayTracingInstances = 0;
		rayTracingInstanceBuffer.UnmapMemory(renderingDevice);
		rayTracingInstanceBuffer.Free(renderingDevice);
		
		if (AccelerationStructure::useGlobalScratchBuffer) {
			globalScratchBuffer.Free(renderingDevice);
		}
	}
	
	void CreateRayTracingPipeline() {
		shaderBindingTable.CreateRayTracingPipeline(renderingDevice);
		rayTracingShaderBindingTableBuffer.size = rayTracingProperties.shaderGroupHandleSize * shaderBindingTable.GetGroups().size();
		rayTracingShaderBindingTableBuffer.Allocate(renderingDevice, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
		shaderBindingTable.WriteShaderBindingTableToBuffer(renderingDevice, &rayTracingShaderBindingTableBuffer, rayTracingProperties.shaderGroupHandleSize);
	}
	void DestroyRayTracingPipeline() {
		rayTracingShaderBindingTableBuffer.Free(renderingDevice);
		shaderBindingTable.DestroyRayTracingPipeline(renderingDevice);
	}
	
	void ConfigureRayTracingShaders() {
		shaderBindingTable.AddMissShader("incubator_rendering/assets/shaders/rtx.rmiss");
		shaderBindingTable.AddMissShader("incubator_rendering/assets/shaders/rtx.shadow.rmiss");
		// Base ray tracing shaders
		Geometry::rayTracingShaderOffsets["standard"] = shaderBindingTable.AddHitShader("incubator_rendering/assets/shaders/rtx.rchit" /*, "incubator_rendering/assets/shaders/rtx.rahit"*/ );
		Geometry::rayTracingShaderOffsets["sphere"] = shaderBindingTable.AddHitShader("incubator_rendering/assets/shaders/rtx.sphere.rchit", "", "incubator_rendering/assets/shaders/rtx.sphere.rint");
		Geometry::rayTracingShaderOffsets["light"] = shaderBindingTable.AddHitShader("incubator_rendering/assets/shaders/rtx.light.rchit", "", "incubator_rendering/assets/shaders/rtx.sphere.rint");
	}
	
	void RecordRayTracingCommands(VkCommandBuffer commandBuffer) {
		VkDeviceSize bindingStride = rayTracingProperties.shaderGroupHandleSize;
		VkDeviceSize bindingOffsetMissShader = bindingStride * shaderBindingTable.GetMissGroupOffset();
		VkDeviceSize bindingOffsetHitShader = bindingStride * shaderBindingTable.GetHitGroupOffset();
		
		VkStridedBufferRegionKHR rayGen {rayTracingShaderBindingTableBuffer.buffer, 0, bindingStride, rayTracingShaderBindingTableBuffer.size};
		VkStridedBufferRegionKHR rayMiss {rayTracingShaderBindingTableBuffer.buffer, bindingOffsetMissShader, bindingStride, rayTracingShaderBindingTableBuffer.size};
		VkStridedBufferRegionKHR rayHit {rayTracingShaderBindingTableBuffer.buffer, bindingOffsetHitShader, bindingStride, rayTracingShaderBindingTableBuffer.size};
		VkStridedBufferRegionKHR callable {VK_NULL_HANDLE, 0, bindingStride, rayTracingShaderBindingTableBuffer.size};
		
		int width = (int)((float)swapChain->extent.width);
		int height = (int)((float)swapChain->extent.height);
		
		renderingDevice->CmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, shaderBindingTable.GetPipeline());
		shaderBindingTable.GetPipelineLayout()->Bind(renderingDevice, commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR);
		renderingDevice->CmdTraceRaysKHR(
			commandBuffer, 
			&rayGen,
			&rayMiss,
			&rayHit,
			&callable,
			width, height, 1
		);
	}
	
	void BuildBottomLevelRayTracingAccelerationStructures(Device* device, VkCommandBuffer commandBuffer) {
		// Build all new/updated bottom levels
		std::lock_guard lock(blasBuildQueueMutex);
		if (blasQueueBuildGeometryInfos.size() > 0) {
			// #ifdef V4D_RENDERER_RAYTRACING_USE_DEVICE_LOCAL_VERTEX_INDEX_BUFFERS
			// 	VkBufferMemoryBarrier bufferBarriers[2];
			// 	bufferBarriers[0] = {};
			// 		bufferBarriers[0].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
			// 		bufferBarriers[0].srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT;
			// 		bufferBarriers[0].dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
			// 		bufferBarriers[0].offset = 0;
			// 		bufferBarriers[0].size = Geometry::globalBuffers.vertexBuffer.deviceLocalBuffer.size;
			// 		bufferBarriers[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			// 		bufferBarriers[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			// 		bufferBarriers[0].buffer = Geometry::globalBuffers.vertexBuffer.deviceLocalBuffer.buffer;
			// 	bufferBarriers[1] = {};
			// 		bufferBarriers[1].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
			// 		bufferBarriers[1].srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT;
			// 		bufferBarriers[1].dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
			// 		bufferBarriers[1].offset = 0;
			// 		bufferBarriers[1].size = Geometry::globalBuffers.indexBuffer.deviceLocalBuffer.size;
			// 		bufferBarriers[1].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			// 		bufferBarriers[1].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			// 		bufferBarriers[1].buffer = Geometry::globalBuffers.indexBuffer.deviceLocalBuffer.buffer;
			// 	device->CmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, 0, 0, nullptr, 2, bufferBarriers, 0, nullptr);
			// #endif
			
			device->CmdBuildAccelerationStructureKHR(commandBuffer, blasQueueBuildGeometryInfos.size(), blasQueueBuildGeometryInfos.data(), blasQueueBuildOffsetInfos.data());
			
			VkMemoryBarrier memoryBarrier {
				VK_STRUCTURE_TYPE_MEMORY_BARRIER,
				nullptr,// pNext
				VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR | VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR,// VkAccessFlags srcAccessMask
				VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR | VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR,// VkAccessFlags dstAccessMask
			};
			device->CmdPipelineBarrier(
				commandBuffer, 
				VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, 
				VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, 
				0, 
				1, &memoryBarrier, 
				0, 0, 
				0, 0
			);
		}
	}
	
	void BuildTopLevelRayTracingAccelerationStructure(Device* device, VkCommandBuffer commandBuffer) {
		std::lock_guard lock(rayTracingInstanceMutex);
		static const VkAccelerationStructureBuildOffsetInfoKHR* topLevelAccelerationStructureGeometriesOffsets = &topLevelAccelerationStructure.buildOffsetInfo;
		topLevelAccelerationStructure.SetInstanceCount(nbRayTracingInstances);
		device->CmdBuildAccelerationStructureKHR(commandBuffer, 1, &topLevelAccelerationStructure.buildGeometryInfo, &topLevelAccelerationStructureGeometriesOffsets);
	
		VkMemoryBarrier memoryBarrier {
			VK_STRUCTURE_TYPE_MEMORY_BARRIER,
			nullptr,// pNext
			VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR,// VkAccessFlags srcAccessMask
			VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR | VK_ACCESS_UNIFORM_READ_BIT | VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_MEMORY_READ_BIT,// VkAccessFlags dstAccessMask
		};
		device->CmdPipelineBarrier(
			commandBuffer, 
			VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, 
			VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, 
			0, 
			1, &memoryBarrier, 
			0, 0, 
			0, 0
		);
	}
	
	void AddRayTracingBlasBuild(std::shared_ptr<AccelerationStructure> blas) {
		std::lock_guard lock(blasBuildQueueMutex);
		if (blasQueueBuildGeometryInfos.size() < maxBlasBuildsPerFrame) {
			blasQueueBuildGeometryInfos.push_back(blas->buildGeometryInfo);
			blasQueueBuildOffsetInfos.push_back(&blas->buildOffsetInfo);
			blas->built = true;
		}
	}
	
	void ResetRayTracingBlasBuilds() {
		std::lock_guard lock(blasBuildQueueMutex);
		blasQueueBuildGeometryInfos.clear();
		blasQueueBuildOffsetInfos.clear();
	}
	
	void MakeRayTracingBlas(GeometryInstance* geometryInstance) {
		std::lock_guard lock(blasBuildQueueMutex);
		geometryInstance->geometry->blas = std::make_shared<AccelerationStructure>();
		geometryInstance->geometry->blas->AssignBottomLevel(renderingDevice, geometryInstance->geometry);
		geometryInstance->geometry->blas->Create(renderingDevice);
		geometryInstance->geometry->blas->Allocate(renderingDevice);
	}
	
	void SetRayTracingInstanceTransform(GeometryInstance* geometryInstance, const glm::mat4& transform) {
		std::lock_guard lock(rayTracingInstanceMutex);
		if (geometryInstance->rayTracingInstanceIndex == -1) return;
		rayTracingInstances[geometryInstance->rayTracingInstanceIndex].transform = glm::transpose(transform);
	}
	
	void AddRayTracingInstance(GeometryInstance* geometryInstance) {
		std::lock_guard lock(rayTracingInstanceMutex);
		if (geometryInstance->rayTracingInstanceIndex != -1) return;
		if (!geometryInstance->geometry->blas || !geometryInstance->geometry->blas->handle) return;
		int index = nbRayTracingInstances++;
		rayTracingInstances[index].accelerationStructureHandle = geometryInstance->geometry->blas->handle;
		rayTracingInstances[index].customInstanceId = geometryInstance->geometry->geometryOffset;
		rayTracingInstances[index].mask = geometryInstance->geometry->rayTracingMask;
		rayTracingInstances[index].shaderInstanceOffset = Geometry::rayTracingShaderOffsets[geometryInstance->type];
		rayTracingInstances[index].flags = geometryInstance->geometry->flags;
		rayTracingInstances[index].transform = glm::mat3x4{0};
		geometryInstance->rayTracingInstanceIndex = index;
		activeGeometries[geometryInstance->rayTracingInstanceIndex] = geometryInstance->geometry;
	}
	
	void RemoveRayTracingInstance(GeometryInstance* geometryInstance) {
		std::lock_guard lock(rayTracingInstanceMutex);
		if (geometryInstance->rayTracingInstanceIndex == -1) return;
		int lastIndex = --nbRayTracingInstances;
		int index = geometryInstance->rayTracingInstanceIndex;
		geometryInstance->rayTracingInstanceIndex = -1;
		rayTracingInstances[index] = rayTracingInstances[lastIndex];
		rayTracingInstances[lastIndex].accelerationStructureHandle = 0;
	
		// inactiveBlas.push_back(activeGeometries[index]->blas);
		activeGeometries[index] = activeGeometries[lastIndex];
		activeGeometries[lastIndex] = nullptr;
		
		if (rayTracingInstances[index].accelerationStructureHandle != 0) {
			for (auto* obj : scene.objectInstances) {
				for (auto& geom : obj->GetGeometries()) {
					if (geom.rayTracingInstanceIndex == lastIndex) {
						geom.rayTracingInstanceIndex = index;
						return;
					}
				}
			}
			LOG_ERROR("Object Instance to move to deleted instance index : Not Found")
		}
	}
	
private: // Post Processing
	PipelineLayout postProcessingPipelineLayout, thumbnailPipelineLayout, histogramComputeLayout;
	RenderPass postProcessingRenderPass, thumbnailRenderPass;
	RasterShaderPipeline thumbnailShader {postProcessingPipelineLayout, {
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
	Image ppImage { VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT ,1,1, { VK_FORMAT_R16G16B16A16_SFLOAT }};
	Image historyImage { VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT ,1,1, { VK_FORMAT_R16G16B16A16_SFLOAT }};
	Image thumbnailImage { VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT };
	float thumbnailScale = 1.0/16;
	float exposureFactor = 1;
	
	DescriptorSet postProcessingDescriptorSet_1, thumbnailDescriptorSet_1, histogramDescriptorSet_1;
	
	void CreatePostProcessingResources() {
		uint rasterWidth = (uint)((float)swapChain->extent.width);
		uint rasterHeight = (uint)((float)swapChain->extent.height);
		uint thumbnailWidth = (uint)((float)rasterWidth * thumbnailScale);
		uint thumbnailHeight = (uint)((float)rasterHeight * thumbnailScale);
		
		ppImage.Create(renderingDevice, rasterWidth, rasterHeight);
		historyImage.Create(renderingDevice, rasterWidth, rasterHeight);
		thumbnailImage.Create(renderingDevice, thumbnailWidth, thumbnailHeight, {litImage.format});
		
		TransitionImageLayout(historyImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
		TransitionImageLayout(thumbnailImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
	}
	void DestroyPostProcessingResources() {
		ppImage.Destroy(renderingDevice);
		historyImage.Destroy(renderingDevice);
		thumbnailImage.Destroy(renderingDevice);
	}
	
	void CreatePostProcessingPipeline() {
		
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
			for (auto* shaderPipeline : rasterShaders["thumbnail"]) {
				shaderPipeline->SetRenderPass(&thumbnailImage, thumbnailRenderPass.handle, 0);
				shaderPipeline->AddColorBlendAttachmentState();
				shaderPipeline->CreatePipeline(renderingDevice);
			}
		}
		
		{// Post Processing render pass
			std::array<VkAttachmentDescription, 3> attachments {};
			// std::array<VkAttachmentReference, 0> inputAttachmentRefs {};
			
			// PP image
			attachments[0].format = ppImage.format;
			attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
			attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			attachments[0].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			uint32_t ppAttachmentIndex = postProcessingRenderPass.AddAttachment(attachments[0]);
			
			// History image
			attachments[1].format = historyImage.format;
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
				ppImage.view, 
				historyImage.view, 
				VK_NULL_HANDLE
			});
			
			// Shaders
			for (auto* shader : rasterShaders["postProcessing_0"]) {
				shader->SetRenderPass(swapChain, postProcessingRenderPass.handle, 0);
				shader->AddColorBlendAttachmentState();
				shader->CreatePipeline(renderingDevice);
			}
			for (auto* shader : rasterShaders["postProcessing_1"]) {
				shader->SetRenderPass(swapChain, postProcessingRenderPass.handle, 1);
				shader->AddColorBlendAttachmentState(VK_FALSE);
				shader->CreatePipeline(renderingDevice);
			}
			for (auto* shader : rasterShaders["postProcessing_2"]) {
				shader->SetRenderPass(swapChain, postProcessingRenderPass.handle, 2);
				shader->AddColorBlendAttachmentState();
				shader->CreatePipeline(renderingDevice);
			}
		}
		
		// Compute
		histogramComputeShader.CreatePipeline(renderingDevice);
	}
	void DestroyPostProcessingPipeline() {
		// Thumbnail Gen
		for (auto* shaderPipeline : rasterShaders["thumbnail"]) {
			shaderPipeline->DestroyPipeline(renderingDevice);
		}
		thumbnailRenderPass.DestroyFrameBuffers(renderingDevice);
		thumbnailRenderPass.Destroy(renderingDevice);
		
		// Post-processing
		for (auto[rp, ss] : rasterShaders) if (rp.substr(0, 14) == "postProcessing") {
			for (auto* s : ss) {
				s->DestroyPipeline(renderingDevice);
			}
		}
		postProcessingRenderPass.DestroyFrameBuffers(renderingDevice);
		postProcessingRenderPass.Destroy(renderingDevice);
		
		// Compute
		histogramComputeShader.DestroyPipeline(renderingDevice);
	}
	
	void ConfigurePostProcessingShaders() {
		// Thumbnail Gen
		thumbnailShader.inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
		thumbnailShader.depthStencilState.depthTestEnable = VK_FALSE;
		thumbnailShader.depthStencilState.depthWriteEnable = VK_FALSE;
		thumbnailShader.rasterizer.cullMode = VK_CULL_MODE_NONE;
		thumbnailShader.SetData(3);
		
		// Post-Processing
		for (auto[rp, ss] : rasterShaders) if (rp.substr(0, 14) == "postProcessing") {
			for (auto* s : ss) {
				s->inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
				s->depthStencilState.depthTestEnable = VK_FALSE;
				s->depthStencilState.depthWriteEnable = VK_FALSE;
				s->rasterizer.cullMode = VK_CULL_MODE_NONE;
				s->SetData(3);
			}
		}
	}
	
	void AllocatePostProcessingBuffers() {
		// Compute histogram
		totalLuminance.Allocate(renderingDevice, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, false);
		totalLuminance.MapMemory(renderingDevice);
		glm::vec4 luminance {0,0,0,1};
		totalLuminance.WriteToMappedData(&luminance);
	}
	void FreePostProcessingBuffers() {
		// Compute histogram
		totalLuminance.UnmapMemory(renderingDevice);
		totalLuminance.Free(renderingDevice);
	}
	
	void RecordPostProcessingCommands(VkCommandBuffer commandBuffer, int imageIndex) {
		
		// Gen Thumbnail
		thumbnailRenderPass.Begin(renderingDevice, commandBuffer, thumbnailImage, {{.0,.0,.0,.0}});
			for (auto* shader : rasterShaders["thumbnail"]) {
				shader->Execute(renderingDevice, commandBuffer);
			}
		thumbnailRenderPass.End(renderingDevice, commandBuffer);
		
		// Post Processing
		postProcessingRenderPass.Begin(renderingDevice, commandBuffer, swapChain, {{.0,.0,.0,.0}, {.0,.0,.0,.0}, {.0,.0,.0,.0}}, imageIndex);
			for (auto* shader : rasterShaders["postProcessing_0"]) {
				shader->Execute(renderingDevice, commandBuffer);
			}
			renderingDevice->CmdNextSubpass(commandBuffer, VK_SUBPASS_CONTENTS_INLINE);
			for (auto* shader : rasterShaders["postProcessing_1"]) {
				shader->Execute(renderingDevice, commandBuffer);
			}
			renderingDevice->CmdNextSubpass(commandBuffer, VK_SUBPASS_CONTENTS_INLINE);
			for (auto* shader : rasterShaders["postProcessing_2"]) {
				shader->Execute(renderingDevice, commandBuffer);
			}
		postProcessingRenderPass.End(renderingDevice, commandBuffer);
	}
	void RunDynamicPostProcessingLowPriorityCompute(VkCommandBuffer commandBuffer) {
		// Compute
		histogramComputeShader.SetGroupCounts(1, 1, 1);
		histogramComputeShader.Execute(renderingDevice, commandBuffer);
	}
	void PostProcessingLowPriorityUpdate() {
		// Histogram
		glm::vec4 luminance;
		totalLuminance.ReadFromMappedData(&luminance);
		if (luminance.a > 0) {
			scene.camera.luminance = glm::vec4(glm::vec3(luminance) / luminance.a, exposureFactor);
		}
	}
	
private: // Global Containers
	std::unordered_map<std::string, std::vector<PipelineLayout*>> pipelineLayouts {
		{"raster", {}},
		{"rayTracing", {&rayTracingPipelineLayout}},
		{"thumbnail", {&thumbnailPipelineLayout}},
		{"ui", {&uiPipelineLayout}},
		{"postProcessing", {&postProcessingPipelineLayout}},
		{"histogram", {&histogramComputeLayout}},
	};

	std::unordered_map<std::string, std::vector<RasterShaderPipeline*>> rasterShaders {
		/* RenderPass_SubPass => ShadersList */
		{"thumbnail", {&thumbnailShader}},
		{"postProcessing_0", {&postProcessingShader_txaa}},
		{"postProcessing_1", {&postProcessingShader_history}},
		{"postProcessing_2", {&postProcessingShader_hdr, &postProcessingShader_ui}},
		{"ui", {}},
	};
	
	std::vector<ComputeShaderPipeline*> computeShaders {
		&histogramComputeShader,
	};
	
public: // Scene

	Scene scene {};
	
	StagedBuffer cameraUniformBuffer {VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, sizeof(Camera)};
	
	StagedBuffer activeLightsUniformBuffer {VK_BUFFER_USAGE_STORAGE_BUFFER_BIT};
		uint32_t nbActiveLights = 0;
		uint32_t activeLights[MAX_ACTIVE_LIGHTS];
		
	Buffer totalLuminance {VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, sizeof(glm::vec4)};
	
	std::unordered_map<std::string, Image*> images {
		{"depthImage", &depthImage},
		{"litImage", &litImage},
		{"thumbnail", &thumbnailImage},
		{"historyImage", &historyImage},
		{"ui", &uiImage},
	};
	
	void AllocateSceneBuffers() {
		// Uniform Buffers
		cameraUniformBuffer.Allocate(renderingDevice);
		activeLightsUniformBuffer.Allocate(renderingDevice);
	}
	void FreeSceneBuffers() {
		scene.ClenupObjectInstancesGeometries();
		activeGeometries.clear();
		scene.CollectGarbage();
		
		// Uniform Buffers
		cameraUniformBuffer.Free(renderingDevice);
		activeLightsUniformBuffer.Free(renderingDevice);
	}
	
private: // Init overrides
	void ScorePhysicalDeviceSelection(int& score, PhysicalDevice* physicalDevice) override {
		// Submodules
		for (auto* submodule : renderingSubmodules) {
			submodule->ScorePhysicalDeviceSelection(score, physicalDevice);
		}
	}
	void Init() override {
		// Ray Tracing
		RequiredDeviceExtension(VK_KHR_RAY_TRACING_EXTENSION_NAME); // RayTracing extension
		RequiredDeviceExtension(VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME); // Needed for RayTracing extension
		RequiredDeviceExtension(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME); // Needed for RayTracing extension
		RequiredDeviceExtension(VK_KHR_PIPELINE_LIBRARY_EXTENSION_NAME); // Needed for RayTracing extension
		
		vulkan12DeviceFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
		vulkan12DeviceFeatures.bufferDeviceAddress = VK_TRUE;
		vulkan12DeviceFeatures.pNext = &rayTracingFeatures;
		rayTracingFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_FEATURES_KHR;
		rayTracingFeatures.rayTracing = VK_TRUE;
		
		// UBOs
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
		
		AccelerationStructure::useGlobalScratchBuffer = true;
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
		
		descriptorSets["0_base"] = &baseDescriptorSet_0;
			baseDescriptorSet_0.AddBinding_uniformBuffer(0, &cameraUniformBuffer.deviceLocalBuffer, VK_SHADER_STAGE_ALL_GRAPHICS | VK_SHADER_STAGE_INTERSECTION_BIT_KHR | VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR);
			baseDescriptorSet_0.AddBinding_storageBuffer(1, &Geometry::globalBuffers.objectBuffer.deviceLocalBuffer, VK_SHADER_STAGE_INTERSECTION_BIT_KHR | VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR);
			baseDescriptorSet_0.AddBinding_storageBuffer(2, &Geometry::globalBuffers.lightBuffer.deviceLocalBuffer, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR);
			baseDescriptorSet_0.AddBinding_storageBuffer(3, &activeLightsUniformBuffer.deviceLocalBuffer, VK_SHADER_STAGE_ALL_GRAPHICS | VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR);
		
		descriptorSets["1_raster"] = &rasterDescriptorSet_1;
			//...
		
		descriptorSets["1_rayTracing"] = &rayTracingDescriptorSet_1;
			rayTracingDescriptorSet_1.AddBinding_accelerationStructure(0, &topLevelAccelerationStructure.accelerationStructure, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR);
			rayTracingDescriptorSet_1.AddBinding_imageView(1, &litImage, VK_SHADER_STAGE_RAYGEN_BIT_KHR);
			rayTracingDescriptorSet_1.AddBinding_imageView(2, &depthImage, VK_SHADER_STAGE_RAYGEN_BIT_KHR);
			rayTracingDescriptorSet_1.AddBinding_storageBuffer(3, &Geometry::globalBuffers.geometryBuffer.deviceLocalBuffer, VK_SHADER_STAGE_INTERSECTION_BIT_KHR | VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR | VK_SHADER_STAGE_COMPUTE_BIT);
			#ifdef V4D_RENDERER_RAYTRACING_USE_DEVICE_LOCAL_VERTEX_INDEX_BUFFERS
				rayTracingDescriptorSet_1.AddBinding_storageBuffer(4, &Geometry::globalBuffers.indexBuffer.deviceLocalBuffer, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR | VK_SHADER_STAGE_COMPUTE_BIT);
				rayTracingDescriptorSet_1.AddBinding_storageBuffer(5, &Geometry::globalBuffers.vertexBuffer.deviceLocalBuffer, VK_SHADER_STAGE_INTERSECTION_BIT_KHR | VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR | VK_SHADER_STAGE_COMPUTE_BIT);
			#else
				rayTracingDescriptorSet_1.AddBinding_storageBuffer(4, &Geometry::globalBuffers.indexBuffer, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR | VK_SHADER_STAGE_COMPUTE_BIT);
				rayTracingDescriptorSet_1.AddBinding_storageBuffer(5, &Geometry::globalBuffers.vertexBuffer, VK_SHADER_STAGE_INTERSECTION_BIT_KHR | VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR | VK_SHADER_STAGE_COMPUTE_BIT);
			#endif
		
		descriptorSets["1_thumbnail"] = &thumbnailDescriptorSet_1;
			thumbnailDescriptorSet_1.AddBinding_combinedImageSampler(0, &litImage, VK_SHADER_STAGE_FRAGMENT_BIT);
			
		descriptorSets["1_ui"] = &uiDescriptorSet_1;
			//...
		
		descriptorSets["1_postProcessing"] = &postProcessingDescriptorSet_1;
			postProcessingDescriptorSet_1.AddBinding_combinedImageSampler(0, &litImage, VK_SHADER_STAGE_FRAGMENT_BIT);
			postProcessingDescriptorSet_1.AddBinding_combinedImageSampler(1, &uiImage, VK_SHADER_STAGE_FRAGMENT_BIT);
			postProcessingDescriptorSet_1.AddBinding_inputAttachment(2, &ppImage, VK_SHADER_STAGE_FRAGMENT_BIT);
			postProcessingDescriptorSet_1.AddBinding_combinedImageSampler(3, &historyImage, VK_SHADER_STAGE_FRAGMENT_BIT);
			postProcessingDescriptorSet_1.AddBinding_combinedImageSampler(4, &depthImage, VK_SHADER_STAGE_FRAGMENT_BIT);
		
		descriptorSets["1_histogram"] = &histogramDescriptorSet_1;
			histogramDescriptorSet_1.AddBinding_imageView(0, &thumbnailImage, VK_SHADER_STAGE_COMPUTE_BIT);
			histogramDescriptorSet_1.AddBinding_storageBuffer(1, &totalLuminance, VK_SHADER_STAGE_COMPUTE_BIT);
		
		// Add the same set 0 to all pipeline layouts
		for (auto[name,layouts] : pipelineLayouts) for (auto* layout : layouts) {
			layout->AddDescriptorSet(&baseDescriptorSet_0);
		}
		// Add specific set 1 to specific layout lists
		for (auto* layout : pipelineLayouts["raster"]) {
			layout->AddDescriptorSet(&rasterDescriptorSet_1);
		}
		for (auto* layout : pipelineLayouts["rayTracing"]) {
			layout->AddDescriptorSet(&rayTracingDescriptorSet_1);
		}
		for (auto* layout : pipelineLayouts["thumbnail"]) {
			layout->AddDescriptorSet(&thumbnailDescriptorSet_1);
		}
		for (auto* layout : pipelineLayouts["ui"]) {
			layout->AddDescriptorSet(&uiDescriptorSet_1);
		}
		for (auto* layout : pipelineLayouts["postProcessing"]) {
			layout->AddDescriptorSet(&postProcessingDescriptorSet_1);
		}
		for (auto* layout : pipelineLayouts["histogram"]) {
			layout->AddDescriptorSet(&histogramDescriptorSet_1);
		}
		
		// Submodules
		for (auto* submodule : renderingSubmodules) {
			submodule->InitLayouts(descriptorSets, images, &rayTracingPipelineLayout);
		}
	}
	
	void ConfigureShaders() override {
		ConfigureRayTracingShaders();
		ConfigurePostProcessingShaders();
		ConfigureUiShaders();
		
		// Submodules
		for (auto* submodule : renderingSubmodules) {
			submodule->ConfigureShaders(rasterShaders, &shaderBindingTable);
		}
	}

public: // Scene configuration overrides
	void ReadShaders() override {
		// Rasterization
		for (auto&[t, shaderList] : rasterShaders) {
			for (auto* shader : shaderList) {
				shader->ReadShaders();
			}
		}
		
		// Ray Tracing
		shaderBindingTable.ReadShaders();
		
		// Compute
		for (auto* shader : computeShaders) {
			shader->ReadShaders();
		}
		
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
	}
	void UnloadScene() override {
		// Submodules
		for (auto* submodule : renderingSubmodules) {
			submodule->UnloadScene(scene);
		}
	}
	
private: // Resources overrides
	void CreateResources() override {
		CreateUiResources();
		CreateRenderingResources();
		CreatePostProcessingResources();
		CreateRayTracingResources();
		
		// Submodules
		for (auto* submodule : renderingSubmodules) {
			submodule->SetSwapChain(swapChain);
			submodule->CreateResources();
		}
	}
	void DestroyResources() override {
		DestroyUiResources();
		DestroyRenderingResources();
		DestroyPostProcessingResources();
		DestroyRayTracingResources();
		
		// Submodules
		for (auto* submodule : renderingSubmodules) {
			submodule->DestroyResources();
		}
	}
	void AllocateBuffers() override {
		AllocateSceneBuffers();
		AllocatePostProcessingBuffers();
		AllocateRayTracingBuffers();
		
		// Submodules
		for (auto* submodule : renderingSubmodules) {
			submodule->AllocateBuffers();
		}
		
		Geometry::globalBuffers.DefragmentMemory();
		Geometry::globalBuffers.Allocate(renderingDevice, {lowPriorityComputeQueue.familyIndex, graphicsQueue.familyIndex});
	}
	void FreeBuffers() override {
		FreeSceneBuffers();
		FreePostProcessingBuffers();
		FreeRayTracingBuffers();
		
		// Submodules
		for (auto* submodule : renderingSubmodules) {
			submodule->FreeBuffers();
		}
		
		Geometry::globalBuffers.Free(renderingDevice);
	}

private: // Graphics configuration overrides
	void CreatePipelines() override {
		// Pipeline layouts
		for (auto[name,layouts] : pipelineLayouts) for (auto* layout : layouts) {
			layout->Create(renderingDevice);
		}
		
		// Submodules
		for (auto* submodule : renderingSubmodules) {
			submodule->CreatePipelines(images);
		}
		
		// UI
		CreateUiPipeline();
		#ifdef _ENABLE_IMGUI
			LoadImGui();
		#endif
		
		// Ray Tracing
		CreateRayTracingPipeline();
		
		// Post Processing
		CreatePostProcessingPipeline();
	}
	void DestroyPipelines() override {
		// UI
		#ifdef _ENABLE_IMGUI
			UnloadImGui();
		#endif
		DestroyUiPipeline();
		
		// Ray Tracing
		DestroyRayTracingPipeline();
		
		// Post Processing
		DestroyPostProcessingPipeline();
		
		// Submodules
		for (auto* submodule : renderingSubmodules) {
			submodule->DestroyPipelines();
		}
		
		// Pipeline layouts
		for (auto[name,layouts] : pipelineLayouts) for (auto* layout : layouts) {
			layout->Destroy(renderingDevice);
		}
	}
	
public: // Update overrides
	void FrameUpdate(uint imageIndex) override {
		
		// Reset camera information
		scene.camera.width = swapChain->extent.width;
		scene.camera.height = swapChain->extent.height;
		scene.camera.RefreshProjectionMatrix();
		
		// Submodules
		for (auto* submodule : renderingSubmodules) {
			submodule->FrameUpdate(scene);
		}
		
		// Global Scratch Buffer
		size_t globalScratchBufferSize = topLevelAccelerationStructure.GetMemoryRequirementsForScratchBuffer(renderingDevice);
		
		ResetRayTracingBlasBuilds();
		scene.CollectGarbage();
		inactiveBlas.clear();
		
		// Update object transforms and light sources (Use all lights for now)
		scene.Lock();
			nbActiveLights = 0;
			for (auto* obj : scene.objectInstances) {
				if (obj) {
					// obj->Lock();
					if (obj->IsActive()) {
						if (!obj->IsGenerated()) obj->GenerateGeometries();
						// Matrices
						obj->WriteMatrices(scene.camera.viewMatrix);
						// Light sources
						for (auto* lightSource : obj->GetLightSources()) {
							activeLights[nbActiveLights++] = lightSource->lightOffset;
						}
						// Geometries
						for (auto& geom : obj->GetGeometries()) {
							if (geom.geometry->active) {
								if (!geom.geometry->blas) {
									MakeRayTracingBlas(&geom);
								}
								if (geom.geometry->blas && !geom.geometry->blas->built) {
									
									// Global Scratch Buffer
									if (AccelerationStructure::useGlobalScratchBuffer) {
										VkDeviceSize scratchSize = geom.geometry->blas->GetMemoryRequirementsForScratchBuffer(renderingDevice);
										if (!globalScratchDynamicSize && globalScratchBufferSize + scratchSize > globalScratchBuffer.size) {
											continue;
										}
										geom.geometry->blas->globalScratchBufferOffset = globalScratchBufferSize;
										geom.geometry->blas->SetGlobalScratchBuffer(renderingDevice, globalScratchBuffer.buffer);
										globalScratchBufferSize += scratchSize;
										if (globalScratchDynamicSize) blasBuildsForGlobalScratchBufferReallocation.push_back(geom.geometry->blas);
									}
									
									AddRayTracingBlasBuild(geom.geometry->blas);
								}
								if (geom.geometry->blas && geom.geometry->blas->built) {
									if (geom.rayTracingInstanceIndex == -1) {
										AddRayTracingInstance(&geom);
									}
									SetRayTracingInstanceTransform(&geom, obj->GetModelViewMatrix() * geom.transform);
								}
							} else if (geom.rayTracingInstanceIndex != -1) {
								RemoveRayTracingInstance(&geom);
							}
						}
					} else {
						for (auto& geom : obj->GetGeometries()) if (geom.rayTracingInstanceIndex != -1) {
							RemoveRayTracingInstance(&geom);
						}
					}
					// obj->Unlock();
				}
			}
		scene.Unlock();
		
		// Global Scratch Buffer
		if (AccelerationStructure::useGlobalScratchBuffer && globalScratchDynamicSize) {
			// If current scratch buffer size is too small or more than 4x the necessary size, reallocate it
			if (globalScratchBuffer.size < globalScratchBufferSize || globalScratchBuffer.size > globalScratchBufferSize*4) {
				globalScratchBuffer.Free(renderingDevice);
				globalScratchBuffer.size = globalScratchBufferSize;
				globalScratchBuffer.Allocate(renderingDevice, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, false);
				LOG("Reallocated global scratch buffer size " << globalScratchBufferSize)
				topLevelAccelerationStructure.SetGlobalScratchBuffer(renderingDevice, globalScratchBuffer.buffer);
				ResetRayTracingBlasBuilds();
				for (auto blas : blasBuildsForGlobalScratchBufferReallocation) {
					blas->SetGlobalScratchBuffer(renderingDevice, globalScratchBuffer.buffer);
					AddRayTracingBlasBuild(blas);
				}
			}
			blasBuildsForGlobalScratchBufferReallocation.clear();
		}
		
	}
	void LowPriorityFrameUpdate() override {
		PostProcessingLowPriorityUpdate();
		
		// Submodules
		for (auto* submodule : renderingSubmodules) {
			submodule->LowPriorityFrameUpdate();
		}
	}
	void BeforeGraphics() override {}
	
private: // Commands overrides
	void RunDynamicGraphics(VkCommandBuffer commandBuffer) override {
		
		// Transfer data to rendering device
		cameraUniformBuffer.Update(renderingDevice, commandBuffer);
		activeLightsUniformBuffer.Update(renderingDevice, commandBuffer, sizeof(uint32_t)/*lightCount*/ + sizeof(uint32_t)*nbActiveLights/*lightIndices*/);
		Geometry::globalBuffers.PushObjects(renderingDevice, commandBuffer);
		Geometry::globalBuffers.PushLights(renderingDevice, commandBuffer);
		for (auto[i,geometry] : activeGeometries) {
			if (geometry) geometry->AutoPush(renderingDevice, commandBuffer);
		}
		
		//TODO memory barrier between transfer and vertex shaders ?
	
		// // Submodules
		// for (auto* submodule : renderingSubmodules) {
		// 	submodule->RunDynamicGraphicsTop(commandBuffer, images);
		// }
	
		// Acceleration Structures
		BuildBottomLevelRayTracingAccelerationStructures(renderingDevice, commandBuffer);
		BuildTopLevelRayTracingAccelerationStructure(renderingDevice, commandBuffer);
		
		// // Submodules
		// for (auto* submodule : renderingSubmodules) {
		// 	submodule->RunDynamicGraphicsMiddle(commandBuffer, images);
		// }
		
		RecordRayTracingCommands(commandBuffer);
		
		// // Submodules
		// for (auto* submodule : renderingSubmodules) {
		// 	submodule->RunDynamicGraphicsBottom(commandBuffer, images);
		// }
	}
	void RecordGraphicsCommandBuffer(VkCommandBuffer commandBuffer, int imageIndex) override {
		RecordPostProcessingCommands(commandBuffer, imageIndex);
	}
	void RunDynamicLowPriorityCompute(VkCommandBuffer commandBuffer) override {
		RunDynamicPostProcessingLowPriorityCompute(commandBuffer);
		
		// Submodules
		for (auto* submodule : renderingSubmodules) {
			submodule->RunDynamicLowPriorityCompute(commandBuffer);
		}
	}
	void RunDynamicLowPriorityGraphics(VkCommandBuffer commandBuffer) override {
		// UI
		uiRenderPass.Begin(renderingDevice, commandBuffer, uiImage, {{.0,.0,.0,.0}});
			for (auto* shader : rasterShaders["ui"]) {
				shader->Execute(renderingDevice, commandBuffer);
			}
			#ifdef _ENABLE_IMGUI
				DrawImGui(commandBuffer);
			#endif
		uiRenderPass.End(renderingDevice, commandBuffer);
		
		// Submodules
		for (auto* submodule : renderingSubmodules) {
			submodule->RunDynamicLowPriorityGraphics(commandBuffer);
		}
	}
	
public: // custom functions

	#ifdef _ENABLE_IMGUI
		void RunImGui() {
			// Submodules
			ImGui::SetNextWindowSize({405, 285});
			ImGui::Begin("Settings and Modules");
			// ImGui::Checkbox("Debug G-Buffers", &scene.camera.debug);
			ImGui::Checkbox("TXAA", &scene.camera.txaa);
			ImGui::Checkbox("Gamma correction", &scene.camera.gammaCorrection);
			ImGui::Checkbox("HDR Tone Mapping", &scene.camera.hdr);
			ImGui::SliderFloat("HDR Exposure", &exposureFactor, 0, 8);
			for (auto* submodule : renderingSubmodules) {
				ImGui::Separator();
				submodule->RunImGui();
			}
			ImGui::End();
		}
	#endif
};
