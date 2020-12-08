#define _V4D_MODULE
#define V4D_HYBRID_RENDERER_MODULE

#include <v4d.h>
#include "Texture2D.hpp"
#include "RayCast.hh"
#include "camera_options.hh"

#include "../V4D_flycam/common.hh"

using namespace v4d::scene;
using namespace v4d::graphics;
using namespace v4d::graphics::vulkan;
using namespace v4d::graphics::vulkan::rtx;

#pragma region Limits
	const uint32_t MAX_RENDERABLE_ENTITY_INSTANCES = 65536; // 80 bytes each
#pragma endregion

// Application
Renderer* r = nullptr;
Scene* scene = nullptr;

RayCast currentRayCast {};

// Textures
Texture2D tex_img_font_atlas { V4D_MODULE_ASSET_PATH(THIS_MODULE, "resources/monospace_font_atlas.png"), STBI_grey_alpha};

VkPhysicalDeviceRayTracingPipelinePropertiesKHR rayTracingPipelineProperties {};
VkPhysicalDeviceAccelerationStructurePropertiesKHR accelerationStructureProperties {};
AccelerationStructure topLevelAccelerationStructure {};
Buffer rayTracingShaderBindingTableBuffer {VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT};
std::recursive_mutex rayTracingInstanceMutex, blasBuildQueueMutex;
std::vector<VkAccelerationStructureBuildGeometryInfoKHR> blasQueueBuildGeometryInfos {};
std::vector<VkAccelerationStructureBuildRangeInfoKHR*> blasQueueBuildRangeInfos {};

std::array<std::map<int32_t, std::shared_ptr<RenderableGeometryEntity>>, Renderer::NB_FRAMES_IN_FLIGHT> currentRenderableEntities {};

#pragma region Buffers
	StagingBuffer<Mesh::ModelInfo, MAX_RENDERABLE_ENTITY_INSTANCES, 1> renderableEntityInstanceBuffer {};
	StagingBuffer<Camera, 1> cameraUniformBuffer {VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT};
	StagingBuffer<RenderableGeometryEntity::LightSource, MAX_ACTIVE_LIGHTS> lightSourcesBuffer {};
	StagingBuffer<RayTracingBLASInstance, RAY_TRACING_TLAS_MAX_INSTANCES> rayTracingInstanceBuffer {VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR};
	uint32_t nbRayTracingInstances = 0;
	Buffer totalLuminance {VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, sizeof(glm::vec4)};
	Buffer raycastBuffer {VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, sizeof(RayCast)};
#pragma endregion

#pragma region Descriptor Sets
	DescriptorSet set0;
	DescriptorSet set1_raytracing;
	DescriptorSet set1_post;
	DescriptorSet set1_thumbnail;
	DescriptorSet set1_histogram;
	DescriptorSet set1_raycast;
	DescriptorSet set1_overlay;
#pragma endregion

#pragma region Images
	Image img_depth { VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT ,1,1, { VK_FORMAT_R32_SFLOAT } };
	Image img_lit { VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT ,1,1, { VK_FORMAT_R16G16B16A16_SFLOAT }};
	Image img_pp { VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT ,1,1, { VK_FORMAT_R16G16B16A16_SFLOAT }};
	Image img_history { VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT ,1,1, { VK_FORMAT_R16G16B16A16_SFLOAT }};
	Image img_thumbnail { VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT ,1,1, { VK_FORMAT_R16G16B16A16_SFLOAT }};
	Image img_overlay { VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT ,1,1, { VK_FORMAT_R8G8B8A8_UNORM }};
	
	// Textures
	Texture2D tex_metal_normal {V4D_MODULE_ASSET_PATH(THIS_MODULE, "resources/textures/metal_normal.tga"), STBI_rgb_alpha};
	std::array<Texture2D*, 1> textures {
		&tex_metal_normal,
	};

#pragma endregion

#pragma region Pipeline Layouts
	PipelineLayout pl_raytracing;
	PipelineLayout pl_overlay;
	PipelineLayout pl_post;
	PipelineLayout pl_thumbnail;
	PipelineLayout pl_histogram;
	PipelineLayout pl_raycast;
#pragma endregion

#pragma region Shaders

	// Ray Tracing
	ShaderBindingTable sbt_raytracing {pl_raytracing, V4D_MODULE_ASSET_PATH(THIS_MODULE, "shaders/raytracing.rgen")};

	// Overlay
	RasterShaderPipeline shader_overlay_circles {pl_overlay, {
		V4D_MODULE_ASSET_PATH(THIS_MODULE, "shaders/overlay_shapes.vert"),
		V4D_MODULE_ASSET_PATH(THIS_MODULE, "shaders/overlay_shapes.circle.frag"),
	}, -4};
	RasterShaderPipeline shader_overlay_squares {pl_overlay, {
		V4D_MODULE_ASSET_PATH(THIS_MODULE, "shaders/overlay_shapes.vert"),
		V4D_MODULE_ASSET_PATH(THIS_MODULE, "shaders/overlay_shapes.square.frag"),
	}, -3};
	RasterShaderPipeline shader_overlay_lines {pl_overlay, {
		V4D_MODULE_ASSET_PATH(THIS_MODULE, "shaders/overlay_lines.vert"),
		V4D_MODULE_ASSET_PATH(THIS_MODULE, "shaders/overlay_lines.frag"),
	}, -2};
	RasterShaderPipeline shader_overlay_text {pl_overlay, {
		V4D_MODULE_ASSET_PATH(THIS_MODULE, "shaders/overlay_text.vert"),
		V4D_MODULE_ASSET_PATH(THIS_MODULE, "shaders/overlay_text.frag"),
	}, -1};
	
	// Post Processing
	RasterShaderPipeline shader_thumbnail {pl_thumbnail, {
		V4D_MODULE_ASSET_PATH(THIS_MODULE, "shaders/v4d_thumbnail.vert"),
		V4D_MODULE_ASSET_PATH(THIS_MODULE, "shaders/v4d_thumbnail.frag"),
	}};
	RasterShaderPipeline shader_fx_txaa {pl_post, {
		V4D_MODULE_ASSET_PATH(THIS_MODULE, "shaders/v4d_post.vert"),
		V4D_MODULE_ASSET_PATH(THIS_MODULE, "shaders/v4d_post.txaa.frag"),
	}, 100};
	RasterShaderPipeline shader_history_write {pl_post, {
		V4D_MODULE_ASSET_PATH(THIS_MODULE, "shaders/v4d_post.vert"),
		V4D_MODULE_ASSET_PATH(THIS_MODULE, "shaders/v4d_post.history_write.frag"),
	}};
	RasterShaderPipeline shader_present_hdr {pl_post, {
		V4D_MODULE_ASSET_PATH(THIS_MODULE, "shaders/v4d_post.vert"),
		V4D_MODULE_ASSET_PATH(THIS_MODULE, "shaders/v4d_post.hdr.frag"),
	}, -100};
	RasterShaderPipeline shader_present_overlay_apply {pl_post, {
		V4D_MODULE_ASSET_PATH(THIS_MODULE, "shaders/v4d_post.vert"),
		V4D_MODULE_ASSET_PATH(THIS_MODULE, "shaders/v4d_post.overlay_apply.frag"),
	}, 100};
	ComputeShaderPipeline shader_histogram_compute {pl_histogram, 
		V4D_MODULE_ASSET_PATH(THIS_MODULE, "shaders/v4d_histogram.comp")
	};
	ComputeShaderPipeline shader_raycast_compute {pl_raycast, 
		V4D_MODULE_ASSET_PATH(THIS_MODULE, "shaders/v4d_raycast.comp")
	};
	
#pragma endregion

#pragma region Containers
	// Pipelines
	std::unordered_map<std::string, Image*> images {
		{"img_depth", &img_depth},
		{"img_lit", &img_lit},
		{"img_pp", &img_pp},
		{"img_history", &img_history},
		{"img_thumbnail", &img_thumbnail},
		{"img_overlay", &img_overlay},
	};
	std::unordered_map<std::string, PipelineLayout*> pipelineLayouts {
		{"pl_raytracing", &pl_raytracing},
		{"pl_overlay", &pl_overlay},
		{"pl_post", &pl_post},
		{"pl_thumbnail", &pl_thumbnail},
		{"pl_histogram", &pl_histogram},
		{"pl_raycast", &pl_raycast},
	};
	std::unordered_map<std::string, std::vector<RasterShaderPipeline*>> shaderGroups {
		{"sg_thumbnail", {&shader_thumbnail}},
		{"sg_postfx", {&shader_fx_txaa}},
		{"sg_history_write", {&shader_history_write}},
		{"sg_present", {&shader_present_hdr, &shader_present_overlay_apply}},
		{"sg_overlay", {&shader_overlay_lines, &shader_overlay_text, &shader_overlay_squares, &shader_overlay_circles}},
	};
	std::unordered_map<std::string, ShaderBindingTable*> shaderBindingTables {
		{"sbt_raytracing", &sbt_raytracing},
	};
	// FrameBuffers
	std::unordered_map<std::string, std::vector<VkSemaphore>> semaphores {};
	std::unordered_map<std::string, std::vector<VkFence>> fences {};
	std::unordered_map<std::string, std::vector<VkCommandBuffer>> commandBuffers {};
#pragma endregion

#pragma region Render Passes (Rasterization)
	RenderPass postProcessingRenderPass;
	RenderPass thumbnailRenderPass;
	RenderPass uiRenderPass;
#pragma endregion

#pragma region Rendering Resources

	void CreateRenderingResources() {
		uint rasterWidth = (uint)((float)r->swapChain->extent.width);
		uint rasterHeight = (uint)((float)r->swapChain->extent.height);
		
		img_lit.Create(r->renderingDevice, rasterWidth, rasterHeight);
		img_depth.Create(r->renderingDevice, rasterWidth, rasterHeight);
		
		auto commandBuffer = r->BeginSingleTimeCommands(r->renderingDevice->GetQueue("graphics"));
			r->TransitionImageLayout(commandBuffer, img_lit, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
			r->TransitionImageLayout(commandBuffer, img_depth, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
		r->EndSingleTimeCommands(r->renderingDevice->GetQueue("graphics"), commandBuffer);
	}
	void DestroyRenderingResources() {
		img_lit.Destroy(r->renderingDevice);
		img_depth.Destroy(r->renderingDevice);
	}
	
	// void ClearLitImage(VkCommandBuffer commandBuffer) {
	// 	r->TransitionImageLayout(commandBuffer, img_lit, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
	// 		const VkClearColorValue clearValues = {0,0,0,0};
	// 		VkImageSubresourceRange range {VK_IMAGE_ASPECT_COLOR_BIT, 0,1,0,1};
	// 		r->renderingDevice->CmdClearColorImage(commandBuffer, img_lit.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearValues, 1, &range);
	// 	r->TransitionImageLayout(commandBuffer, img_lit, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL);
	// }
	
#pragma endregion

#pragma region Ray-Tracing Pipelines & Resouces

	void CreateRayTracingResources() {
		topLevelAccelerationStructure.AssignTopLevel();
		topLevelAccelerationStructure.CreateAndAllocate(r->renderingDevice, true);
		topLevelAccelerationStructure.SetInstanceBuffer(r->renderingDevice, rayTracingInstanceBuffer.GetDeviceLocalBuffer());
	}
	void DestroyRayTracingResources() {
		topLevelAccelerationStructure.FreeAndDestroy(r->renderingDevice);
	}
	
	void AllocateRayTracingBuffers() {
		rayTracingInstanceBuffer.Allocate(r->renderingDevice);
	}
	void FreeRayTracingBuffers() {
		nbRayTracingInstances = 0;
		rayTracingInstanceBuffer.Free();
	}
	
	void CreateRayTracingPipeline() {
		sbt_raytracing.CreateRayTracingPipeline(r->renderingDevice);
		rayTracingShaderBindingTableBuffer.size = sbt_raytracing.GetSbtBufferSize(rayTracingPipelineProperties);
		rayTracingShaderBindingTableBuffer.Allocate(r->renderingDevice, MEMORY_USAGE_CPU_TO_GPU);
		sbt_raytracing.WriteShaderBindingTableToBuffer(r->renderingDevice, &rayTracingShaderBindingTableBuffer, 0, rayTracingPipelineProperties);
	}
	void DestroyRayTracingPipeline() {
		rayTracingShaderBindingTableBuffer.Free(r->renderingDevice);
		sbt_raytracing.DestroyRayTracingPipeline(r->renderingDevice);
	}
	
	void ConfigureRayTracingShaders() {
		sbt_raytracing.AddMissShader(V4D_MODULE_ASSET_PATH(THIS_MODULE, "shaders/raytracing.rmiss"));
		sbt_raytracing.AddMissShader(V4D_MODULE_ASSET_PATH(THIS_MODULE, "shaders/raytracing.shadow.rmiss"));
		RenderableGeometryEntity::sbtOffsets["default"] = sbt_raytracing.AddHitShader(V4D_MODULE_ASSET_PATH(THIS_MODULE, "shaders/default.rchit"));
		RenderableGeometryEntity::sbtOffsets["aabb_cube"] = sbt_raytracing.AddHitShader(V4D_MODULE_ASSET_PATH(THIS_MODULE, "shaders/aabb_cube.rchit"), "", V4D_MODULE_ASSET_PATH(THIS_MODULE, "shaders/aabb_cube.rint"));
		RenderableGeometryEntity::sbtOffsets["aabb_sphere"] = sbt_raytracing.AddHitShader(V4D_MODULE_ASSET_PATH(THIS_MODULE, "shaders/aabb_sphere.rchit"), "", V4D_MODULE_ASSET_PATH(THIS_MODULE, "shaders/aabb_sphere.rint"));
		RenderableGeometryEntity::sbtOffsets["aabb_sphere.light"] = sbt_raytracing.AddHitShader(V4D_MODULE_ASSET_PATH(THIS_MODULE, "shaders/aabb_sphere.light.rchit"), "", V4D_MODULE_ASSET_PATH(THIS_MODULE, "shaders/aabb_sphere.rint"));
	}
	
	void RunRayTracingCommands(VkCommandBuffer commandBuffer) {
		
		int width = (int)((float)r->swapChain->extent.width);
		int height = (int)((float)r->swapChain->extent.height);
		
		r->renderingDevice->CmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, sbt_raytracing.GetPipeline());
		sbt_raytracing.GetPipelineLayout()->Bind(r->renderingDevice, commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR);
		r->renderingDevice->CmdTraceRaysKHR(
			commandBuffer, 
			sbt_raytracing.GetRayGenDeviceAddressRegion(),
			sbt_raytracing.GetRayMissDeviceAddressRegion(),
			sbt_raytracing.GetRayHitDeviceAddressRegion(),
			sbt_raytracing.GetRayCallableDeviceAddressRegion(),
			width, height, 1
		);
	}
	
	void BuildBottomLevelRayTracingAccelerationStructures(Device* device, VkCommandBuffer commandBuffer) {
		// Build all new/updated bottom levels
		std::lock_guard lock(blasBuildQueueMutex);
		if (blasQueueBuildGeometryInfos.size() > 0) {
			device->CmdBuildAccelerationStructuresKHR(commandBuffer, blasQueueBuildGeometryInfos.size(), blasQueueBuildGeometryInfos.data(), blasQueueBuildRangeInfos.data());
		}
	}
	
	void BuildTopLevelRayTracingAccelerationStructure(Device* device, VkCommandBuffer commandBuffer) {
		if (!device->TouchAllocation(topLevelAccelerationStructure.accelerationStructureAllocation)) {
			LOG_DEBUG("Top Level acceleration structure ALLOCATION LOST")
			return;
		}
		std::lock_guard lock(rayTracingInstanceMutex);
		static const VkAccelerationStructureBuildRangeInfoKHR* topLevelAccelerationStructureGeometriesOffsets = &topLevelAccelerationStructure.buildRangeInfo;
		topLevelAccelerationStructure.SetInstanceCount(nbRayTracingInstances);
		device->CmdBuildAccelerationStructuresKHR(commandBuffer, 1, &topLevelAccelerationStructure.buildGeometryInfo, &topLevelAccelerationStructureGeometriesOffsets);
	}
	
#pragma endregion

#pragma region Post Processing
	
	float thumbnailScale = 1.0/16;
	float exposureFactor = 1.0;
	
	void CreatePostProcessingResources() {
		uint rasterWidth = (uint)((float)r->swapChain->extent.width);
		uint rasterHeight = (uint)((float)r->swapChain->extent.height);
		uint thumbnailWidth = (uint)((float)rasterWidth * thumbnailScale);
		uint thumbnailHeight = (uint)((float)rasterHeight * thumbnailScale);
		
		img_pp.Create(r->renderingDevice, rasterWidth, rasterHeight);
		img_history.Create(r->renderingDevice, rasterWidth, rasterHeight);
		img_thumbnail.Create(r->renderingDevice, thumbnailWidth, thumbnailHeight, {img_lit.format});
		
		auto commandBuffer = r->BeginSingleTimeCommands(r->renderingDevice->GetQueue("graphics"));
			r->TransitionImageLayout(commandBuffer, img_history, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
			r->TransitionImageLayout(commandBuffer, img_thumbnail, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
		r->EndSingleTimeCommands(r->renderingDevice->GetQueue("graphics"), commandBuffer);
	}
	void DestroyPostProcessingResources() {
		img_pp.Destroy(r->renderingDevice);
		img_history.Destroy(r->renderingDevice);
		img_thumbnail.Destroy(r->renderingDevice);
	}
	
	void CreatePostProcessingPipeline() {
		{// Thumbnail Gen render pass
			VkAttachmentDescription colorAttachment = {};
				colorAttachment.format = img_thumbnail.format;
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
			thumbnailRenderPass.Create(r->renderingDevice);
			thumbnailRenderPass.CreateFrameBuffers(r->renderingDevice, img_thumbnail);
			
			// Shaders
			for (auto* shaderPipeline : shaderGroups["sg_thumbnail"]) {
				shaderPipeline->SetRenderPass(&img_thumbnail, thumbnailRenderPass.handle, 0);
				shaderPipeline->AddColorBlendAttachmentState();
				shaderPipeline->CreatePipeline(r->renderingDevice);
			}
		}
		
		{// Post Processing render pass
			std::array<VkAttachmentDescription, 3> attachments {};
			// std::array<VkAttachmentReference, 0> inputAttachmentRefs {};
			
			// PP image
			attachments[0].format = img_pp.format;
			attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
			attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			attachments[0].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			uint32_t ppAttachmentIndex = postProcessingRenderPass.AddAttachment(attachments[0]);
			
			// History image
			attachments[1].format = img_history.format;
			attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
			attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			attachments[1].initialLayout = VK_IMAGE_LAYOUT_GENERAL;
			attachments[1].finalLayout = VK_IMAGE_LAYOUT_GENERAL;
			uint32_t historyAttachmentIndex = postProcessingRenderPass.AddAttachment(attachments[1]);
			
			// SwapChain image
			attachments[2].format = r->swapChain->format.format;
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
				},
			};
			postProcessingRenderPass.renderPassInfo.dependencyCount = subPassDependencies.size();
			postProcessingRenderPass.renderPassInfo.pDependencies = subPassDependencies.data();
			
			// Create the render pass
			postProcessingRenderPass.Create(r->renderingDevice);
			postProcessingRenderPass.CreateFrameBuffers(r->renderingDevice, r->swapChain, {
				img_pp.view, 
				img_history.view, 
				VK_NULL_HANDLE,
			});
			
			// Shaders
			for (auto* shader : shaderGroups["sg_postfx"]) {
				shader->SetRenderPass(r->swapChain, postProcessingRenderPass.handle, 0);
				shader->AddColorBlendAttachmentState();
				shader->CreatePipeline(r->renderingDevice);
			}
			for (auto* shader : shaderGroups["sg_history_write"]) {
				shader->SetRenderPass(r->swapChain, postProcessingRenderPass.handle, 1);
				shader->AddColorBlendAttachmentState(VK_FALSE);
				shader->CreatePipeline(r->renderingDevice);
			}
			for (auto* shader : shaderGroups["sg_present"]) {
				shader->SetRenderPass(r->swapChain, postProcessingRenderPass.handle, 2);
				shader->AddColorBlendAttachmentState();
				shader->CreatePipeline(r->renderingDevice);
			}
		}
		
		// Compute
		shader_histogram_compute.CreatePipeline(r->renderingDevice);
		shader_raycast_compute.CreatePipeline(r->renderingDevice);
	}
	void DestroyPostProcessingPipeline() {
		// Thumbnail Gen
		for (auto* shaderPipeline : shaderGroups["sg_thumbnail"]) {
			shaderPipeline->DestroyPipeline(r->renderingDevice);
		}
		thumbnailRenderPass.DestroyFrameBuffers(r->renderingDevice);
		thumbnailRenderPass.Destroy(r->renderingDevice);
		
		// Post-processing
		for (auto* s : shaderGroups["sg_postfx"]) {
			s->DestroyPipeline(r->renderingDevice);
		}
		for (auto* s : shaderGroups["sg_history_write"]) {
			s->DestroyPipeline(r->renderingDevice);
		}
		for (auto* s : shaderGroups["sg_present"]) {
			s->DestroyPipeline(r->renderingDevice);
		}
		postProcessingRenderPass.DestroyFrameBuffers(r->renderingDevice);
		postProcessingRenderPass.Destroy(r->renderingDevice);
		
		// Compute
		shader_histogram_compute.DestroyPipeline(r->renderingDevice);
		shader_raycast_compute.DestroyPipeline(r->renderingDevice);
	}
	
	void ConfigurePostProcessingShaders() {
		// Thumbnail Gen
		shader_thumbnail.inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
		shader_thumbnail.depthStencilState.depthTestEnable = VK_FALSE;
		shader_thumbnail.depthStencilState.depthWriteEnable = VK_FALSE;
		shader_thumbnail.rasterizer.cullMode = VK_CULL_MODE_NONE;
		shader_thumbnail.SetData(3);
		
		// Post-Processing
		for (auto[rp, ss] : shaderGroups) if (rp == "sg_postfx" || rp == "sg_history_write" || rp == "sg_present") {
			for (auto* s : ss) {
				s->inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
				s->depthStencilState.depthTestEnable = VK_FALSE;
				s->depthStencilState.depthWriteEnable = VK_FALSE;
				s->rasterizer.cullMode = VK_CULL_MODE_NONE;
				s->SetData(3);
			}
		}
	}
	
	void AllocateComputeBuffers() {
		// histogram
		totalLuminance.Allocate(r->renderingDevice, MEMORY_USAGE_CPU_TO_GPU, false);
		totalLuminance.MapMemory(r->renderingDevice);
		glm::vec4 luminance {0,0,0,1};
		totalLuminance.WriteToMappedData(&luminance);
		// raycast
		raycastBuffer.Allocate(r->renderingDevice, MEMORY_USAGE_CPU_TO_GPU, false);
		raycastBuffer.MapMemory(r->renderingDevice);
		currentRayCast = {};
		raycastBuffer.WriteToMappedData(&currentRayCast);
	}
	void FreeComputeBuffers() {
		// histogram
		totalLuminance.UnmapMemory(r->renderingDevice);
		totalLuminance.Free(r->renderingDevice);
		// raycast
		currentRayCast = {};
		raycastBuffer.UnmapMemory(r->renderingDevice);
		raycastBuffer.Free(r->renderingDevice);
	}
	
	void RecordPostProcessingCommands(VkCommandBuffer commandBuffer, int imageIndex) {
		// Gen Thumbnail
		thumbnailRenderPass.Begin(r->renderingDevice, commandBuffer, img_thumbnail, {{.0,.0,.0,.0}});
			for (auto* shader : shaderGroups["sg_thumbnail"]) {
				shader->Execute(r->renderingDevice, commandBuffer);
			}
		thumbnailRenderPass.End(r->renderingDevice, commandBuffer);
		
		// Post Processing
		postProcessingRenderPass.Begin(r->renderingDevice, commandBuffer, r->swapChain, {{.0,.0,.0,.0}, {.0,.0,.0,.0}, {.0,.0,.0,.0}}, imageIndex);
			for (auto* shader : shaderGroups["sg_postfx"]) {
				shader->Execute(r->renderingDevice, commandBuffer);
			}
			r->renderingDevice->CmdNextSubpass(commandBuffer, VK_SUBPASS_CONTENTS_INLINE);
			for (auto* shader : shaderGroups["sg_history_write"]) {
				shader->Execute(r->renderingDevice, commandBuffer);
			}
			r->renderingDevice->CmdNextSubpass(commandBuffer, VK_SUBPASS_CONTENTS_INLINE);
			for (auto* shader : shaderGroups["sg_present"]) {
				shader->Execute(r->renderingDevice, commandBuffer);
			}
		postProcessingRenderPass.End(r->renderingDevice, commandBuffer);
	}
	void RunDynamicPostProcessingCompute(VkCommandBuffer commandBuffer) {
		// histogram
		shader_histogram_compute.SetGroupCounts(1, 1, 1);
		shader_histogram_compute.Execute(r->renderingDevice, commandBuffer);
	}
	void PostProcessingUpdate2() {
		// Histogram
		glm::vec4 luminance;
		totalLuminance.ReadFromMappedData(&luminance);
		if (luminance.a > 0) {
			scene->camera.luminance = glm::vec4(glm::vec3(luminance) / luminance.a, exposureFactor);
		}
	}
	void RunTXAA() {
		// TXAA
		if (RENDER_OPTIONS::TXAA && !DEBUG_OPTIONS::PHYSICS) {
			static unsigned long frameCount = 0;
			static const glm::dvec2 samples8[8] = {
				glm::dvec2(-7.0, 1.0) / 8.0,
				glm::dvec2(-5.0, -5.0) / 8.0,
				glm::dvec2(-1.0, -3.0) / 8.0,
				glm::dvec2(3.0, -7.0) / 8.0,
				glm::dvec2(5.0, -1.0) / 8.0,
				glm::dvec2(7.0, 7.0) / 8.0,
				glm::dvec2(1.0, 3.0) / 8.0,
				glm::dvec2(-3.0, 5.0) / 8.0
			};
			glm::dvec2 texelSize = 1.0 / glm::dvec2(scene->camera.width, scene->camera.height);
			glm::dvec2 subSample = samples8[frameCount % 8] * texelSize * double(scene->camera.txaaKernelSize);
			scene->camera.projectionMatrix[2].x = subSample.x;
			scene->camera.projectionMatrix[2].y = subSample.y;
			// historyTxaaOffset = txaaOffset;
			scene->camera.txaaOffset = subSample / 2.0;
			frameCount++;
			
			scene->camera.reprojectionMatrix = (scene->camera.projectionMatrix * scene->camera.historyViewMatrix) * glm::inverse(scene->camera.projectionMatrix * scene->camera.viewMatrix);
			
			// Save Projection and View matrices from previous frame
			scene->camera.historyViewMatrix = scene->camera.viewMatrix;
		}
	}
	
#pragma endregion

#pragma region UI
	float img_overlayScale = 1.0;
	
	static const int MAX_OVERLAY_LINES = 4000000;
	static const int MAX_OVERLAY_TEXT_CHARS = 32768;
	static const int MAX_OVERLAY_CIRCLES = 4096;
	static const int MAX_OVERLAY_SQUARES = 4096;
	std::mutex overlayMutex;
	// Overlay Lines
	struct OverlayLine {
		float x;
		float y;
		float color;
		float width;
	};
	Buffer overlayLinesBuffer {VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, sizeof(OverlayLine) * MAX_OVERLAY_LINES*2};
	OverlayLine* overlayLines = nullptr;
	size_t overlayLinesCount = 0;
	void AddOverlayLine(float x1, float y1, float x2, float y2, glm::vec4 color = {1,1,1,1}, float lineWidth = 1) {
		std::lock_guard lock(overlayMutex);
		if (overlayLinesCount+2 > MAX_OVERLAY_LINES) {LOG_WARN("Overlay Line buffer overlow") overlayLinesCount=0;return;}
		auto floatColor = PackColorAsFloat(color);
		overlayLines[overlayLinesCount++] = {x1, y1, floatColor, lineWidth};
		overlayLines[overlayLinesCount++] = {x2, y2, floatColor, lineWidth};
	}
	// Overlay Text
	struct OverlayText {
		float x;
		float y;
		float color;
		float c;
	};
	Buffer overlayTextBuffer {VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, sizeof(OverlayText) * MAX_OVERLAY_TEXT_CHARS};
	OverlayText* overlayText = nullptr;
	size_t overlayTextSize = 0;
	void AddOverlayText(char c, float x, float y, glm::vec4 color = {1,1,1,1}, uint16_t size = 20, uint8_t flags = 0) {
		std::lock_guard lock(overlayMutex);
		if (overlayTextSize+1 > MAX_OVERLAY_TEXT_CHARS) {LOG_WARN("Overlay Text buffer overlow") overlayTextSize=0;return;}
		overlayText[overlayTextSize++] = {x, y, PackColorAsFloat(color), glm::uintBitsToFloat((size << 16) | (c << 8) | flags)};
	}
	void AddOverlayText(const std::string& text, float x, float y, glm::vec4 color = {1,1,1,1}, uint16_t size = 20, uint8_t flags = 0, float letterSpacing = 0, float fontSizeRatio = 0.5) {
		const float letterWidth = (float(size)*fontSizeRatio+letterSpacing) / scene->camera.width * 2;
		x -= letterWidth * float(text.length()-1) / 2;
		for (int i = 0; i < text.length(); ++i) {
			AddOverlayText(text[i], x, y, color, size, flags);
			x += letterWidth;
		}
	}
	// Overlay Circles
	struct OverlayCircle {
		float x;
		float y;
		float color;
		float size;
	};
	Buffer overlayCirclesBuffer {VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, sizeof(OverlayCircle) * MAX_OVERLAY_CIRCLES};
	OverlayCircle* overlayCircles = nullptr;
	size_t overlayCirclesCount = 0;
	void AddOverlayCircle(float x, float y, glm::vec4 color = {1,1,1,1}, uint16_t size = 20, uint8_t border = 0) {
		std::lock_guard lock(overlayMutex);
		if (overlayCirclesCount+1 > MAX_OVERLAY_CIRCLES) {LOG_WARN("Overlay Circles buffer overlow") overlayCirclesCount=0;return;}
		overlayCircles[overlayCirclesCount++] = {x, y, PackColorAsFloat(color), glm::uintBitsToFloat((size << 20) | (size << 8) | border)};
	}
	// Overlay Squares
	struct OverlaySquare {
		float x;
		float y;
		float color;
		float size;
	};
	Buffer overlaySquaresBuffer {VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, sizeof(OverlaySquare) * MAX_OVERLAY_SQUARES};
	OverlaySquare* overlaySquares = nullptr;
	size_t overlaySquaresCount = 0;
	void AddOverlaySquare(float x, float y, glm::vec4 color = {1,1,1,1}, glm::mediump_u16vec2 size = {20,20}, uint8_t border = 0) {
		std::lock_guard lock(overlayMutex);
		if (overlaySquaresCount+1 > MAX_OVERLAY_SQUARES) {LOG_WARN("Overlay Squares buffer overlow") overlaySquaresCount=0;return;}
		overlaySquares[overlaySquaresCount++] = {x, y, PackColorAsFloat(color), glm::uintBitsToFloat((size.x << 20) | (size.y << 8) | border)};
	}
	
	void CreateUiResources() {
		img_overlay.Create(r->renderingDevice, 
			(uint)((float)r->swapChain->extent.width * img_overlayScale), 
			(uint)((float)r->swapChain->extent.height * img_overlayScale)
		);
		auto commandBuffer = r->BeginSingleTimeCommands(r->renderingDevice->GetQueue("graphics"));
			r->TransitionImageLayout(commandBuffer, img_overlay, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
		r->EndSingleTimeCommands(r->renderingDevice->GetQueue("graphics"), commandBuffer);
	}
	void DestroyUiResources() {
		img_overlay.Destroy(r->renderingDevice);
	}
	
	void CreateUiPipeline() {
		// Color Attachment (Fragment shader Standard Output)
		VkAttachmentDescription colorAttachment = {};
			colorAttachment.format = img_overlay.format;
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
		uiRenderPass.Create(r->renderingDevice);
		uiRenderPass.CreateFrameBuffers(r->renderingDevice, img_overlay);
		
		// Shader pipeline
		for (auto* shader : shaderGroups["sg_overlay"]) {
			shader->SetRenderPass(&img_overlay, uiRenderPass.handle, 0);
			shader->AddColorBlendAttachmentState();
			shader->CreatePipeline(r->renderingDevice);
		}
	}
	void DestroyUiPipeline() {
		for (auto* shader : shaderGroups["sg_overlay"]) {
			shader->DestroyPipeline(r->renderingDevice);
		}
		uiRenderPass.DestroyFrameBuffers(r->renderingDevice);
		uiRenderPass.Destroy(r->renderingDevice);
	}
	
	void ConfigureOverlayShaders() {
		// Lines
		shader_overlay_lines.AddVertexInputBinding(16, VK_VERTEX_INPUT_RATE_VERTEX, {{0, 0, VK_FORMAT_R32G32B32A32_SFLOAT}});
		shader_overlay_lines.inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
		shader_overlay_lines.rasterizer.cullMode = VK_CULL_MODE_NONE;
		shader_overlay_lines.depthStencilState.depthTestEnable = false;
		shader_overlay_lines.depthStencilState.depthWriteEnable = false;
		shader_overlay_lines.dynamicStates.push_back(VK_DYNAMIC_STATE_LINE_WIDTH);
		shader_overlay_lines.SetData(&overlayLinesBuffer, 0);
		
		// Text
		shader_overlay_text.AddVertexInputBinding(16, VK_VERTEX_INPUT_RATE_VERTEX, {{0, 0, VK_FORMAT_R32G32B32A32_SFLOAT}});
		shader_overlay_text.inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
		shader_overlay_text.rasterizer.cullMode = VK_CULL_MODE_NONE;
		shader_overlay_text.depthStencilState.depthTestEnable = false;
		shader_overlay_text.depthStencilState.depthWriteEnable = false;
		shader_overlay_text.SetData(&overlayTextBuffer, 0);
		
		// Circles
		shader_overlay_circles.AddVertexInputBinding(16, VK_VERTEX_INPUT_RATE_VERTEX, {{0, 0, VK_FORMAT_R32G32B32A32_SFLOAT}});
		shader_overlay_circles.inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
		shader_overlay_circles.rasterizer.cullMode = VK_CULL_MODE_NONE;
		shader_overlay_circles.depthStencilState.depthTestEnable = false;
		shader_overlay_circles.depthStencilState.depthWriteEnable = false;
		shader_overlay_circles.SetData(&overlayCirclesBuffer, 0);
		
		// Squares
		shader_overlay_squares.AddVertexInputBinding(16, VK_VERTEX_INPUT_RATE_VERTEX, {{0, 0, VK_FORMAT_R32G32B32A32_SFLOAT}});
		shader_overlay_squares.inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
		shader_overlay_squares.rasterizer.cullMode = VK_CULL_MODE_NONE;
		shader_overlay_squares.depthStencilState.depthTestEnable = false;
		shader_overlay_squares.depthStencilState.depthWriteEnable = false;
		shader_overlay_squares.SetData(&overlaySquaresBuffer, 0);
	}
	
	#ifdef _ENABLE_IMGUI
		void LoadImGui() {
			static ImGui_ImplVulkan_InitInfo init_info {};
				init_info.Instance = r->GetHandle();
				init_info.PhysicalDevice = r->renderingDevice->GetPhysicalDevice()->GetHandle();
				init_info.Device = r->renderingDevice->GetHandle();
				init_info.QueueFamily = r->renderingDevice->GetQueue("graphics").familyIndex;
				init_info.Queue = r->renderingDevice->GetQueue("graphics").handle;
				init_info.DescriptorPool = r->descriptorPool;
				init_info.MinImageCount = r->swapChain->images.size();
				init_info.ImageCount = r->swapChain->images.size();
				init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
				init_info.CheckVkResultFn = [](VkResult err){if (err) LOG_ERROR("ImGui Vk Error " << (int)err)};
			ImGui_ImplVulkan_Init(&init_info, uiRenderPass.handle);
			// Clear any existing draw data
			auto* drawData = ImGui::GetDrawData();
			if (drawData) drawData->Clear();
			// Font Upload
			auto cmdBuffer = r->BeginSingleTimeCommands(r->renderingDevice->GetQueue("graphics"));
				ImGui_ImplVulkan_CreateFontsTexture(cmdBuffer);
			r->EndSingleTimeCommands(r->renderingDevice->GetQueue("graphics"), cmdBuffer);
			ImGui_ImplVulkan_DestroyFontUploadObjects();
		}
		void UnloadImGui() {
			// Clear any existing draw data
			auto* drawData = ImGui::GetDrawData();
			if (drawData) drawData->Clear();
			// Shutdown ImGui
			ImGui_ImplVulkan_Shutdown();
		}
		void DrawImGui(VkCommandBuffer commandBuffer) {
			auto* drawData = ImGui::GetDrawData();
			if (drawData) {
				ImGui_ImplVulkan_RenderDrawData(drawData, commandBuffer);
			}
		}
	#endif


#pragma endregion

#pragma region Frame Update & Graphics commands

	void FrameUpdate(uint imageIndex) {
		
		renderableEntityInstanceBuffer.SetCurrentFrame(r->currentFrameInFlight);
		cameraUniformBuffer.SetCurrentFrame(r->currentFrameInFlight);
		lightSourcesBuffer.SetCurrentFrame(r->currentFrameInFlight);
		rayTracingInstanceBuffer.SetCurrentFrame(r->currentFrameInFlight);
		
		// {// Handle RayCast from previous frame here before reassigning anything in the geometry buffers
		// 	currentRayCast = *((RayCast*)raycastBuffer.data);
		// 	if (currentRayCast.hit) {
		// 		auto objData = ((Geometry::ObjectBuffer_T*)(Geometry::globalBuffers.objectBuffer.stagingBuffer.data))[currentRayCast.objectBufferOffset];
		// 		if (objData.moduleVen != 0 && objData.moduleId != 0) {
		// 			auto mod = V4D_Mod::LoadModule(v4d::modular::ModuleID(objData.moduleVen, objData.moduleId));
		// 			if (mod && mod->OnRendererRayCastHit) {
		// 				RenderRayCastHit hit;
		// 					hit.position = glm::inverse(objData.modelTransform) * glm::inverse(scene->camera.viewMatrix) * glm::dvec4(currentRayCast.position, 1);
		// 					hit.distance = currentRayCast.distance;
		// 					hit.objId = objData.objId;
		// 					hit.flags = currentRayCast.flags;
		// 					hit.customType = currentRayCast.customType;
		// 					hit.customData0 = currentRayCast.customData0;
		// 					hit.customData1 = currentRayCast.customData1;
		// 					hit.customData2 = currentRayCast.customData2;
		// 				mod->OnRendererRayCastHit(hit);
		// 			}
		// 		}
		// 	}
		// }
		
		{// Reset camera information
			scene->camera.width = r->swapChain->extent.width;
			scene->camera.height = r->swapChain->extent.height;
			scene->camera.RefreshProjectionMatrix();
			scene->camera.time = float(v4d::Timer::GetCurrentTimestamp() - 1587838909.0);
		}
		
		RunTXAA();
		
		// Modules
		V4D_Mod::ForEachSortedModule([](auto* mod){
			if (mod->BeginFrameUpdate) mod->BeginFrameUpdate(); // camera's View Matrix is created here in FlyCam Module
		});
		
		
		// Ray Tracing
		
		std::scoped_lock lock(blasBuildQueueMutex, rayTracingInstanceMutex);

		blasQueueBuildGeometryInfos.clear();
		blasQueueBuildRangeInfos.clear();
		
		nbRayTracingInstances = 0;
		
		int nbActiveLights = 0;
		
		// currentRenderableEntities[r->currentFrameInFlight].clear();
		
		RenderableGeometryEntity::ForEach([&nbActiveLights](auto entity){
			if (entity->GetIndex() == -1) return;
			
			if (!entity->generated) {
				renderableEntityInstanceBuffer[entity->GetIndex()].moduleVen = entity->moduleId.vendor;
				renderableEntityInstanceBuffer[entity->GetIndex()].moduleId = entity->moduleId.module;
				renderableEntityInstanceBuffer[entity->GetIndex()].objId = entity->objId;
				renderableEntityInstanceBuffer[entity->GetIndex()].customData = entity->customData;
				// Generate/Load
				entity->generator(entity.get());
				entity->generated = true;
				
				// Indices
				if (auto indexData = entity->meshIndices.Lock(); indexData) {
					entity->geometryData.indexBuffer = r->renderingDevice->GetBufferDeviceOrHostAddressConst(indexData->deviceBuffer);
					entity->geometryData.indexOffset = 0;
					entity->geometryData.indexCount = indexData->count;
					entity->geometryData.indexSize = sizeof(Mesh::Index);

					renderableEntityInstanceBuffer[entity->GetIndex()].indices = entity->geometryData.indexBuffer.deviceAddress;
				}
				// Vertex Positions
				if (auto vertexData = entity->meshVertexPosition.Lock(); vertexData) {
					entity->geometryData.vertexBuffer = r->renderingDevice->GetBufferDeviceOrHostAddressConst(vertexData->deviceBuffer);
					entity->geometryData.vertexOffset = 0;
					entity->geometryData.vertexCount = vertexData->count;
					entity->geometryData.vertexSize = sizeof(Mesh::VertexPosition);
					
					renderableEntityInstanceBuffer[entity->GetIndex()].vertexPositions = entity->geometryData.vertexBuffer.deviceAddress;
				} else if (auto proceduralVertexData = entity->proceduralVertexAABB.Lock(); proceduralVertexData) {
					entity->geometryData.vertexBuffer = r->renderingDevice->GetBufferDeviceOrHostAddressConst(proceduralVertexData->deviceBuffer);
					entity->geometryData.vertexOffset = 0;
					entity->geometryData.vertexCount = proceduralVertexData->count;
					entity->geometryData.vertexSize = sizeof(Mesh::ProceduralVertexAABB);
					
					renderableEntityInstanceBuffer[entity->GetIndex()].vertexPositions = entity->geometryData.vertexBuffer.deviceAddress;
				}
				// Vertex normals
				if (auto vertexData = entity->meshVertexNormal.Lock(); vertexData) {
					renderableEntityInstanceBuffer[entity->GetIndex()].vertexNormals = r->renderingDevice->GetBufferDeviceAddress(vertexData->deviceBuffer);
				}
				// Vertex colors
				if (auto vertexData = entity->meshVertexColor.Lock(); vertexData) {
					renderableEntityInstanceBuffer[entity->GetIndex()].vertexColors = r->renderingDevice->GetBufferDeviceAddress(vertexData->deviceBuffer);
				}
				// Vertex UVs
				if (auto vertexData = entity->meshVertexUV.Lock(); vertexData) {
					renderableEntityInstanceBuffer[entity->GetIndex()].vertexUVs = r->renderingDevice->GetBufferDeviceAddress(vertexData->deviceBuffer);
				}
				// Transform
				if (auto transformData = entity->transform.Lock(); transformData) {
					renderableEntityInstanceBuffer[entity->GetIndex()].transform = r->renderingDevice->GetBufferDeviceAddress(transformData->deviceBuffer);
				}
			}
			if (!entity->blas && (entity->meshVertexPosition || entity->proceduralVertexAABB)) {
				entity->blas = std::make_shared<Blas>();
				if (entity->meshVertexPosition) {
					entity->blas->AssignBottomLevelGeometry(r->renderingDevice, entity->geometryData);
				} else {
					entity->blas->AssignBottomLevelProceduralVertex(r->renderingDevice, entity->geometryData);
				}
				entity->blas->CreateAndAllocate(r->renderingDevice);
				blasQueueBuildGeometryInfos.push_back(entity->blas->buildGeometryInfo);
				blasQueueBuildRangeInfos.push_back(&entity->blas->buildRangeInfo);
				entity->blas->built = true;
			}
			
			// add BLAS instance to TLAS
			if (nbRayTracingInstances < RAY_TRACING_TLAS_MAX_INSTANCES) {
				// Update and Assign transform
				if (auto transform = entity->transform.Lock(); transform && transform->data && entity->blas) {
					
					if (!r->renderingDevice->TouchAllocation(entity->blas->accelerationStructureAllocation)) {
						LOG_DEBUG("AddRayTracingBlasBuild ALLOCATION LOST")
					}
					
					int index = nbRayTracingInstances++;
					rayTracingInstanceBuffer[index].instanceCustomIndex = entity->GetIndex();
					rayTracingInstanceBuffer[index].accelerationStructureReference = entity->blas->deviceAddress;
					rayTracingInstanceBuffer[index].instanceShaderBindingTableRecordOffset = entity->sbtOffset;
					rayTracingInstanceBuffer[index].mask = entity->rayTracingMask;
					rayTracingInstanceBuffer[index].flags = entity->rayTracingFlags;
					rayTracingInstanceBuffer[index].transform = glm::transpose(transform->data->modelView);
					
					transform->data->modelView = scene->camera.viewMatrix * transform->data->worldTransform;
					transform->data->normalView = glm::transpose(glm::inverse(glm::mat3(transform->data->modelView)));
					transform->dirtyOnDevice = true;
					
					// Light Source
					if (nbActiveLights < MAX_ACTIVE_LIGHTS) {
						entity->lightSource.Do([&nbActiveLights, &transform](auto& lightSource){
							lightSourcesBuffer[nbActiveLights] = lightSource;
							lightSourcesBuffer[nbActiveLights].position = glm::vec4(scene->camera.viewMatrix * transform->data->worldTransform * glm::dvec4(glm::dvec3(lightSource.position), 1));
							++nbActiveLights;
						});
					}
				} else {
					// LOG_ERROR("An entity is missing a transform component or blas")
				}
			}
			
			currentRenderableEntities[r->currentFrameInFlight][entity->GetIndex()] = entity;
		});
		
		for (int i = nbActiveLights; i < MAX_ACTIVE_LIGHTS; ++i) {
			lightSourcesBuffer[i].Reset();
		}
	}

	void RunDynamicGraphics(VkCommandBuffer commandBuffer) {
		for (auto&[name, img] : images) {
			if (!r->renderingDevice->TouchAllocation(img->allocation)) {
				LOG_DEBUG("Image ALLOCATION LOST")
			}
		}
		
		{// Wait for previous ray-tracing pass to finish before starting transfers
			VkMemoryBarrier memoryBarrier {
				VK_STRUCTURE_TYPE_MEMORY_BARRIER,
				nullptr,// pNext
				VK_ACCESS_UNIFORM_READ_BIT | VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_MEMORY_READ_BIT,// VkAccessFlags srcAccessMask
				VK_ACCESS_TRANSFER_WRITE_BIT,// VkAccessFlags dstAccessMask
			};
			r->renderingDevice->CmdPipelineBarrier(
				commandBuffer, 
				VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 
				VK_PIPELINE_STAGE_TRANSFER_BIT, 
				0, 
				1, &memoryBarrier, 
				0, 0, 
				0, 0
			);
		}
		
		RenderableGeometryEntity::meshIndicesComponents.ForEach([commandBuffer](int32_t, auto& data){
			data.Push(r->renderingDevice, commandBuffer);
		});
		RenderableGeometryEntity::meshVertexPositionComponents.ForEach([commandBuffer](int32_t, auto& data){
			data.Push(r->renderingDevice, commandBuffer);
		});
		RenderableGeometryEntity::proceduralVertexAABBComponents.ForEach([commandBuffer](int32_t, auto& data){
			data.Push(r->renderingDevice, commandBuffer);
		});
		RenderableGeometryEntity::meshVertexNormalComponents.ForEach([commandBuffer](int32_t, auto& data){
			data.Push(r->renderingDevice, commandBuffer);
		});
		RenderableGeometryEntity::meshVertexColorComponents.ForEach([commandBuffer](int32_t, auto& data){
			data.Push(r->renderingDevice, commandBuffer);
		});
		RenderableGeometryEntity::meshVertexUVComponents.ForEach([commandBuffer](int32_t, auto& data){
			data.Push(r->renderingDevice, commandBuffer);
		});
		RenderableGeometryEntity::transformComponents.ForEach([commandBuffer](int32_t, auto& transform){
			transform.Push(r->renderingDevice, commandBuffer);
		});
		
		// Transfer data to rendering device
		scene->camera.renderOptions = RENDER_OPTIONS::Get();
		scene->camera.debugOptions = DEBUG_OPTIONS::Get();
		cameraUniformBuffer = scene->camera;
		cameraUniformBuffer.Push(commandBuffer);
		renderableEntityInstanceBuffer.Push(commandBuffer);
		lightSourcesBuffer.Push(commandBuffer);
		rayTracingInstanceBuffer.Push(commandBuffer, nbRayTracingInstances);
		
		{// Wait for TRANSFERS to finish before building BLAS
			VkMemoryBarrier memoryBarrier {
				VK_STRUCTURE_TYPE_MEMORY_BARRIER,
				nullptr,// pNext
				VK_ACCESS_TRANSFER_WRITE_BIT,// VkAccessFlags srcAccessMask
				VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR,// VkAccessFlags dstAccessMask
			};
			r->renderingDevice->CmdPipelineBarrier(
				commandBuffer, 
				VK_PIPELINE_STAGE_TRANSFER_BIT, 
				VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, 
				0, 
				1, &memoryBarrier, 
				0, 0, 
				0, 0
			);
		}
		
		// Build Buttom-level Acceleration Structures
		BuildBottomLevelRayTracingAccelerationStructures(r->renderingDevice, commandBuffer);
	
		{// Wait for BLAS to finish before building TLAS
			VkMemoryBarrier memoryBarrier {
				VK_STRUCTURE_TYPE_MEMORY_BARRIER,
				nullptr,// pNext
				VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR,// VkAccessFlags srcAccessMask
				VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR,// VkAccessFlags dstAccessMask
			};
			r->renderingDevice->CmdPipelineBarrier(
				commandBuffer, 
				VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, 
				VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, 
				0, 
				1, &memoryBarrier, 
				0, 0, 
				0, 0
			);
		}
		
		// Build Top-Level acceleration structure
		BuildTopLevelRayTracingAccelerationStructure(r->renderingDevice, commandBuffer);
		
		{// Wait for TLAS build to finish before calling any ray tracing shader
			VkMemoryBarrier memoryBarrier {
				VK_STRUCTURE_TYPE_MEMORY_BARRIER,
				nullptr,// pNext
				VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR,// VkAccessFlags srcAccessMask
				VK_ACCESS_UNIFORM_READ_BIT | VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_MEMORY_READ_BIT,// VkAccessFlags dstAccessMask
			};
			r->renderingDevice->CmdPipelineBarrier(
				commandBuffer, 
				VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, 
				VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, 
				0, 
				1, &memoryBarrier, 
				0, 0, 
				0, 0
			);
		}
		
		V4D_Mod::ForEachSortedModule([&commandBuffer](auto* mod){
			if (mod->RenderUpdate2) mod->RenderUpdate2(commandBuffer);
		}, "render");
	}
	
	void RecordGraphicsCommandBuffer(VkCommandBuffer commandBuffer, int imageIndex) {
		
		// Run Ray Tracing
		RunRayTracingCommands(commandBuffer);
		
		{// Wait for ray-tracing to finish before calling any fragment shader (overlay and post processing)
			VkMemoryBarrier memoryBarrier {
				VK_STRUCTURE_TYPE_MEMORY_BARRIER,
				nullptr,// pNext
				VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_SHADER_WRITE_BIT,// VkAccessFlags dstAccessMask
				VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_SHADER_READ_BIT,// VkAccessFlags srcAccessMask
			};
			r->renderingDevice->CmdPipelineBarrier(
				commandBuffer, 
				VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, 
				VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 
				0, 
				1, &memoryBarrier, 
				0, 0, 
				0, 0
			);
		}
		
		V4D_Mod::ForEachSortedModule([commandBuffer, imageIndex](auto* mod){
			if (mod->RecordStaticGraphicsCommands) mod->RecordStaticGraphicsCommands(commandBuffer, imageIndex);
		}, "render");
		
		// // raycast compute
		// shader_raycast_compute.SetGroupCounts(1, 1, 1);
		// shader_raycast_compute.Execute(r->renderingDevice, commandBuffer);
		
		RecordPostProcessingCommands(commandBuffer, imageIndex);
		
		V4D_Mod::ForEachSortedModule([commandBuffer, imageIndex](auto* mod){
			if (mod->RecordStaticGraphicsCommands2) mod->RecordStaticGraphicsCommands2(commandBuffer, imageIndex);
		}, "render");
	}

#pragma endregion

///////////////////////////////////////////////////////////

V4D_MODULE_CLASS(V4D_Mod) {
	
	V4D_MODULE_FUNC(int, OrderIndex) {return -1000;}
	
	#pragma region Containers Access
		
		V4D_MODULE_FUNC(Image*, GetImage, const std::string& name) {
			if (images.find(name) == images.end()) {
				throw std::runtime_error(std::string("Image '") + name + "' does not exist");
				return nullptr;
			}
			return images[name];
		}
		
		V4D_MODULE_FUNC(PipelineLayout*, GetPipelineLayout, const std::string& name) {
			if (pipelineLayouts.find(name) == pipelineLayouts.end()) {
				throw std::runtime_error(std::string("Pipeline layout '") + name + "' does not exist");
				return nullptr;
			}
			return pipelineLayouts[name];
		}
		
		V4D_MODULE_FUNC(void, AddShader, const std::string& groupName, RasterShaderPipeline* shader) {
			if (shaderGroups.find(groupName) == shaderGroups.end()) {
				throw std::runtime_error(std::string("Shader group '") + groupName + "' does not exist");
				return;
			}
			shaderGroups[groupName].push_back(shader);
		}
		
		V4D_MODULE_FUNC(ShaderBindingTable*, GetShaderBindingTable, const std::string& sbtName) {
			if (shaderBindingTables.find(sbtName) == shaderBindingTables.end()) {
				throw std::runtime_error(std::string("Shader binding table '") + sbtName + "' does not exist");
				return nullptr;
			}
			return shaderBindingTables[sbtName];
		}
		
	#pragma endregion
	
	#pragma region Graphics configuration / Init
		
		V4D_MODULE_FUNC(void, ScorePhysicalDeviceSelection, int& score, PhysicalDevice* physicalDevice) {
			// Higher score for Ray Tracing support
			if (physicalDevice->GetRayQueryFeatures().rayQuery) score += 1;
			if (!physicalDevice->GetRayTracingPipelineFeatures().rayTracingPipeline) score *= 0;
		}
		
		V4D_MODULE_FUNC(void, LoadScene, Scene* _s) {
			scene = _s;
		}
		
		V4D_MODULE_FUNC(void, UnloadScene) {
			RenderableGeometryEntity::ClearAll();
		}
		
		V4D_MODULE_FUNC(void, InitRenderer, Renderer* _r) {
			r = _r;
			
			V4D_Mod::SortModules([](auto* a, auto* b){
				return (a->RenderOrderIndex? a->RenderOrderIndex():0) < (b->RenderOrderIndex? b->RenderOrderIndex():0);
			}, "render");
			
			r->queuesInfo.emplace_back("secondary", VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT);
			
			// Device Extensions
			if (Loader::VULKAN_API_VERSION >= VK_API_VERSION_1_2) {
				r->RequiredDeviceExtension(VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME);
				// RayTracing extensions
				r->OptionalDeviceExtension(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME);
				r->OptionalDeviceExtension(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME);
				r->OptionalDeviceExtension(VK_KHR_RAY_QUERY_EXTENSION_NAME);
				// Needed for RayTracing extensions
				r->OptionalDeviceExtension(VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME);
				r->OptionalDeviceExtension(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME);
				r->OptionalDeviceExtension(VK_KHR_PIPELINE_LIBRARY_EXTENSION_NAME);
			}
		}
		
		V4D_MODULE_FUNC(void, InitVulkanDeviceFeatures) {
			r->deviceFeatures.shaderFloat64 = VK_TRUE;
			r->deviceFeatures.shaderInt64 = VK_TRUE;
			r->deviceFeatures.depthClamp = VK_TRUE;
			r->deviceFeatures.fillModeNonSolid = VK_TRUE;
			r->deviceFeatures.geometryShader = VK_TRUE;
			r->deviceFeatures.wideLines = VK_TRUE;
			
			// Vulkan 1.2
			if (Loader::VULKAN_API_VERSION >= VK_API_VERSION_1_2) {
				r->EnableVulkan12DeviceFeatures()->bufferDeviceAddress = VK_TRUE;
				r->EnableVulkan12DeviceFeatures()->descriptorIndexing = VK_TRUE;
				r->EnableAccelerationStructureFeatures()->accelerationStructure = VK_TRUE;
				r->EnableAccelerationStructureFeatures()->descriptorBindingAccelerationStructureUpdateAfterBind = VK_TRUE;
				r->EnableRayTracingPipelineFeatures()->rayTracingPipeline = VK_TRUE;
				r->EnableRayTracingPipelineFeatures()->rayTracingPipelineTraceRaysIndirect = VK_TRUE;
				r->EnableRayQueryFeatures()->rayQuery = VK_TRUE;
			}
		}
		
		V4D_MODULE_FUNC(void, ConfigureRenderer) {
			if (!r->rayTracingPipelineFeatures.rayTracingPipeline) {
				throw std::runtime_error("Ray-tracing not supported");
			}
			LOG_SUCCESS("Ray-Tracing Supported")
			// Query the ray tracing properties of the current implementation
			accelerationStructureProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR;
			rayTracingPipelineProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;
			accelerationStructureProperties.pNext = &rayTracingPipelineProperties;
			VkPhysicalDeviceProperties2 deviceProps2 {};{
				deviceProps2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
				deviceProps2.pNext = &accelerationStructureProperties;
			}
			r->GetPhysicalDeviceProperties2(r->renderingDevice->GetPhysicalDevice()->GetHandle(), &deviceProps2);
		}
	
	#pragma endregion
	
	V4D_MODULE_FUNC(void, InitVulkanLayouts) {
		{r->descriptorSets["set0"] = &set0;
			set0.AddBinding_uniformBuffer(0, cameraUniformBuffer, VK_SHADER_STAGE_ALL_GRAPHICS | VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_INTERSECTION_BIT_KHR | VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR);
			set0.AddBinding_accelerationStructure(1, &topLevelAccelerationStructure.accelerationStructure, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_FRAGMENT_BIT);
			set0.AddBinding_storageBuffer(2, renderableEntityInstanceBuffer, VK_SHADER_STAGE_ALL_GRAPHICS | VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_INTERSECTION_BIT_KHR | VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR);
			set0.AddBinding_storageBuffer(3, lightSourcesBuffer, VK_SHADER_STAGE_ALL_GRAPHICS | VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_INTERSECTION_BIT_KHR | VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR);
			
			set0.AddBinding_combinedImageSampler(4, tex_metal_normal.GetImage(), VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_FRAGMENT_BIT);
		}
		
		{r->descriptorSets["set1_raytracing"] = &set1_raytracing;
			set1_raytracing.AddBinding_imageView(0, &img_lit, VK_SHADER_STAGE_RAYGEN_BIT_KHR);
			set1_raytracing.AddBinding_imageView(1, &img_depth, VK_SHADER_STAGE_RAYGEN_BIT_KHR);
		}
		
		{r->descriptorSets["set1_post"] = &set1_post;
			int i = 0;
			set1_post.AddBinding_combinedImageSampler(i++, &img_lit, VK_SHADER_STAGE_FRAGMENT_BIT);
			set1_post.AddBinding_combinedImageSampler(i++, &img_overlay, VK_SHADER_STAGE_FRAGMENT_BIT);
			set1_post.AddBinding_combinedImageSampler(i++, &img_history, VK_SHADER_STAGE_FRAGMENT_BIT);
			set1_post.AddBinding_combinedImageSampler(i++, &img_depth, VK_SHADER_STAGE_FRAGMENT_BIT);
			set1_post.AddBinding_inputAttachment(i++, &img_pp, VK_SHADER_STAGE_FRAGMENT_BIT);
		}
		
		{r->descriptorSets["set1_overlay"] = &set1_overlay;
			set1_overlay.AddBinding_combinedImageSampler(0, tex_img_font_atlas.GetImage(), VK_SHADER_STAGE_FRAGMENT_BIT);
		}
		
		{r->descriptorSets["set1_thumbnail"] = &set1_thumbnail;
			set1_thumbnail.AddBinding_combinedImageSampler(0, &img_lit, VK_SHADER_STAGE_FRAGMENT_BIT);
		}
		
		{r->descriptorSets["set1_histogram"] = &set1_histogram;
			set1_histogram.AddBinding_imageView(0, &img_thumbnail, VK_SHADER_STAGE_COMPUTE_BIT);
			set1_histogram.AddBinding_storageBuffer(1, totalLuminance, VK_SHADER_STAGE_COMPUTE_BIT);
		}
		
		{r->descriptorSets["set1_raycast"] = &set1_raycast;
			set1_raycast.AddBinding_storageBuffer(0, raycastBuffer, VK_SHADER_STAGE_COMPUTE_BIT);
		}
		
		{ // Assign descriptor sets to layouts
			// Add the same set 0 to all pipeline layouts
			for (auto[name, layout] : pipelineLayouts) {
				layout->AddDescriptorSet(&set0);
			}
			// Add specific set 1 to specific layout lists
			pipelineLayouts["pl_raytracing"]->AddDescriptorSet(&set1_raytracing);
			pipelineLayouts["pl_thumbnail"]->AddDescriptorSet(&set1_thumbnail);
			pipelineLayouts["pl_overlay"]->AddDescriptorSet(&set1_overlay);
			pipelineLayouts["pl_post"]->AddDescriptorSet(&set1_post);
			pipelineLayouts["pl_histogram"]->AddDescriptorSet(&set1_histogram);
			pipelineLayouts["pl_raycast"]->AddDescriptorSet(&set1_raycast);
		}
	}
	
	#pragma region Load/Upload Renderer
		
		V4D_MODULE_FUNC(void, ConfigureShaders) {
			ConfigureRayTracingShaders();
			ConfigurePostProcessingShaders();
			ConfigureOverlayShaders();
		}

		V4D_MODULE_FUNC(void, ReadShaders) {
			for (auto&[grpName, shaderList] : shaderGroups) {
				for (auto* shader : shaderList) {
					shader->ReadShaders();
				}
			}
			
			for (auto&[name, sbt] : shaderBindingTables) {
				sbt->ReadShaders();
			}
			
			shader_histogram_compute.ReadShaders();
			shader_raycast_compute.ReadShaders();
		}
		
		V4D_MODULE_FUNC(void, CreateVulkanSyncObjects) {
			semaphores["imageAvailable"].resize(r->NB_FRAMES_IN_FLIGHT);
			semaphores["staticRenderFinished"].resize(r->NB_FRAMES_IN_FLIGHT);
			semaphores["dynamicRenderFinished"].resize(r->NB_FRAMES_IN_FLIGHT);
			fences["graphics"].resize(r->NB_FRAMES_IN_FLIGHT);
			fences["compute"].resize(r->NB_FRAMES_IN_FLIGHT);

			VkSemaphoreCreateInfo semaphoreInfo = {};
				semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

			VkFenceCreateInfo fenceInfo = {};
				fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
				fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT; // Initialize in the signaled state so that we dont get stuck on the first frame

			for (int i = 0; i < r->NB_FRAMES_IN_FLIGHT; i++) {
				for (auto&[name, s] : semaphores) {
					if (r->renderingDevice->CreateSemaphore(&semaphoreInfo, nullptr, &s[i]) != VK_SUCCESS) {
						throw std::runtime_error("Failed to create semaphores");
					}
				}
				for (auto&[name, f] : fences) {
					if (r->renderingDevice->CreateFence(&fenceInfo, nullptr, &f[i]) != VK_SUCCESS) {
						throw std::runtime_error("Failed to create fences");
					}
				}
			}
		}

		V4D_MODULE_FUNC(void, DestroyVulkanSyncObjects) {
			for (int i = 0; i < r->NB_FRAMES_IN_FLIGHT; i++) {
				for (auto&[name, s] : semaphores) r->renderingDevice->DestroySemaphore(s[i], nullptr);
				for (auto&[name, f] : fences) r->renderingDevice->DestroyFence(f[i], nullptr);
			}
		}

		V4D_MODULE_FUNC(void, CreateVulkanResources) {
			CreateUiResources();
			CreateRenderingResources();
			CreatePostProcessingResources();
		
			CreateRayTracingResources();
			
			// It appears that we now need to initially BUILD the empty TLAS before updating descriptor sets... maybe a bug in the validation layers (issue #2368 opened)
			r->renderingDevice->RunSingleTimeCommands(r->renderingDevice->GetQueue("graphics"), [](auto commandBuffer){
				BuildTopLevelRayTracingAccelerationStructure(r->renderingDevice, commandBuffer);
			});
			
			
			// Textures
			tex_img_font_atlas.SetMipLevels();
			// tex_img_font_atlas.SetSamplerAnisotropy(16);
			tex_img_font_atlas.AllocateAndWriteStagingMemory(r->renderingDevice);
			tex_img_font_atlas.CreateImage(r->renderingDevice);
			auto commandBuffer = r->renderingDevice->BeginSingleTimeCommands(r->renderingDevice->GetQueue("graphics"));
				tex_img_font_atlas.CopyStagingBufferToImage(r->renderingDevice, commandBuffer);
			r->renderingDevice->EndSingleTimeCommands(r->renderingDevice->GetQueue("graphics"), commandBuffer);
			tex_img_font_atlas.FreeStagingMemory(r->renderingDevice);
			
			for (auto* tex : textures) {
				tex->SetSamplerAddressMode(VK_SAMPLER_ADDRESS_MODE_REPEAT);
				tex->SetMipLevels();
				tex->AllocateAndWriteStagingMemory(r->renderingDevice);
				tex->CreateImage(r->renderingDevice);
				auto commandBuffer = r->renderingDevice->BeginSingleTimeCommands(r->renderingDevice->GetQueue("graphics"));
					tex->CopyStagingBufferToImage(r->renderingDevice, commandBuffer);
				r->renderingDevice->EndSingleTimeCommands(r->renderingDevice->GetQueue("graphics"), commandBuffer);
				tex->FreeStagingMemory(r->renderingDevice);
			}
		}
		
		V4D_MODULE_FUNC(void, DestroyVulkanResources) {
			DestroyUiResources();
			DestroyRenderingResources();
			DestroyPostProcessingResources();
			DestroyRayTracingResources();
			
			// Textures
			tex_img_font_atlas.DestroyImage(r->renderingDevice);
			
			for (auto* tex : textures) {
				tex->DestroyImage(r->renderingDevice);
			}
		}
		
		V4D_MODULE_FUNC(void, AllocateVulkanBuffers) {
			// Buffers
			cameraUniformBuffer.Allocate(r->renderingDevice);
			renderableEntityInstanceBuffer.Allocate(r->renderingDevice);
			lightSourcesBuffer.Allocate(r->renderingDevice);
			
			// Overlays
			overlayLinesBuffer.Allocate(r->renderingDevice, MEMORY_USAGE_CPU_TO_GPU);
			overlayLinesBuffer.MapMemory(r->renderingDevice);
			overlayLines = (OverlayLine*)overlayLinesBuffer.data;
			overlayTextBuffer.Allocate(r->renderingDevice, MEMORY_USAGE_CPU_TO_GPU);
			overlayTextBuffer.MapMemory(r->renderingDevice);
			overlayText = (OverlayText*)overlayTextBuffer.data;
			overlayCirclesBuffer.Allocate(r->renderingDevice, MEMORY_USAGE_CPU_TO_GPU);
			overlayCirclesBuffer.MapMemory(r->renderingDevice);
			overlayCircles = (OverlayCircle*)overlayCirclesBuffer.data;
			overlaySquaresBuffer.Allocate(r->renderingDevice, MEMORY_USAGE_CPU_TO_GPU);
			overlaySquaresBuffer.MapMemory(r->renderingDevice);
			overlaySquares = (OverlaySquare*)overlaySquaresBuffer.data;
			
			AllocateComputeBuffers();
			AllocateRayTracingBuffers();
		}
		
		V4D_MODULE_FUNC(void, FreeVulkanBuffers) {
			RenderableGeometryEntity::ForEach([](auto entity){
				entity->FreeComponentsBuffers();
			});
			
			for (auto& v : currentRenderableEntities) {
				for (auto& [i,e] : v) {
					if (e) e->FreeComponentsBuffers();
				}
				v.clear();
			}
			
			// Overlays
			overlayLinesBuffer.UnmapMemory(r->renderingDevice);
			overlayLinesBuffer.Free(r->renderingDevice);
			overlayLines = nullptr;
			overlayTextBuffer.UnmapMemory(r->renderingDevice);
			overlayTextBuffer.Free(r->renderingDevice);
			overlayText = nullptr;
			overlayCirclesBuffer.UnmapMemory(r->renderingDevice);
			overlayCirclesBuffer.Free(r->renderingDevice);
			overlayCircles = nullptr;
			overlaySquaresBuffer.UnmapMemory(r->renderingDevice);
			overlaySquaresBuffer.Free(r->renderingDevice);
			overlaySquares = nullptr;
			
			// Buffers
			cameraUniformBuffer.Free();
			renderableEntityInstanceBuffer.Free();
			lightSourcesBuffer.Free();
			
			FreeComputeBuffers();
			FreeRayTracingBuffers();
		}

		V4D_MODULE_FUNC(void, CreateVulkanPipelines) {
			// Sort shaders
			for (auto&[rs, ss] : shaderGroups) {
				std::sort(ss.begin(), ss.end(), [](auto* a, auto* b){
					return a->sortIndex < b->sortIndex;
				});
			}
			
			// Pipeline layouts
			for (auto[name, layout] : pipelineLayouts) {
				layout->Create(r->renderingDevice);
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
		
		V4D_MODULE_FUNC(void, DestroyVulkanPipelines) {
			// UI
			#ifdef _ENABLE_IMGUI
				UnloadImGui();
			#endif
			DestroyUiPipeline();
			
			// Ray Tracing
			DestroyRayTracingPipeline();
			
			// Post Processing
			DestroyPostProcessingPipeline();
			
			// Pipeline layouts
			for (auto[name,layout] : pipelineLayouts) {
				layout->Destroy(r->renderingDevice);
			}
			
		}
		
		V4D_MODULE_FUNC(void, CreateVulkanCommandBuffers) {
			VkCommandBufferAllocateInfo allocInfo = {};
			allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
			allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
							/*	VK_COMMAND_BUFFER_LEVEL_PRIMARY = Can be submitted to a queue for execution, but cannot be called from other command buffers
								VK_COMMAND_BUFFER_LEVEL_SECONDARY = Cannot be submitted directly, but can be called from primary command buffers
							*/
			
			{// Graphics Commands Recorder
				// Because one of the drawing commands involves binding the right VkFramebuffer, we'll actually have to record a command buffer for every image in the swap chain once again.
				// Command buffers will be automatically freed when their command pool is destroyed, so we don't need an explicit cleanup
				commandBuffers["graphics"].resize(r->swapChain->imageViews.size());
				
				allocInfo.commandPool = r->renderingDevice->GetQueue("graphics").commandPool;
				allocInfo.commandBufferCount = (uint) commandBuffers["graphics"].size();
				if (r->renderingDevice->AllocateCommandBuffers(&allocInfo, commandBuffers["graphics"].data()) != VK_SUCCESS) {
					throw std::runtime_error("Failed to allocate command buffers");
				}

				// Starting command buffer recording...
				for (size_t i = 0; i < commandBuffers["graphics"].size(); i++) {
					VkCommandBufferBeginInfo beginInfo = {};
					beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
					beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT; // We have used this flag because we may already be scheduling the drawing commands for the next frame while the last frame is not finished yet.
									/*	VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT = The command buffer will be rerecorded right after executing it once
										VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT = This is a secondary command buffer that will be entirely within a single render pass.
										VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT = The command buffer can be resubmited while it is also already pending execution
									*/
					beginInfo.pInheritanceInfo = nullptr; // only relevant for secondary command buffers. It specifies which state to inherit from the calling primary command buffers.
					// If a command buffer was already recorded once, then a call to vkBeginCommandBuffer will implicitly reset it.
					// It's not possible to append commands to a buffer at a later time.
					if (r->renderingDevice->BeginCommandBuffer(commandBuffers["graphics"][i], &beginInfo) != VK_SUCCESS) {
						throw std::runtime_error("Faild to begin recording command buffer");
					}
					
					// Record commands
					RecordGraphicsCommandBuffer(commandBuffers["graphics"][i], i);
					
					if (r->renderingDevice->EndCommandBuffer(commandBuffers["graphics"][i]) != VK_SUCCESS) {
						throw std::runtime_error("Failed to record command buffer");
					}
				}
			}
			commandBuffers["graphicsDynamic"].resize(commandBuffers["graphics"].size());
			if (r->renderingDevice->AllocateCommandBuffers(&allocInfo, commandBuffers["graphicsDynamic"].data()) != VK_SUCCESS) {
				throw std::runtime_error("Failed to allocate command buffers");
			}
		}

		V4D_MODULE_FUNC(void, DestroyVulkanCommandBuffers) {
			r->renderingDevice->FreeCommandBuffers(r->renderingDevice->GetQueue("graphics").commandPool, static_cast<uint32_t>(commandBuffers["graphics"].size()), commandBuffers["graphics"].data());
			r->renderingDevice->FreeCommandBuffers(r->renderingDevice->GetQueue("graphics").commandPool, static_cast<uint32_t>(commandBuffers["graphicsDynamic"].size()), commandBuffers["graphicsDynamic"].data());
		}
	
	#pragma endregion
	
	#pragma region Overlay
		
		V4D_MODULE_FUNC(void, DrawOverlayLine, float x1, float y1, float x2, float y2, glm::vec4 color, float lineWidth) {
			AddOverlayLine(x1, y1, x2, y2, color, lineWidth);
		}
		V4D_MODULE_FUNC(void, DrawOverlayText, const char* text, float x, float y, glm::vec4 color, float size) {
			AddOverlayText(std::string(text), x, y, color, uint16_t(size));
		}
		V4D_MODULE_FUNC(void, DrawOverlayCircle, float x, float y, glm::vec4 color, float size, float borderSize) {
			AddOverlayCircle(x, y, color, uint16_t(size), uint8_t(borderSize));
		}
		V4D_MODULE_FUNC(void, DrawOverlaySquare, float x, float y, glm::vec4 color, glm::vec2 size, float borderSize) {
			AddOverlaySquare(x, y, color, size, uint8_t(borderSize));
		}
		
	#pragma endregion

	V4D_MODULE_FUNC(void, RenderUpdate) {
		
		uint64_t timeout = 1000UL * 1000 * 1000 * 10; // 10 seconds

		// Get an image from the swapchain
		uint imageIndex;
		{
			VkResult result = r->renderingDevice->AcquireNextImageKHR(
				r->swapChain->GetHandle(), // swapChain
				timeout, // timeout in nanoseconds (using max disables the timeout)
				semaphores["imageAvailable"][r->currentFrameInFlight], // semaphore
				VK_NULL_HANDLE, // fence
				&imageIndex // output the index of the swapchain image in there
			);

			// Check for errors
			if (result == VK_ERROR_OUT_OF_DATE_KHR) {
				// SwapChain is out of date, for instance if the window was resized, stop here and ReCreate the swapchain.
				r->RecreateSwapChains();
				return;
			} else if (result == VK_SUBOPTIMAL_KHR) {
				LOG_VERBOSE("Swapchain is suboptimal...")
			} else if (result != VK_SUCCESS) {
				throw std::runtime_error("Failed to acquire swap chain images");
			}
		}
		
		// r->renderingDevice->QueueWaitIdle(r->renderingDevice->GetQueue("graphics").handle);

		if (r->renderingDevice->WaitForFences(1, &fences["graphics"][r->currentFrameInFlight], VK_TRUE, timeout) != VK_SUCCESS) {
			throw std::runtime_error("Failed on WaitForFences");
			return;
		}
		r->renderingDevice->ResetFences(1, &fences["graphics"][r->currentFrameInFlight]);
		
		// Update data every frame
		FrameUpdate(imageIndex);

		std::array<VkSubmitInfo, 2> computeSubmitInfo {};
			computeSubmitInfo[0].sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
			computeSubmitInfo[1].sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		std::array<VkSubmitInfo, 2> graphicsSubmitInfo {};
			graphicsSubmitInfo[0].sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
			graphicsSubmitInfo[1].sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		
		std::array<VkSemaphore, 2> staticGraphicsWaitSemaphores {
			semaphores["imageAvailable"][r->currentFrameInFlight],
			semaphores["dynamicRenderFinished"][r->currentFrameInFlight],
		};
		VkPipelineStageFlags staticGraphicsWaitStages[] {
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		};
		
		{// Configure Graphics
			VkCommandBufferBeginInfo beginInfo = {};
			beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
			beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
			
			r->renderingDevice->ResetCommandBuffer(commandBuffers["graphicsDynamic"][imageIndex], 0);
			if (r->renderingDevice->BeginCommandBuffer(commandBuffers["graphicsDynamic"][imageIndex], &beginInfo) != VK_SUCCESS) {
				throw std::runtime_error("Faild to begin recording command buffer");
			}
			RunDynamicGraphics(commandBuffers["graphicsDynamic"][imageIndex]);
			if (r->renderingDevice->EndCommandBuffer(commandBuffers["graphicsDynamic"][imageIndex]) != VK_SUCCESS) {
				throw std::runtime_error("Failed to record command buffer");
			}
			// dynamic commands
			graphicsSubmitInfo[0].waitSemaphoreCount = 0;
			graphicsSubmitInfo[0].pWaitSemaphores = nullptr;
			graphicsSubmitInfo[0].pWaitDstStageMask = nullptr;
			graphicsSubmitInfo[0].commandBufferCount = 1;
			graphicsSubmitInfo[0].pCommandBuffers = &commandBuffers["graphicsDynamic"][imageIndex];
			graphicsSubmitInfo[0].signalSemaphoreCount = 1;
			graphicsSubmitInfo[0].pSignalSemaphores = &semaphores["dynamicRenderFinished"][r->currentFrameInFlight];
			// static commands
			graphicsSubmitInfo[1].waitSemaphoreCount = staticGraphicsWaitSemaphores.size();
			graphicsSubmitInfo[1].pWaitSemaphores = staticGraphicsWaitSemaphores.data();
			graphicsSubmitInfo[1].pWaitDstStageMask = staticGraphicsWaitStages;
			graphicsSubmitInfo[1].commandBufferCount = 1;
			graphicsSubmitInfo[1].pCommandBuffers = &commandBuffers["graphics"][imageIndex];
			graphicsSubmitInfo[1].signalSemaphoreCount = 1;
			graphicsSubmitInfo[1].pSignalSemaphores = &semaphores["staticRenderFinished"][r->currentFrameInFlight];
		}
		
		// Submit Graphics
		VkResult result = r->renderingDevice->QueueSubmit(r->renderingDevice->GetQueue("graphics").handle, graphicsSubmitInfo.size(), graphicsSubmitInfo.data(), fences["graphics"][r->currentFrameInFlight]);
		if (result != VK_SUCCESS) {
			if (result == VK_ERROR_DEVICE_LOST) {
				LOG_ERROR("Render() Failed to submit graphics command buffer : VK_ERROR_DEVICE_LOST")
				// SLEEP(500ms)
				// ReloadRenderer();
				// return;
			}
			LOG_ERROR((int)result)
			throw std::runtime_error("Render() Failed to submit graphics command buffer");
		}

		// Present
		VkPresentInfoKHR presentInfo = {};
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		// Specify which semaphore to wait on before presentation can happen
		presentInfo.waitSemaphoreCount = 1;
		VkSemaphore presentWaitSemaphores[] = {
			semaphores["staticRenderFinished"][r->currentFrameInFlight],
		};
		presentInfo.pWaitSemaphores = presentWaitSemaphores;
		// Specify the swap chains to present images to and the index for each swap chain. (almost always a single one)
		VkSwapchainKHR swapChains[] = {r->swapChain->GetHandle()};
		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = swapChains;
		presentInfo.pImageIndices = &imageIndex;
		// The next param allows to specify an array of VkResult values to check for every individual swap chain if presentation was successful.
		// its not necessary if only using a single swap chain, because we can simply use the return value of the present function.
		presentInfo.pResults = nullptr;
		// Send the present info to the presentation queue !
		// This submits the request to present an image to the swap chain.
		result = r->renderingDevice->QueuePresentKHR(r->renderingDevice->GetQueue("present").handle, &presentInfo);

		// Check for errors
		if (result == VK_ERROR_OUT_OF_DATE_KHR) {
			// SwapChain is out of date, for instance if the window was resized, stop here and ReCreate the swapchain.
			r->RecreateSwapChains();
			return;
		} else if (result == VK_SUBOPTIMAL_KHR) {
			LOG_VERBOSE("Swapchain is suboptimal...")
		} else if (result != VK_SUCCESS) {
			throw std::runtime_error("Failed to present swap chain images");
		}
	}
	
	V4D_MODULE_FUNC(void, SecondaryRenderUpdate) {
		PostProcessingUpdate2();
		
		// Modules
		V4D_Mod::ForEachSortedModule([](auto* mod){
			if (mod->BeginSecondaryFrameUpdate) mod->BeginSecondaryFrameUpdate();
		});
	
		// Dynamic compute
		auto cmdBuffer = r->BeginSingleTimeCommands(r->renderingDevice->GetQueue("secondary"));
		
			RunDynamicPostProcessingCompute(cmdBuffer);
			
			// Modules
			V4D_Mod::ForEachSortedModule([cmdBuffer](auto* mod){
				if (mod->SecondaryFrameCompute) mod->SecondaryFrameCompute(cmdBuffer);
			});
			V4D_Mod::ForEachSortedModule([cmdBuffer](auto* mod){
				if (mod->SecondaryRenderUpdate2) mod->SecondaryRenderUpdate2(cmdBuffer);
			}, "render");
			
		r->EndSingleTimeCommands(r->renderingDevice->GetQueue("secondary"), cmdBuffer);
	}
	
	V4D_MODULE_FUNC(void, DrawUi) {
		#ifdef _ENABLE_IMGUI
			ImGui::SetNextWindowSize({340, 150});
			ImGui::Begin("Settings and Modules");
			// #ifdef _DEBUG
				ImGui::Checkbox("Debug Physics", &DEBUG_OPTIONS::PHYSICS);
				ImGui::Checkbox("Debug Normals", &DEBUG_OPTIONS::NORMALS);
			// #endif
			if (r->rayTracingPipelineFeatures.rayTracingPipeline) {
				ImGui::Checkbox("Ray-traced reflections", &RENDER_OPTIONS::REFLECTIONS);
			} else {
				ImGui::Text("Ray-Tracing unavailable, using rasterization");
			}
			ImGui::Checkbox("Ray-traced Shadows", &RENDER_OPTIONS::HARD_SHADOWS);
			ImGui::Checkbox("TXAA", &RENDER_OPTIONS::TXAA);
			ImGui::Checkbox("Gamma correction", &RENDER_OPTIONS::GAMMA_CORRECTION);
			ImGui::Checkbox("HDR Tone Mapping", &RENDER_OPTIONS::HDR_TONE_MAPPING);
			ImGui::SliderFloat("HDR Exposure", &exposureFactor, 0, 10);
			ImGui::SliderFloat("brightness", &scene->camera.brightness, 0, 2);
			ImGui::SliderFloat("contrast", &scene->camera.contrast, 0, 2);
			ImGui::SliderFloat("gamma", &scene->camera.gamma, 0, 5);
		#endif
		// Modules
		V4D_Mod::ForEachSortedModule([](auto* mod){
			if (mod->DrawUi2) {
				#ifdef _ENABLE_IMGUI
					ImGui::Separator();
				#endif
				mod->DrawUi2();
			}
		});
		#ifdef _ENABLE_IMGUI
			ImGui::End();
		#endif
		// #ifdef _DEBUG
			#ifdef _ENABLE_IMGUI
				ImGui::SetNextWindowPos({425+345,0});
				ImGui::SetNextWindowSize({250, 100});
				ImGui::Begin("Debug");
				// RayCast
				if (currentRayCast.hit) ImGui::Text((std::string("RayCast Hit object ") + std::to_string(currentRayCast.objectBufferOffset) + ", data: " + std::to_string(currentRayCast.customData0)).c_str());
				else ImGui::Text("RayCast no hit");
			#endif
				// Modules
				V4D_Mod::ForEachSortedModule([](auto* mod){
					#ifdef _ENABLE_IMGUI
						ImGui::Separator();
					#endif
					if (mod->DrawUiDebug2) mod->DrawUiDebug2();
				});
			#ifdef _ENABLE_IMGUI
				ImGui::End();
			#endif
		// #endif
	}
	
	// Render pipelines
	
	V4D_MODULE_FUNC(void, SecondaryRenderUpdate2, VkCommandBuffer commandBuffer) {
		// Overlay/UI
		uiRenderPass.Begin(r->renderingDevice, commandBuffer, img_overlay, {{.0,.0,.0,.0}});
			{
				// Adjust lines and text buffers
				std::lock_guard lock(overlayMutex);
				auto tmpCurrentLinesVertexBufferOffset = shader_overlay_lines.vertexOffset;
				shader_overlay_lines.vertexCount = overlayLinesCount;
				shader_overlay_text.vertexCount = overlayTextSize;
				shader_overlay_circles.vertexCount = overlayCirclesCount;
				shader_overlay_squares.vertexCount = overlaySquaresCount;
				
				for (auto* shader : shaderGroups["sg_overlay"]) {
					if (shader == &shader_overlay_lines) {
						if (overlayLinesCount == 0) continue;
						shader->Bind(r->renderingDevice, commandBuffer);
						float lineWidth = 1.0f;
						uint32_t linesRendered = 0;
						do {
							shader_overlay_lines.vertexCount = 0;
							while (shader_overlay_lines.vertexCount < overlayLinesCount && overlayLines[linesRendered + shader_overlay_lines.vertexCount].width == lineWidth) {
								shader_overlay_lines.vertexCount += 2;
							}
							if (shader_overlay_lines.vertexCount > 0) {
								shader_overlay_lines.vertexOffset = sizeof(OverlayLine)*linesRendered + tmpCurrentLinesVertexBufferOffset;
								r->renderingDevice->CmdSetLineWidth(commandBuffer, lineWidth);
								shader->Render(r->renderingDevice, commandBuffer, 1);
								linesRendered += shader_overlay_lines.vertexCount;
							}
							if (linesRendered < overlayLinesCount) {
								lineWidth = overlayLines[linesRendered].width;
							} else {
								break;
							}
						} while (true);
					} else {
						if (shader == &shader_overlay_text && overlayTextSize == 0) continue;
						if (shader == &shader_overlay_circles && overlayCirclesCount == 0) continue;
						if (shader == &shader_overlay_squares && overlaySquaresCount == 0) continue;
						shader->Execute(r->renderingDevice, commandBuffer);
					}
				}
				shader_overlay_lines.vertexOffset = tmpCurrentLinesVertexBufferOffset;
				overlayLinesCount = 0;
				overlayTextSize = 0;
				overlayCirclesCount = 0;
				overlaySquaresCount = 0;
			}
			#ifdef _ENABLE_IMGUI
				DrawImGui(commandBuffer);
			#endif
		uiRenderPass.End(r->renderingDevice, commandBuffer);
	}
	
	#pragma region Input
	
	V4D_MODULE_FUNC(std::string, InputCallbackName) {return THIS_MODULE;}
	
	V4D_MODULE_FUNC(void, InputKeyCallback, int key, int scancode, int action, int mods) {
		if (action != GLFW_RELEASE
			#ifdef _ENABLE_IMGUI
				&& !ImGui::IsAnyWindowFocused()
			#endif
		) {
			switch (key) {
				
				// Reload Renderer
				case GLFW_KEY_R:
					r->ReloadRenderer();
					break;
				
			}
		}
	}
	
	#pragma endregion
	
};
