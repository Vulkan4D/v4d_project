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

	enum RenderingMode : int {
		rasterization,
		fullRayTracing,
		// hybridRayTracing,
		// pathTracing,
	};
	const char* RENDER_MODES_STR = 
		"Rasterization\0"
		"Full Ray Tracing\0"
		// "Hybrid Ray Tracing\0"
		// "Path Tracing\0"
	;
	enum ShadowType : int {
		shadows_off,
		shadows_hard,
		// shadows_soft,
	};
	const char* SHADOW_TYPES_STR = 
		"Disabled\0"
		"Hard shadows\0"
		// "Soft shadows\0"
	;
	static const int DEFAULT_RENDER_MODE = fullRayTracing;
	static const int DEFAULT_SHADOW_TYPE = shadows_hard;

	Image litImage { VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT ,1,1, { VK_FORMAT_R16G16B16A16_SFLOAT }};
	Image depthImage { VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT ,1,1, { VK_FORMAT_R32_SFLOAT } };
	DescriptorSet visibilityDescriptorSet_1, lightingDescriptorSet_1;
	
	RenderPass lightingRenderPass;
	PipelineLayout lightingPipelineLayout;
	RasterShaderPipeline lightingShader {lightingPipelineLayout, {
		"incubator_rendering/assets/shaders/raster.lighting.vert",
		"incubator_rendering/assets/shaders/raster.lighting.frag",
	}};
	// ComputeShaderPipeline lightingComputeShader {lightingPipelineLayout, "incubator_rendering/assets/shaders/raster.lighting.comp"};
	
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
	
	void CreateLightingPipeline() {
		
		{// Lighting pass
			const int nbColorAttachments = 1;
			const int nbInputAttachments = NB_G_BUFFERS;
			std::array<VkAttachmentDescription, nbColorAttachments + nbInputAttachments> attachments {};
			std::vector<Image*> images(nbColorAttachments + nbInputAttachments);
			std::array<VkAttachmentReference, nbColorAttachments> colorAttachmentRefs0 {};
			std::array<VkAttachmentReference, nbInputAttachments> inputAttachmentRefs0 {};
			
			int i = 0;
			
			// Color attachment
			images[i] = &litImage;
			attachments[i].format = litImage.format;
			attachments[i].samples = VK_SAMPLE_COUNT_1_BIT;
			attachments[i].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
			attachments[i].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			attachments[i].initialLayout = VK_IMAGE_LAYOUT_GENERAL;
			attachments[i].finalLayout = VK_IMAGE_LAYOUT_GENERAL;
			colorAttachmentRefs0[0] = {
				lightingRenderPass.AddAttachment(attachments[i]),
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
			};
			++i;
			
			// Input attachments
			for (auto* img : gBuffers) {
				images[i] = img;
				attachments[i].format = img->format;
				attachments[i].samples = VK_SAMPLE_COUNT_1_BIT;
				attachments[i].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
				attachments[i].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
				attachments[i].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
				attachments[i].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
				attachments[i].initialLayout = VK_IMAGE_LAYOUT_GENERAL;
				attachments[i].finalLayout = VK_IMAGE_LAYOUT_GENERAL;
				inputAttachmentRefs0[i-nbColorAttachments] = {
					lightingRenderPass.AddAttachment(attachments[i]),
					VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
				};
				++i;
			}
			
			std::array<VkAttachmentReference, nbColorAttachments> colorAttachmentRefs1 = colorAttachmentRefs0;
			std::array<VkAttachmentReference, nbInputAttachments> inputAttachmentRefs1 = inputAttachmentRefs0;
		
			// SubPasses
			std::array<VkSubpassDependency, 2> subpassDependencies {
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
			};
			std::array<VkSubpassDescription, 2> subpasses {};
				subpasses[0] = {};
				subpasses[0].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
				subpasses[0].colorAttachmentCount = colorAttachmentRefs0.size();
				subpasses[0].pColorAttachments = colorAttachmentRefs0.data();
				subpasses[0].inputAttachmentCount = inputAttachmentRefs0.size();
				subpasses[0].pInputAttachments = inputAttachmentRefs0.data();
				subpasses[1] = {};
				subpasses[1].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
				subpasses[1].colorAttachmentCount = colorAttachmentRefs1.size();
				subpasses[1].pColorAttachments = colorAttachmentRefs1.data();
				subpasses[1].inputAttachmentCount = inputAttachmentRefs1.size();
				subpasses[1].pInputAttachments = inputAttachmentRefs1.data();
			lightingRenderPass.AddSubpass(subpasses[0]);
			lightingRenderPass.AddSubpass(subpasses[1]);
			lightingRenderPass.renderPassInfo.dependencyCount = subpassDependencies.size();
			lightingRenderPass.renderPassInfo.pDependencies = subpassDependencies.data();
			
			// Create the render pass
			lightingRenderPass.Create(renderingDevice);
			lightingRenderPass.CreateFrameBuffers(renderingDevice, images);
			
			// Shaders
			for (auto* s : rasterShaders["lighting_0"]) {
				s->SetRenderPass(&litImage, lightingRenderPass.handle, 0);
				s->AddColorBlendAttachmentState();
				s->CreatePipeline(renderingDevice);
			}
			for (auto* s : rasterShaders["lighting_1"]) {
				s->SetRenderPass(&litImage, lightingRenderPass.handle, 1);
				s->AddColorBlendAttachmentState();
				s->CreatePipeline(renderingDevice);
			}
		}
		
		// lightingComputeShader.CreatePipeline(renderingDevice);
	}
	void DestroyLightingPipeline() {
		for (auto[rp, ss] : rasterShaders) if (rp.substr(0, 8) == "lighting") {
			for (auto* s : ss) {
				s->DestroyPipeline(renderingDevice);
			}
		}
		lightingRenderPass.DestroyFrameBuffers(renderingDevice);
		lightingRenderPass.Destroy(renderingDevice);
		
		// lightingComputeShader.DestroyPipeline(renderingDevice);
	}
	
	void ConfigureLightingShaders() {
		for (auto[rp, ss] : rasterShaders) if (rp.substr(0, 8) == "lighting") {
			for (auto* s : ss) {
				s->inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
				s->depthStencilState.depthTestEnable = VK_FALSE;
				s->depthStencilState.depthWriteEnable = VK_FALSE;
				s->rasterizer.cullMode = VK_CULL_MODE_FRONT_BIT;
				s->SetData(3);
			}
		}
	}
	
	void ClearLitImage(VkCommandBuffer commandBuffer) {
		TransitionImageLayout(commandBuffer, litImage, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
			const VkClearColorValue clearValues = {0,0,0,0};
			VkImageSubresourceRange range {VK_IMAGE_ASPECT_COLOR_BIT, 0,1,0,1};
			renderingDevice->CmdClearColorImage(commandBuffer, litImage.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearValues, 1, &range);
		TransitionImageLayout(commandBuffer, litImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL);
	}
	
	void RunLighting(Device* device, VkCommandBuffer commandBuffer, bool runSubpass0, bool runSubpass1) {
		if (!runSubpass0 && !runSubpass1) return;
		lightingRenderPass.Begin(device, commandBuffer, litImage);
			if (runSubpass0) {
				for (auto* s : rasterShaders["lighting_0"]) {
					s->Execute(device, commandBuffer);
				}
			}
			renderingDevice->CmdNextSubpass(commandBuffer, VK_SUBPASS_CONTENTS_INLINE);
			if (runSubpass1) {
				for (auto* s : rasterShaders["lighting_1"]) {
					s->Execute(device, commandBuffer);
				}
			}
		lightingRenderPass.End(device, commandBuffer);
	}
	
private: // Raster Visibility
	RenderPass visibilityRasterPass;
	PipelineLayout visibilityRasterLayout;
	
	RasterShaderPipeline visibilityRasterShader {visibilityRasterLayout, {
		"incubator_rendering/assets/shaders/raster.visibility.vert",
		"incubator_rendering/assets/shaders/raster.visibility.frag",
	}};
	
	#ifdef _DEBUG
		RasterShaderPipeline debugRasterShader {visibilityRasterLayout, {
			"incubator_rendering/assets/shaders/raster.visibility.vert",
			"incubator_rendering/assets/shaders/raster.visibility.frag",
		}};
	#endif
	
	struct GeometryPushConstant {
		uint objectIndex;
		uint geometryIndex;
	};

	Image gBuffer_albedo_geometryIndex { VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT ,1,1, { VK_FORMAT_R32G32_SFLOAT }}; // r = albedo, g = geometry index
	Image gBuffer_normal_uv { VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT ,1,1, { VK_FORMAT_R32G32B32A32_SFLOAT }}; // rgb = normal xyz,  a = uv
	Image gBuffer_position_dist { VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT ,1,1, { VK_FORMAT_R32G32B32A32_SFLOAT }}; // rgb = position xyz,  a = trueDistanceFromCamera
	
	static const int NB_G_BUFFERS = 3;
	std::array<Image*, NB_G_BUFFERS> gBuffers {
		&gBuffer_albedo_geometryIndex,// attachment 0
		&gBuffer_normal_uv, 	// attachment 1
		&gBuffer_position_dist, // attachment 2
	};
	DepthImage rasterDepthImage { VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT };
	
	void CreateRasterVisibilityResources() {
		uint rasterWidth = (uint)((float)swapChain->extent.width);
		uint rasterHeight = (uint)((float)swapChain->extent.height);
		
		for (auto* img : gBuffers)
			img->Create(renderingDevice, rasterWidth, rasterHeight);
		for (auto* img : gBuffers)
			TransitionImageLayout(*img, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
			
		rasterDepthImage.viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
		rasterDepthImage.Create(renderingDevice, rasterWidth, rasterHeight);
		TransitionImageLayout(rasterDepthImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
	}
	void DestroyRasterVisibilityResources() {
		for (auto* img : gBuffers)
			img->Destroy(renderingDevice);
		rasterDepthImage.Destroy(renderingDevice);
	}
	
	void CreateRasterVisibilityPipeline() {
		
		{// Opaque Raster Visibility pass
			std::array<Image*, NB_G_BUFFERS+1> images {};
			std::array<VkAttachmentDescription, NB_G_BUFFERS+1> attachments {};
			std::array<VkAttachmentReference, NB_G_BUFFERS> colorAttachmentRefs {};
			VkAttachmentReference depthAttachmentRef;
			int i = 0;
			// G-Buffers
			for (auto* img : gBuffers) {
				images[i] = img;
				attachments[i].format = img->format;
				attachments[i].samples = VK_SAMPLE_COUNT_1_BIT;
				attachments[i].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
				attachments[i].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
				attachments[i].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
				attachments[i].finalLayout = VK_IMAGE_LAYOUT_GENERAL;
				colorAttachmentRefs[i] = {
					visibilityRasterPass.AddAttachment(attachments[i]),
					VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
				};
				++i;
			}
			{ // Depth buffer
				images[i] = &rasterDepthImage;
				attachments[i].format = rasterDepthImage.format;
				attachments[i].samples = VK_SAMPLE_COUNT_1_BIT;
				attachments[i].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
				attachments[i].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
				attachments[i].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
				attachments[i].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
				attachments[i].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
				attachments[i].finalLayout = VK_IMAGE_LAYOUT_GENERAL;
				depthAttachmentRef = {
					visibilityRasterPass.AddAttachment(attachments[i]),
					VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
				};
				++i;
			}
			
			// SubPass
			VkSubpassDescription subpass = {};
				subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
				subpass.colorAttachmentCount = colorAttachmentRefs.size();
				subpass.pColorAttachments = colorAttachmentRefs.data();
				subpass.pDepthStencilAttachment = &depthAttachmentRef;
			visibilityRasterPass.AddSubpass(subpass);
			
			// Create the render pass
			visibilityRasterPass.Create(renderingDevice);
			visibilityRasterPass.CreateFrameBuffers(renderingDevice, images.data(), images.size());
			
			// Shaders
			for (auto* s : rasterShaders["visibility"]) {
				s->SetRenderPass(*images.data(), visibilityRasterPass.handle, 0);
				for (int i = 0; i < colorAttachmentRefs.size(); ++i)
					s->AddColorBlendAttachmentState(VK_FALSE);
				s->CreatePipeline(renderingDevice);
			}
		}
		
	}
	
	void DestroyRasterVisibilityPipeline() {
		
		for (auto* s : rasterShaders["visibility"]) {
			s->DestroyPipeline(renderingDevice);
		}
		visibilityRasterPass.DestroyFrameBuffers(renderingDevice);
		visibilityRasterPass.Destroy(renderingDevice);
		
	}
	
	void ConfigureRasterVisibilityShaders() {
		visibilityRasterLayout.AddPushConstant<GeometryPushConstant>(VK_SHADER_STAGE_ALL_GRAPHICS);
		
		visibilityRasterShader.AddVertexInputBinding(sizeof(Geometry::VertexBuffer_T), VK_VERTEX_INPUT_RATE_VERTEX, Geometry::VertexBuffer_T::GetInputAttributes());
		#ifdef V4D_RENDERER_RAYTRACING_USE_DEVICE_LOCAL_VERTEX_INDEX_BUFFERS
			visibilityRasterShader.SetData(&Geometry::globalBuffers.vertexBuffer.deviceLocalBuffer, &Geometry::globalBuffers.indexBuffer.deviceLocalBuffer);
		#else
			visibilityRasterShader.SetData(&Geometry::globalBuffers.vertexBuffer, &Geometry::globalBuffers.indexBuffer);
		#endif
		
		#ifdef _DEBUG
			debugRasterShader.AddVertexInputBinding(sizeof(Geometry::VertexBuffer_T), VK_VERTEX_INPUT_RATE_VERTEX, Geometry::VertexBuffer_T::GetInputAttributes());
			debugRasterShader.rasterizer.polygonMode = VK_POLYGON_MODE_LINE;
			debugRasterShader.rasterizer.lineWidth = 1;
			debugRasterShader.rasterizer.cullMode = VK_CULL_MODE_NONE;
			#ifdef V4D_RENDERER_RAYTRACING_USE_DEVICE_LOCAL_VERTEX_INDEX_BUFFERS
				debugRasterShader.SetData(&Geometry::globalBuffers.vertexBuffer.deviceLocalBuffer, &Geometry::globalBuffers.indexBuffer.deviceLocalBuffer);
			#else
				debugRasterShader.SetData(&Geometry::globalBuffers.vertexBuffer, &Geometry::globalBuffers.indexBuffer);
			#endif
		#endif
	}
	
	void RunRasterVisibility(Device* device, VkCommandBuffer commandBuffer) {
		
		std::vector<VkClearValue> clearValues(NB_G_BUFFERS+1);
		int i = 0;
		for (; i < NB_G_BUFFERS; ++i)
			clearValues[i] = {.0,.0,.0,.0};
		clearValues[i] = {0.0f,0}; // depth
		
		// Visibility Raster pass
		visibilityRasterPass.Begin(renderingDevice, commandBuffer, **gBuffers.data(), clearValues);
			for (auto* s : rasterShaders["visibility"]) {
				if (
					s == &visibilityRasterShader
					#ifdef _DEBUG
						|| s == &debugRasterShader
					#endif
				) {
					#ifdef _DEBUG
						if (scene.camera.debug != (s == &debugRasterShader)) {
							continue;
						}
					#endif
					scene.Lock();
						for (auto* obj : scene.objectInstances) {
							if (obj) {
								// obj->Lock();
								if (obj->IsActive()) {
									// Geometries
									for (auto& geom : obj->GetGeometries()) {
										if (geom.geometry->active) {
											// Frustum culling
											if (scene.camera.IsVisibleInScreen((obj->GetWorldTransform() * glm::dmat4(geom.transform))[3], geom.geometry->boundingDistance)) {
												if (geom.geometry->isProcedural) {
													//TODO procedural objects
												} else {
													s->vertexOffset = geom.geometry->vertexOffset * sizeof(Geometry::VertexBuffer_T);
													s->indexOffset = geom.geometry->indexOffset * sizeof(Geometry::IndexBuffer_T);
													s->indexCount = geom.geometry->indexCount;
													GeometryPushConstant geometryPushConstant {};
														geometryPushConstant.objectIndex = obj->GetObjectOffset();
														geometryPushConstant.geometryIndex = geom.geometry->geometryOffset;
													s->Execute(device, commandBuffer, 1, &geometryPushConstant);
												}
											}
										}
									}
								}
								// obj->Unlock();
							}
						}
					scene.Unlock();
				} else {
					s->Execute(device, commandBuffer);
				}
			}
		visibilityRasterPass.End(device, commandBuffer);
		
	}
	
private: // Ray Tracing
	VkPhysicalDeviceRayTracingPropertiesKHR rayTracingProperties{};
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
	std::map<int/*instance index*/, std::shared_ptr<Geometry>> activeRayTracedGeometries {};
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
		
		activeRayTracedGeometries.clear();
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
	
	void RunRayTracingCommands(VkCommandBuffer commandBuffer) {
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
	
	void SetRayTracingInstanceTransform(GeometryInstance* geometryInstance, const glm::dmat4& objectViewTransform) {
		std::lock_guard lock(rayTracingInstanceMutex);
		if (geometryInstance->rayTracingInstanceIndex == -1) return;
		rayTracingInstances[geometryInstance->rayTracingInstanceIndex].transform = glm::transpose(glm::mat4(objectViewTransform * geometryInstance->transform));
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
		activeRayTracedGeometries[geometryInstance->rayTracingInstanceIndex] = geometryInstance->geometry;
	}
	
	void RemoveRayTracingInstance(GeometryInstance* geometryInstance) {
		std::lock_guard lock(rayTracingInstanceMutex);
		if (geometryInstance->rayTracingInstanceIndex == -1) return;
		int lastIndex = --nbRayTracingInstances;
		int index = geometryInstance->rayTracingInstanceIndex;
		geometryInstance->rayTracingInstanceIndex = -1;
		rayTracingInstances[index] = rayTracingInstances[lastIndex];
		rayTracingInstances[lastIndex].accelerationStructureHandle = 0;
	
		// inactiveBlas.push_back(activeRayTracedGeometries[index]->blas);
		activeRayTracedGeometries[index] = activeRayTracedGeometries[lastIndex];
		activeRayTracedGeometries[lastIndex] = nullptr;
		
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
	RenderPass postProcessingRenderPass, thumbnailRenderPass;
	PipelineLayout postProcessingPipelineLayout, thumbnailPipelineLayout, histogramComputeLayout;
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
	float exposureFactor = 1.0;
	
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
	std::unordered_map<std::string, PipelineLayout*> pipelineLayouts {
		{"visibility", &visibilityRasterLayout},
		{"rayTracing", &rayTracingPipelineLayout},
		{"lighting", &lightingPipelineLayout},
		{"thumbnail", &thumbnailPipelineLayout},
		{"ui", &uiPipelineLayout},
		{"postProcessing", &postProcessingPipelineLayout},
		{"histogram", &histogramComputeLayout},
	};

	std::unordered_map<std::string, std::vector<RasterShaderPipeline*>> rasterShaders {
		/* RenderPass_SubPass => ShadersList */
		{"visibility", {
			&visibilityRasterShader,
			#ifdef _DEBUG
				&debugRasterShader,
			#endif
		}},
		{"lighting_0", {&lightingShader}},
		{"lighting_1", {}},
		{"thumbnail", {&thumbnailShader}},
		{"postProcessing_0", {&postProcessingShader_txaa}},
		{"postProcessing_1", {&postProcessingShader_history}},
		{"postProcessing_2", {&postProcessingShader_hdr, &postProcessingShader_ui}},
		{"ui", {}},
	};
	
	std::vector<ComputeShaderPipeline*> computeShaders {
		&histogramComputeShader,
		// &lightingComputeShader,
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
		activeRayTracedGeometries.clear();
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
		
		// Higher score for Ray Tracing support
		if (physicalDevice->GetRayTracingFeatures().rayTracing) {
			score += 2;
		}
		if (physicalDevice->GetRayTracingFeatures().rayQuery) {
			score += 1;
		}
	}
	void Init() override {
		// Device Extensions
		OptionalDeviceExtension(VK_KHR_RAY_TRACING_EXTENSION_NAME); // RayTracing extension
		OptionalDeviceExtension(VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME); // Needed for RayTracing extension
		OptionalDeviceExtension(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME); // Needed for RayTracing extension
		OptionalDeviceExtension(VK_KHR_PIPELINE_LIBRARY_EXTENSION_NAME); // Needed for RayTracing extension
		
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
	void InitDeviceFeatures() {
		deviceFeatures.shaderFloat64 = VK_TRUE;
		deviceFeatures.depthClamp = VK_TRUE;
		deviceFeatures.fillModeNonSolid = VK_TRUE;
		EnableVulkan12DeviceFeatures()->bufferDeviceAddress = VK_TRUE;
		EnableRayTracingFeatures()->rayTracing = VK_TRUE;
		EnableRayTracingFeatures()->rayQuery = VK_TRUE;
		
		for (auto* submodule : renderingSubmodules) {
			submodule->InitDeviceFeatures();
		}
	}
	void Info() override {
		if (rayTracingFeatures.rayTracing) {
			LOG_SUCCESS("Ray-Tracing Supported")
			// Query the ray tracing properties of the current implementation
			rayTracingProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PROPERTIES_KHR;
			VkPhysicalDeviceProperties2 deviceProps2 {};
				deviceProps2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
				deviceProps2.pNext = &rayTracingProperties;
			GetPhysicalDeviceProperties2(renderingDevice->GetPhysicalDevice()->GetHandle(), &deviceProps2);
		} else {
			// No Ray-Tracing support
			LOG_WARN("Ray-Tracing unavailable, using rasterization")
			scene.camera.renderMode = rasterization;
			scene.camera.shadows = shadows_off;
		}
		
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
			baseDescriptorSet_0.AddBinding_uniformBuffer(0, &cameraUniformBuffer.deviceLocalBuffer, VK_SHADER_STAGE_ALL_GRAPHICS | VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_INTERSECTION_BIT_KHR | VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR);
			baseDescriptorSet_0.AddBinding_storageBuffer(1, &Geometry::globalBuffers.objectBuffer.deviceLocalBuffer, VK_SHADER_STAGE_ALL_GRAPHICS | VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_INTERSECTION_BIT_KHR | VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR);
			baseDescriptorSet_0.AddBinding_storageBuffer(2, &Geometry::globalBuffers.lightBuffer.deviceLocalBuffer, VK_SHADER_STAGE_ALL_GRAPHICS | VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR);
			baseDescriptorSet_0.AddBinding_storageBuffer(3, &activeLightsUniformBuffer.deviceLocalBuffer, VK_SHADER_STAGE_ALL_GRAPHICS | VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR);
			baseDescriptorSet_0.AddBinding_storageBuffer(4, &Geometry::globalBuffers.geometryBuffer.deviceLocalBuffer, VK_SHADER_STAGE_ALL_GRAPHICS | VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_INTERSECTION_BIT_KHR | VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR);
			#ifdef V4D_RENDERER_RAYTRACING_USE_DEVICE_LOCAL_VERTEX_INDEX_BUFFERS
				baseDescriptorSet_0.AddBinding_storageBuffer(5, &Geometry::globalBuffers.indexBuffer.deviceLocalBuffer, VK_SHADER_STAGE_ALL_GRAPHICS | VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR | VK_SHADER_STAGE_COMPUTE_BIT);
				baseDescriptorSet_0.AddBinding_storageBuffer(6, &Geometry::globalBuffers.vertexBuffer.deviceLocalBuffer, VK_SHADER_STAGE_ALL_GRAPHICS | VK_SHADER_STAGE_INTERSECTION_BIT_KHR | VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR | VK_SHADER_STAGE_COMPUTE_BIT);
			#else
				baseDescriptorSet_0.AddBinding_storageBuffer(5, &Geometry::globalBuffers.indexBuffer, VK_SHADER_STAGE_ALL_GRAPHICS | VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR | VK_SHADER_STAGE_COMPUTE_BIT);
				baseDescriptorSet_0.AddBinding_storageBuffer(6, &Geometry::globalBuffers.vertexBuffer, VK_SHADER_STAGE_ALL_GRAPHICS | VK_SHADER_STAGE_INTERSECTION_BIT_KHR | VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR | VK_SHADER_STAGE_COMPUTE_BIT);
			#endif
			
		descriptorSets["1_visibility"] = &visibilityDescriptorSet_1;
			//...
			
		descriptorSets["1_rayTracing"] = &rayTracingDescriptorSet_1;
			rayTracingDescriptorSet_1.AddBinding_accelerationStructure(0, &topLevelAccelerationStructure.accelerationStructure, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR);
			rayTracingDescriptorSet_1.AddBinding_imageView(1, &litImage, VK_SHADER_STAGE_RAYGEN_BIT_KHR);
			rayTracingDescriptorSet_1.AddBinding_imageView(2, &depthImage, VK_SHADER_STAGE_RAYGEN_BIT_KHR);
			{
				int i = 3;
				for (auto* img : gBuffers) rayTracingDescriptorSet_1.AddBinding_imageView(i++, img, VK_SHADER_STAGE_RAYGEN_BIT_KHR);
			}
		
		descriptorSets["1_lighting"] = &lightingDescriptorSet_1;
		{
			int i = 0;
			for (auto* img : gBuffers) lightingDescriptorSet_1.AddBinding_inputAttachment(i++, img, VK_SHADER_STAGE_FRAGMENT_BIT);
		}
		
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
			postProcessingDescriptorSet_1.AddBinding_combinedImageSampler(5, &rasterDepthImage, VK_SHADER_STAGE_FRAGMENT_BIT);
		
		descriptorSets["1_histogram"] = &histogramDescriptorSet_1;
			histogramDescriptorSet_1.AddBinding_imageView(0, &thumbnailImage, VK_SHADER_STAGE_COMPUTE_BIT);
			histogramDescriptorSet_1.AddBinding_storageBuffer(1, &totalLuminance, VK_SHADER_STAGE_COMPUTE_BIT);
		
		// Add the same set 0 to all pipeline layouts
		for (auto[name,layout] : pipelineLayouts) {
			layout->AddDescriptorSet(&baseDescriptorSet_0);
		}
		// Add specific set 1 to specific layout lists
		pipelineLayouts["visibility"]->AddDescriptorSet(&visibilityDescriptorSet_1);
		pipelineLayouts["rayTracing"]->AddDescriptorSet(&rayTracingDescriptorSet_1);
		pipelineLayouts["lighting"]->AddDescriptorSet(&lightingDescriptorSet_1);
		pipelineLayouts["thumbnail"]->AddDescriptorSet(&thumbnailDescriptorSet_1);
		pipelineLayouts["ui"]->AddDescriptorSet(&uiDescriptorSet_1);
		pipelineLayouts["postProcessing"]->AddDescriptorSet(&postProcessingDescriptorSet_1);
		pipelineLayouts["histogram"]->AddDescriptorSet(&histogramDescriptorSet_1);
		
		// Submodules
		for (auto* submodule : renderingSubmodules) {
			submodule->InitLayouts(descriptorSets, images, pipelineLayouts);
		}
	}
	
	void ConfigureShaders() override {
		ConfigureRasterVisibilityShaders();
		ConfigureRayTracingShaders();
		ConfigureLightingShaders();
		ConfigurePostProcessingShaders();
		ConfigureUiShaders();
		
		// Submodules
		for (auto* submodule : renderingSubmodules) {
			submodule->ConfigureShaders(rasterShaders, &shaderBindingTable, pipelineLayouts);
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
		
		scene.camera.renderMode = DEFAULT_RENDER_MODE;
		scene.camera.shadows = DEFAULT_SHADOW_TYPE;
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
		if (rayTracingFeatures.rayTracing) CreateRayTracingResources();
		CreateRasterVisibilityResources();
		
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
		if (rayTracingFeatures.rayTracing) DestroyRayTracingResources();
		DestroyRasterVisibilityResources();
		
		// Submodules
		for (auto* submodule : renderingSubmodules) {
			submodule->DestroyResources();
		}
	}
	
	/*
	#define TEST_BUFFER_ALLOC(type, size) {\
		auto timer = v4d::Timer(true);\
		Buffer testBuffer {VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT, size_t(size)*1024*1024};\
		testBuffer.Allocate(renderingDevice, VK_MEMORY_PROPERTY_ ## type ## _BIT);\
		double allocTime = timer.GetElapsedMilliseconds();\
		timer.Reset();\
		testBuffer.Free(renderingDevice);\
		LOG("" << # type << " " << size << " mb Allocated in " << allocTime << " ms, Freed in " << timer.GetElapsedMilliseconds() << " ms")\
	}
	*/
	
	void AllocateBuffers() override {
		AllocateSceneBuffers();
		AllocatePostProcessingBuffers();
		if (rayTracingFeatures.rayTracing) AllocateRayTracingBuffers();
		
		// Submodules
		for (auto* submodule : renderingSubmodules) {
			submodule->AllocateBuffers();
		}
		
		Geometry::globalBuffers.DefragmentMemory();
		Geometry::globalBuffers.Allocate(renderingDevice, {lowPriorityComputeQueue.familyIndex, graphicsQueue.familyIndex});
		
		// TEST_BUFFER_ALLOC(HOST_COHERENT, 512)
		// TEST_BUFFER_ALLOC(HOST_COHERENT, 1024)
		// TEST_BUFFER_ALLOC(HOST_COHERENT, 2048)
		// TEST_BUFFER_ALLOC(HOST_CACHED, 512)
		// TEST_BUFFER_ALLOC(HOST_CACHED, 1024)
		// TEST_BUFFER_ALLOC(HOST_CACHED, 2048)
		// TEST_BUFFER_ALLOC(DEVICE_LOCAL, 512)
		// TEST_BUFFER_ALLOC(DEVICE_LOCAL, 1024)
		// TEST_BUFFER_ALLOC(DEVICE_LOCAL, 2048)
	}
	void FreeBuffers() override {
		FreeSceneBuffers();
		FreePostProcessingBuffers();
		if (rayTracingFeatures.rayTracing) FreeRayTracingBuffers();
		
		// Submodules
		for (auto* submodule : renderingSubmodules) {
			submodule->FreeBuffers();
		}
		
		Geometry::globalBuffers.Free(renderingDevice);
	}

private: // Graphics configuration overrides
	void CreatePipelines() override {
		// Pipeline layouts
		for (auto[name,layout] : pipelineLayouts) {
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
		
		// Visibility
		CreateRasterVisibilityPipeline();
		
		// Ray Tracing
		if (rayTracingFeatures.rayTracing) CreateRayTracingPipeline();
		
		// Lighting
		CreateLightingPipeline();
		
		// Post Processing
		CreatePostProcessingPipeline();
	}
	void DestroyPipelines() override {
		// UI
		#ifdef _ENABLE_IMGUI
			UnloadImGui();
		#endif
		DestroyUiPipeline();
		
		// Visibility
		DestroyRasterVisibilityPipeline();
		
		// Ray Tracing
		if (rayTracingFeatures.rayTracing) DestroyRayTracingPipeline();
		
		// Lighting
		DestroyLightingPipeline();
		
		// Post Processing
		DestroyPostProcessingPipeline();
		
		// Submodules
		for (auto* submodule : renderingSubmodules) {
			submodule->DestroyPipelines();
		}
		
		// Pipeline layouts
		for (auto[name,layout] : pipelineLayouts) {
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
		
		// Ray Tracing
		size_t globalScratchBufferSize = 0;
		if (rayTracingFeatures.rayTracing) {
			topLevelAccelerationStructure.GetMemoryRequirementsForScratchBuffer(renderingDevice);
		}
		if (rayTracingFeatures.rayTracing) {
			ResetRayTracingBlasBuilds();
			inactiveBlas.clear();
		}
		
		scene.CollectGarbage();
		
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
						if (rayTracingFeatures.rayTracing) {
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
										SetRayTracingInstanceTransform(&geom, scene.camera.viewMatrix * obj->GetWorldTransform());
									}
								} else if (geom.rayTracingInstanceIndex != -1) {
									RemoveRayTracingInstance(&geom);
								}
							}
						}
					} else if (rayTracingFeatures.rayTracing) {
						for (auto& geom : obj->GetGeometries()) if (geom.rayTracingInstanceIndex != -1) {
							RemoveRayTracingInstance(&geom);
						}
					}
					// obj->Unlock();
				}
			}
		scene.Unlock();
		
		// Global Scratch Buffer
		if (rayTracingFeatures.rayTracing && AccelerationStructure::useGlobalScratchBuffer && globalScratchDynamicSize) {
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
		ClearLitImage(commandBuffer);
		
		// Transfer data to rendering device
		cameraUniformBuffer.Update(renderingDevice, commandBuffer);
		activeLightsUniformBuffer.Update(renderingDevice, commandBuffer, sizeof(uint32_t)/*lightCount*/ + sizeof(uint32_t)*nbActiveLights/*lightIndices*/);
		Geometry::globalBuffers.PushObjects(renderingDevice, commandBuffer);
		Geometry::globalBuffers.PushLights(renderingDevice, commandBuffer);
		if (rayTracingFeatures.rayTracing) {
			for (auto[i,geometry] : activeRayTracedGeometries) {
				if (geometry) geometry->AutoPush(renderingDevice, commandBuffer, true);
			}
		} else {
			// Geometry::globalBuffers.PushGeometriesInfo(renderingDevice, commandBuffer);
			scene.Lock();
				for (auto* obj : scene.objectInstances) if (obj && obj->IsActive()) {
					for (auto& geom : obj->GetGeometries()) if (geom.geometry->active) {
						if (geom.geometry) geom.geometry->AutoPush(renderingDevice, commandBuffer, true);
					}
				}
			scene.Unlock();
		}
		
		//TODO memory barrier between transfer and vertex shaders ?
	
		// Submodules
		for (auto* submodule : renderingSubmodules) {
			submodule->RunDynamicGraphicsTop(commandBuffer, images);
		}
	
		// Acceleration Structures
		if (rayTracingFeatures.rayTracing) {
			BuildBottomLevelRayTracingAccelerationStructures(renderingDevice, commandBuffer);
			BuildTopLevelRayTracingAccelerationStructure(renderingDevice, commandBuffer);
		}
		
		// Submodules
		for (auto* submodule : renderingSubmodules) {
			submodule->RunDynamicGraphicsMiddle(commandBuffer, images);
		}
		
		if (scene.camera.renderMode == rasterization || scene.camera.debug) {
			// Raster Visibility and lighting
			RunRasterVisibility(renderingDevice, commandBuffer);
			RunLighting(renderingDevice, commandBuffer, true, !scene.camera.debug);
		} else if (rayTracingFeatures.rayTracing) {
			// Ray Tracing lighting
			RunRayTracingCommands(commandBuffer);
			RunLighting(renderingDevice, commandBuffer, false, !scene.camera.debug);
		}
		
		// Submodules
		for (auto* submodule : renderingSubmodules) {
			submodule->RunDynamicGraphicsBottom(commandBuffer, images);
		}
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
			ImGui::SetNextWindowSize({405, 344});
			ImGui::Begin("Settings and Modules");
			#ifdef _DEBUG
				ImGui::Checkbox("Debug", &scene.camera.debug);
			#endif
			if (scene.camera.debug) {
				// ...
			} else {
				if (rayTracingFeatures.rayTracing) {
					ImGui::Combo("Rendering Mode", &scene.camera.renderMode, RENDER_MODES_STR);
				} else {
					ImGui::Text("Ray-Tracing unavailable, using rasterization");
				}
				if (scene.camera.renderMode > 0) {
					ImGui::Combo("Shadows", &scene.camera.shadows, SHADOW_TYPES_STR);
				} else {
					ImGui::Text("Shadows disabled");
				}
				ImGui::Checkbox("TXAA", &scene.camera.txaa);
				ImGui::Checkbox("Gamma correction", &scene.camera.gammaCorrection);
				ImGui::Checkbox("HDR Tone Mapping", &scene.camera.hdr);
				ImGui::SliderFloat("HDR Exposure", &exposureFactor, 0, 10);
				ImGui::SliderFloat("brightness", &scene.camera.brightness, 0, 2);
				ImGui::SliderFloat("contrast", &scene.camera.contrast, 0, 2);
				ImGui::SliderFloat("gamma", &scene.camera.gamma, 0, 5);
			}
			for (auto* submodule : renderingSubmodules) {
				ImGui::Separator();
				submodule->RunImGui();
			}
			ImGui::End();
		}
	#endif
};
