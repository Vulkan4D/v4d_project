#pragma once
#include <v4d.h>

#include "RenderTargetGroup.hpp"

using namespace v4d::graphics;
// using namespace v4d::graphics::vulkan::rtx;

class V4DRenderer : public v4d::graphics::Renderer {
private: 
	using v4d::graphics::Renderer::Renderer;

	std::vector<v4d::modules::Rendering*> renderingSubmodules {};
	
	#pragma region Buffers
	StagedBuffer cameraUniformBuffer {VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, sizeof(Camera)};
	#pragma endregion
	
	#pragma region UI
	float uiImageScale = 1.0;
	Image uiImage { VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT ,1,1, { VK_FORMAT_R8G8B8A8_SNORM }};
	#pragma endregion
	
	#pragma region Render passes
	RenderPass	opaqueRasterPass,
				opaqueLightingPass,
				transparentRasterPass,
				transparentLightingPass,
				thumbnailRenderPass,
				postProcessingRenderPass,
				uiRenderPass;
	#pragma endregion
	
	#pragma region Shaders
	
	PipelineLayout lightingPipelineLayout, thumbnailPipelineLayout, postProcessingPipelineLayout, uiPipelineLayout;
	RasterShaderPipeline opaqueLightingShader {lightingPipelineLayout, {
		"incubator_rendering/assets/shaders/v4d_lighting.vert",
		"incubator_rendering/assets/shaders/v4d_lighting.opaque.frag",
	}};
	RasterShaderPipeline transparentLightingShader {lightingPipelineLayout, {
		"incubator_rendering/assets/shaders/v4d_lighting.vert",
		"incubator_rendering/assets/shaders/v4d_lighting.transparent.frag",
	}};
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
	
	std::unordered_map<std::string, std::vector<RasterShaderPipeline*>> shaders {
		/* RenderPass_SubPass => ShadersList */
		{"opaqueRasterization", {}},
		{"transparentRasterization", {}},
		{"opaqueLighting", {&opaqueLightingShader}},
		{"transparentLighting", {&transparentLightingShader}},
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
		{"gBuffer_albedo", &renderTargetGroup.GetGBuffer(0)},
		{"gBuffer_normal", &renderTargetGroup.GetGBuffer(1)},
		{"gBuffer_roughness", &renderTargetGroup.GetGBuffer(2)},
		{"gBuffer_metallic", &renderTargetGroup.GetGBuffer(3)},
		{"gBuffer_scatter", &renderTargetGroup.GetGBuffer(4)},
		{"gBuffer_occlusion", &renderTargetGroup.GetGBuffer(5)},
		{"gBuffer_emission", &renderTargetGroup.GetGBuffer(6)},
		{"gBuffer_position", &renderTargetGroup.GetGBuffer(7)},
		{"litImage", &renderTargetGroup.GetLitImage()},
		{"thumbnail", &renderTargetGroup.GetThumbnailImage()},
		{"historyImage", &renderTargetGroup.GetHistoryImage()},
		{"ui", &uiImage},
	};
	
private: // Init
	void Init() override {
		cameraUniformBuffer.AddSrcDataPtr(&scene.camera, sizeof(Camera));
		
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
		baseDescriptorSet_0->AddBinding_uniformBuffer(0, &cameraUniformBuffer.deviceLocalBuffer, VK_SHADER_STAGE_ALL_GRAPHICS);
		
		// Lighting pass
		auto* gBuffersDescriptorSet_1 = descriptorSets.emplace_back(new DescriptorSet(1));
		for (int i = 0; i < RenderTargetGroup::GBUFFER_NB_IMAGES; ++i)
			gBuffersDescriptorSet_1->AddBinding_inputAttachment(i, &renderTargetGroup.GetGBuffer(i).view, VK_SHADER_STAGE_FRAGMENT_BIT);
		lightingPipelineLayout.AddDescriptorSet(baseDescriptorSet_0);
		lightingPipelineLayout.AddDescriptorSet(gBuffersDescriptorSet_1);
		lightingPipelineLayout.AddPushConstant<LightSource>(VK_SHADER_STAGE_FRAGMENT_BIT);
		
		// Thumbnail Gen
		auto* thumbnailDescriptorSet_1 = descriptorSets.emplace_back(new DescriptorSet(1));
		thumbnailDescriptorSet_1->AddBinding_combinedImageSampler(0, &renderTargetGroup.GetLitImage(), VK_SHADER_STAGE_FRAGMENT_BIT);
		thumbnailPipelineLayout.AddDescriptorSet(baseDescriptorSet_0);
		thumbnailPipelineLayout.AddDescriptorSet(thumbnailDescriptorSet_1);
		
		// Post-Processing
		auto* postProcessingDescriptorSet_1 = descriptorSets.emplace_back(new DescriptorSet(1));
		postProcessingDescriptorSet_1->AddBinding_combinedImageSampler(0, &renderTargetGroup.GetLitImage(), VK_SHADER_STAGE_FRAGMENT_BIT);
		postProcessingDescriptorSet_1->AddBinding_combinedImageSampler(1, &renderTargetGroup.GetDepthImage(), VK_SHADER_STAGE_FRAGMENT_BIT);
		postProcessingDescriptorSet_1->AddBinding_combinedImageSampler(2, &renderTargetGroup.GetHistoryImage(), VK_SHADER_STAGE_FRAGMENT_BIT);
		postProcessingDescriptorSet_1->AddBinding_combinedImageSampler(3, &uiImage, VK_SHADER_STAGE_FRAGMENT_BIT);
		postProcessingDescriptorSet_1->AddBinding_inputAttachment(4, &renderTargetGroup.GetPpImage().view, VK_SHADER_STAGE_FRAGMENT_BIT);
		postProcessingPipelineLayout.AddDescriptorSet(baseDescriptorSet_0);
		postProcessingPipelineLayout.AddDescriptorSet(postProcessingDescriptorSet_1);
		
		// UI
		//TODO uiPipelineLayout
		
		// Submodules
		for (auto* submodule : renderingSubmodules) {
			submodule->InitLayouts(descriptorSets, images);
		}
	}
	
	void ConfigureShaders() override {
		
		// Lighting Passes
		opaqueLightingShader.inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
		opaqueLightingShader.depthStencilState.depthTestEnable = VK_FALSE;
		opaqueLightingShader.depthStencilState.depthWriteEnable = VK_FALSE;
		opaqueLightingShader.rasterizer.cullMode = VK_CULL_MODE_NONE;
		opaqueLightingShader.SetData(3);
		transparentLightingShader.inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
		transparentLightingShader.depthStencilState.depthTestEnable = VK_FALSE;
		transparentLightingShader.depthStencilState.depthWriteEnable = VK_FALSE;
		transparentLightingShader.rasterizer.cullMode = VK_CULL_MODE_NONE;
		transparentLightingShader.SetData(3);
		
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
			submodule->ConfigureShaders(shaders);
		}
	}
	
private: // Resources

	void CreateResources() override {
		float screenWidth = (float)swapChain->extent.width;
		float screenHeight = (float)swapChain->extent.height;
		uint uiWidth = (uint)(screenWidth * uiImageScale);
		uint uiHeight = (uint)(screenHeight * uiImageScale);
		
		// Create images
		uiImage.Create(renderingDevice, uiWidth, uiHeight);
		renderTargetGroup.SetRenderTarget(swapChain);
		renderTargetGroup.GetDepthImage().viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
		renderTargetGroup.GetDepthImage().preferredFormats = {VK_FORMAT_D32_SFLOAT};
		renderTargetGroup.GetDepthImage().usage |= VK_IMAGE_USAGE_SAMPLED_BIT;
		renderTargetGroup.CreateResources(renderingDevice);
		
		// Transition images
		TransitionImageLayout(uiImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
		TransitionImageLayout(renderTargetGroup.GetThumbnailImage(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
		TransitionImageLayout(renderTargetGroup.GetHistoryImage(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
		
		// Submodules
		for (auto* submodule : renderingSubmodules) {
			submodule->SetSwapChain(swapChain);
			submodule->CreateResources();
		}
	}
	
	void DestroyResources() override {
		// Destroy images
		uiImage.Destroy(renderingDevice);
		renderTargetGroup.DestroyResources(renderingDevice);
		
		// Submodules
		for (auto* submodule : renderingSubmodules) {
			submodule->DestroyResources();
		}
	}
	
	void AllocateBuffers() override {
		// Camera
		cameraUniformBuffer.Allocate(renderingDevice);
		
		// Submodules
		for (auto* submodule : renderingSubmodules) {
			submodule->AllocateBuffers();
		}
	}
	
	void FreeBuffers() override {
		// Camera
		cameraUniformBuffer.Free(renderingDevice);
		
		// Submodules
		for (auto* submodule : renderingSubmodules) {
			submodule->FreeBuffers();
		}
	}

private: // Pipelines

	void CreatePipelines() override {
		
		// Pipeline layouts
		lightingPipelineLayout.Create(renderingDevice);
		thumbnailPipelineLayout.Create(renderingDevice);
		postProcessingPipelineLayout.Create(renderingDevice);
		uiPipelineLayout.Create(renderingDevice);
		
		// Submodules
		for (auto* submodule : renderingSubmodules) {
			submodule->CreatePipelines(images);
		}
		
		{// Opaque Raster pass
			std::array<VkAttachmentDescription, RenderTargetGroup::GBUFFER_NB_IMAGES+1> attachments {};
			std::array<VkAttachmentReference, RenderTargetGroup::GBUFFER_NB_IMAGES> colorAttachmentRefs {};
			for (int i = 0; i < RenderTargetGroup::GBUFFER_NB_IMAGES; ++i) {
				// Format
				attachments[i].format = renderTargetGroup.GetGBuffer(i).format;
				attachments[i].samples = VK_SAMPLE_COUNT_1_BIT;
				// Color
				attachments[i].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
				attachments[i].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
				// Layout
				attachments[i].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
				attachments[i].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				colorAttachmentRefs[i] = {
					opaqueRasterPass.AddAttachment(attachments[i]),
					VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
				};
			}
			
			int depthStencilIndex = RenderTargetGroup::GBUFFER_NB_IMAGES;
			attachments[depthStencilIndex].format = renderTargetGroup.GetDepthImage().format;
			attachments[depthStencilIndex].samples = VK_SAMPLE_COUNT_1_BIT;
			attachments[depthStencilIndex].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			attachments[depthStencilIndex].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			attachments[depthStencilIndex].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			attachments[depthStencilIndex].stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE;
			attachments[depthStencilIndex].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			attachments[depthStencilIndex].finalLayout = VK_IMAGE_LAYOUT_GENERAL;
			VkAttachmentReference depthStencilAttachment {
				opaqueRasterPass.AddAttachment(attachments[depthStencilIndex]),
				VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
			};
			
			// SubPass
			VkSubpassDescription subpass = {};
				subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
				subpass.colorAttachmentCount = colorAttachmentRefs.size();
				subpass.pColorAttachments = colorAttachmentRefs.data();
				subpass.pDepthStencilAttachment = &depthStencilAttachment;
			opaqueRasterPass.AddSubpass(subpass);
			
			// prepare images
			std::vector<Image*> images {};
			images.reserve(attachments.size());
			for (auto& gBufferImage : renderTargetGroup.GetGBuffers()) {
				images.push_back(&gBufferImage);
			}
			images.push_back(&renderTargetGroup.GetDepthImage());
			
			// Create the render pass
			opaqueRasterPass.Create(renderingDevice);
			opaqueRasterPass.CreateFrameBuffers(renderingDevice, images);
			
			// Shaders
			for (auto* shaderPipeline : shaders["opaqueRasterization"]) {
				shaderPipeline->SetRenderPass(&renderTargetGroup.GetGBuffer(0), opaqueRasterPass.handle, 0);
				for (int i = 0; i < RenderTargetGroup::GBUFFER_NB_IMAGES; ++i)
					shaderPipeline->AddColorBlendAttachmentState(VK_FALSE);
				shaderPipeline->CreatePipeline(renderingDevice);
			}
		}
		
		{// Opaque Lighting pass
			const int nbColorAttachments = 1;
			const int nbInputAttachments = RenderTargetGroup::GBUFFER_NB_IMAGES;
			std::array<VkAttachmentDescription, nbColorAttachments + nbInputAttachments> attachments {};
			std::vector<Image*> images(nbColorAttachments + nbInputAttachments);
			std::array<VkAttachmentReference, nbColorAttachments> colorAttachmentRefs {};
			std::array<VkAttachmentReference, nbInputAttachments> inputAttachmentRefs {};
			
			// Color attachment
			images[0] = &renderTargetGroup.GetLitImage();
			attachments[0].format = renderTargetGroup.GetLitImage().format;
			attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
			attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			attachments[0].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			colorAttachmentRefs[0] = {
				opaqueLightingPass.AddAttachment(attachments[0]),
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
			};
			
			// Input attachments
			for (int i = 0; i < nbInputAttachments; ++i) {
				images[i+nbColorAttachments] = &renderTargetGroup.GetGBuffer(i);
				attachments[i+nbColorAttachments].format = renderTargetGroup.GetGBuffer(i).format;
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
			for (auto* shaderPipeline : shaders["opaqueLighting"]) {
				shaderPipeline->SetRenderPass(&renderTargetGroup.GetLitImage(), opaqueLightingPass.handle, 0);
				shaderPipeline->AddColorBlendAttachmentState();
				shaderPipeline->CreatePipeline(renderingDevice);
			}
		}
		
		{// Transparent Raster pass
			std::array<VkAttachmentDescription, RenderTargetGroup::GBUFFER_NB_IMAGES+1> attachments {};
			std::array<VkAttachmentReference, RenderTargetGroup::GBUFFER_NB_IMAGES> colorAttachmentRefs {};
			for (int i = 0; i < RenderTargetGroup::GBUFFER_NB_IMAGES; ++i) {
				// Format
				attachments[i].format = renderTargetGroup.GetGBuffer(i).format;
				attachments[i].samples = VK_SAMPLE_COUNT_1_BIT;
				// Color
				attachments[i].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
				attachments[i].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
				// Layout
				attachments[i].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
				attachments[i].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				colorAttachmentRefs[i] = {
					transparentRasterPass.AddAttachment(attachments[i]),
					VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
				};
			}
			
			int depthStencilIndex = RenderTargetGroup::GBUFFER_NB_IMAGES;
			attachments[depthStencilIndex].format = renderTargetGroup.GetDepthImage().format;
			attachments[depthStencilIndex].samples = VK_SAMPLE_COUNT_1_BIT;
			attachments[depthStencilIndex].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
			attachments[depthStencilIndex].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			attachments[depthStencilIndex].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
			attachments[depthStencilIndex].stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE;
			attachments[depthStencilIndex].initialLayout = VK_IMAGE_LAYOUT_GENERAL;
			attachments[depthStencilIndex].finalLayout = VK_IMAGE_LAYOUT_GENERAL;
			VkAttachmentReference depthStencilAttachment {
				transparentRasterPass.AddAttachment(attachments[depthStencilIndex]),
				VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
			};
			
			// SubPass
			VkSubpassDescription subpass = {};
				subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
				subpass.colorAttachmentCount = colorAttachmentRefs.size();
				subpass.pColorAttachments = colorAttachmentRefs.data();
				subpass.pDepthStencilAttachment = &depthStencilAttachment;
			transparentRasterPass.AddSubpass(subpass);
			
			// prepare images
			std::vector<Image*> images {};
			images.reserve(attachments.size());
			for (auto& gBufferImage : renderTargetGroup.GetGBuffers()) {
				images.push_back(&gBufferImage);
			}
			images.push_back(&renderTargetGroup.GetDepthImage());
			
			// Create the render pass
			transparentRasterPass.Create(renderingDevice);
			transparentRasterPass.CreateFrameBuffers(renderingDevice, images);
			
			// Shaders
			for (auto* shaderPipeline : shaders["transparentRasterization"]) {
				shaderPipeline->SetRenderPass(&renderTargetGroup.GetGBuffer(0), transparentRasterPass.handle, 0);
				for (int i = 0; i < RenderTargetGroup::GBUFFER_NB_IMAGES; ++i)
					shaderPipeline->AddColorBlendAttachmentState(VK_FALSE);
				shaderPipeline->CreatePipeline(renderingDevice);
			}
		}
		
		{// Transparent Lighting pass
			const int nbColorAttachments = 1;
			const int nbInputAttachments = RenderTargetGroup::GBUFFER_NB_IMAGES;
			std::array<VkAttachmentDescription, nbColorAttachments + nbInputAttachments> attachments {};
			std::vector<Image*> images(nbColorAttachments + nbInputAttachments);
			std::array<VkAttachmentReference, nbColorAttachments> colorAttachmentRefs {};
			std::array<VkAttachmentReference, nbInputAttachments> inputAttachmentRefs {};
			
			// Color attachment
			images[0] = &renderTargetGroup.GetLitImage();
			attachments[0].format = renderTargetGroup.GetLitImage().format;
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
				images[i+nbColorAttachments] = &renderTargetGroup.GetGBuffer(i);
				attachments[i+nbColorAttachments].format = renderTargetGroup.GetGBuffer(i).format;
				attachments[i+nbColorAttachments].samples = VK_SAMPLE_COUNT_1_BIT;
				attachments[i+nbColorAttachments].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
				attachments[i+nbColorAttachments].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
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
			for (auto* shaderPipeline : shaders["transparentLighting"]) {
				shaderPipeline->SetRenderPass(&renderTargetGroup.GetLitImage(), transparentLightingPass.handle, 0);
				shaderPipeline->AddColorBlendAttachmentState();
				shaderPipeline->CreatePipeline(renderingDevice);
			}
		}
		
		{// Thumbnail Gen render pass
			VkAttachmentDescription colorAttachment = {};
				colorAttachment.format = renderTargetGroup.GetThumbnailImage().format;
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
			thumbnailRenderPass.CreateFrameBuffers(renderingDevice, renderTargetGroup.GetThumbnailImage());
			
			// Shaders
			for (auto* shaderPipeline : shaders["thumbnail"]) {
				shaderPipeline->SetRenderPass(&renderTargetGroup.GetThumbnailImage(), thumbnailRenderPass.handle, 0);
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
		
	}
	
	void DestroyPipelines() override {
		
		#ifdef _ENABLE_IMGUI
			// ImGui
			ImGui_ImplVulkan_Shutdown();
		#endif
		
		// Rasterization pipelines
		for (ShaderPipeline* shaderPipeline : shaders["opaqueRasterization"]) {
			shaderPipeline->DestroyPipeline(renderingDevice);
		}
		for (ShaderPipeline* shaderPipeline : shaders["transparentRasterization"]) {
			shaderPipeline->DestroyPipeline(renderingDevice);
		}
		opaqueRasterPass.DestroyFrameBuffers(renderingDevice);
		opaqueRasterPass.Destroy(renderingDevice);
		transparentRasterPass.DestroyFrameBuffers(renderingDevice);
		transparentRasterPass.Destroy(renderingDevice);
		
		// Lighting pipelines
		for (ShaderPipeline* shaderPipeline : shaders["opaqueLighting"]) {
			shaderPipeline->DestroyPipeline(renderingDevice);
		}
		for (ShaderPipeline* shaderPipeline : shaders["transparentLighting"]) {
			shaderPipeline->DestroyPipeline(renderingDevice);
		}
		opaqueLightingPass.DestroyFrameBuffers(renderingDevice);
		opaqueLightingPass.Destroy(renderingDevice);
		transparentLightingPass.DestroyFrameBuffers(renderingDevice);
		transparentLightingPass.Destroy(renderingDevice);
		
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
		
		// UI
		for (auto* shaderPipeline : shaders["ui"]) {
			shaderPipeline->DestroyPipeline(renderingDevice);
		}
		uiRenderPass.DestroyFrameBuffers(renderingDevice);
		uiRenderPass.Destroy(renderingDevice);
		
		////////////////////////////
		// Pipeline layouts
		lightingPipelineLayout.Destroy(renderingDevice);
		thumbnailPipelineLayout.Destroy(renderingDevice);
		postProcessingPipelineLayout.Destroy(renderingDevice);
		uiPipelineLayout.Destroy(renderingDevice);
		
		// Submodules
		for (auto* submodule : renderingSubmodules) {
			submodule->DestroyPipelines();
		}
	}
	
private: // Commands

	void RecordGraphicsCommandBuffer(VkCommandBuffer commandBuffer, int imageIndex) override {
		
		// Gen Thumbnail
		thumbnailRenderPass.Begin(renderingDevice, commandBuffer, renderTargetGroup.GetThumbnailImage(), {{.0,.0,.0,.0}});
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
		
		cameraUniformBuffer.Update(renderingDevice, commandBuffer);
		
		// Submodules
		for (auto* submodule : renderingSubmodules) {
			submodule->RunDynamicGraphicsTop(commandBuffer, images);
		}
		
		auto gBuffersAndDepthStencilClearValues = renderTargetGroup.GetGBuffersClearValues();
		gBuffersAndDepthStencilClearValues.push_back(VkClearValue{0.0f,0});
		
		// Opaque Raster pass
		opaqueRasterPass.Begin(renderingDevice, commandBuffer, renderTargetGroup.GetGBuffer(0), gBuffersAndDepthStencilClearValues);
			for (auto* shaderPipeline : shaders["opaqueRasterization"]) {
				shaderPipeline->Execute(renderingDevice, commandBuffer);
			}
		opaqueRasterPass.End(renderingDevice, commandBuffer);
	
		// Opaque Lighting pass
		opaqueLightingPass.Begin(renderingDevice, commandBuffer, renderTargetGroup.GetLitImage(), {{.0,.0,.0,.0}});
			for (auto* shaderPipeline : shaders["opaqueLighting"]) {
				if (shaderPipeline->GetPipelineLayout() == &lightingPipelineLayout) {
					for (auto[id,lightSource] : scene.lightSources) if (lightSource) {
						//TODO optimisation : render spheres here instead of fullscreen quads
						shaderPipeline->Execute(renderingDevice, commandBuffer, 1, lightSource);
					}
				} else {
					shaderPipeline->Execute(renderingDevice, commandBuffer);
				}
			}
		opaqueLightingPass.End(renderingDevice, commandBuffer);
		
		// Submodules
		for (auto* submodule : renderingSubmodules) {
			submodule->RunDynamicGraphicsMiddle(commandBuffer, images);
		}
		
		// Transparent Raster pass
		transparentRasterPass.Begin(renderingDevice, commandBuffer, renderTargetGroup.GetGBuffer(0), gBuffersAndDepthStencilClearValues);
			for (auto* shaderPipeline : shaders["transparentRasterization"]) {
				shaderPipeline->Execute(renderingDevice, commandBuffer);
			}
		transparentRasterPass.End(renderingDevice, commandBuffer);
		
		// Transparent Lighting pass
		transparentLightingPass.Begin(renderingDevice, commandBuffer, renderTargetGroup.GetLitImage());
			for (auto* shaderPipeline : shaders["transparentLighting"]) {
				if (shaderPipeline->GetPipelineLayout() == &lightingPipelineLayout) {
					for (auto[id,lightSource] : scene.lightSources) if (lightSource) {
						//TODO optimisation : render spheres here instead of fullscreen quads
						shaderPipeline->Execute(renderingDevice, commandBuffer, 1, lightSource);
					}
				} else {
					shaderPipeline->Execute(renderingDevice, commandBuffer);
				}
			}
		transparentLightingPass.End(renderingDevice, commandBuffer);
		
		// Submodules
		for (auto* submodule : renderingSubmodules) {
			submodule->RunDynamicGraphicsBottom(commandBuffer, images);
		}
	}
	
	void RunDynamicLowPriorityCompute(VkCommandBuffer commandBuffer) override {
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
	
public: // Update

	void FrameUpdate(uint imageIndex) override {
		
		// Reset scene information
		scene.camera.width = swapChain->extent.width;
		scene.camera.height = swapChain->extent.height;
		scene.camera.RefreshProjectionMatrix();
		
		// Submodules
		for (auto* submodule : renderingSubmodules) {
			submodule->FrameUpdate(scene);
		}
		
		// Light sources
		for (auto[id,lightSource] : scene.lightSources) {
			lightSource->viewPosition = scene.camera.viewMatrix * glm::dvec4(lightSource->worldPosition, 1);
			lightSource->viewDirection = glm::transpose(glm::inverse(glm::mat3(scene.camera.viewMatrix))) * lightSource->worldDirection;
		}
		
		// {// TXAA jittering
		// 	static unsigned long frameCount = 0;
		// 	static const glm::dvec2 samples[16] = {
		// 		glm::dvec2(-8.0, 0.0) / 8.0,
		// 		glm::dvec2(-6.0, -4.0) / 8.0,
		// 		glm::dvec2(-3.0, -2.0) / 8.0,
		// 		glm::dvec2(-2.0, -6.0) / 8.0,
		// 		glm::dvec2(1.0, -1.0) / 8.0,
		// 		glm::dvec2(2.0, -5.0) / 8.0,
		// 		glm::dvec2(6.0, -7.0) / 8.0,
		// 		glm::dvec2(5.0, -3.0) / 8.0,
		// 		glm::dvec2(4.0, 1.0) / 8.0,
		// 		glm::dvec2(7.0, 4.0) / 8.0,
		// 		glm::dvec2(3.0, 5.0) / 8.0,
		// 		glm::dvec2(0.0, 7.0) / 8.0,
		// 		glm::dvec2(-1.0, 3.0) / 8.0,
		// 		glm::dvec2(-4.0, 6.0) / 8.0,
		// 		glm::dvec2(-7.0, 8.0) / 8.0,
		// 		glm::dvec2(-5.0, 2.0) / 8.0
		// 	};
		// 	glm::dvec2 texelSize = 1.0 / glm::dvec2(scene.camera.width, scene.camera.height);
		// 	glm::dvec2 subSample = samples[frameCount % 16] * texelSize / 2.0;
		// 	scene.camera.projectionMatrix[2].x += subSample.x;
		// 	scene.camera.projectionMatrix[2].y += subSample.y;
		// 	frameCount++;
		// }
		
	}
	
	void LowPriorityFrameUpdate() override {
		// Submodules
		for (auto* submodule : renderingSubmodules) {
			submodule->LowPriorityFrameUpdate();
		}
	}
	
	#ifdef _ENABLE_IMGUI
		void RunImGui() {
			// Submodules
			ImGui::Begin("Modules");
			for (auto* submodule : renderingSubmodules) {
				ImGui::Separator();
				submodule->RunImGui();
			}
			ImGui::End();
		}
	#endif
	
};
