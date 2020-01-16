#pragma once
#include <v4d.h>
#include "V4DRenderingPipeline.hh"
#include "Camera.hpp"
#include "../incubator_rendering/helpers/Geometry.hpp"

#include "../incubator_galaxy4d/Noise.hpp"
// #include "../incubator_galaxy4d/Universe.hpp"
// #include "../incubator_galaxy4d/PlanetRayMarchingTests.hpp"
#include "../incubator_galaxy4d/PlanetRenderer.hpp"

using namespace v4d::graphics;
using namespace v4d::graphics::vulkan::rtx;

class V4DRenderer : public v4d::graphics::Renderer {
	using v4d::graphics::Renderer::Renderer;

	// Universe universe;
	PlanetRenderer planetRenderer;
	
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
	
private: 
public: // Camera
	Camera mainCamera;
	
private: // Init
	void ScorePhysicalDeviceSelection(int& score, PhysicalDevice* physicalDevice) override {}
	void Init() override {
		// universe.Init(this);
		planetRenderer.Init(this);
	}
	void Info() override {
		// universe.Info(this, renderingDevice);
		planetRenderer.Info(this, renderingDevice);
	}

	void InitLayouts() override {
		// Base descriptor set containing CameraUBO and such, almost all shaders should use it
		auto* baseDescriptorSet_0 = descriptorSets.emplace_back(new DescriptorSet(0));
		baseDescriptorSet_0->AddBinding_uniformBuffer(0, &cameraUniformBuffer, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_GEOMETRY_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
		
		// Universe
		// universe.InitLayouts(this, descriptorSets, baseDescriptorSet_0);
		planetRenderer.InitLayouts(this, descriptorSets, baseDescriptorSet_0);
		
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
		
		// universe.ConfigureShaders();
		planetRenderer.ConfigureShaders();
		
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
		uint uiWidth = (uint)(screenWidth * uiImageScale);
		uint uiHeight = (uint)(screenHeight * uiImageScale);
		
		// universe.CreateResources(this, renderingDevice, screenWidth, screenHeight);
		planetRenderer.CreateResources(this, renderingDevice, screenWidth, screenHeight);
		
		// Create images
		uiImage.Create(renderingDevice, uiWidth, uiHeight);
		mainCamera.SetRenderTarget(swapChain);
		mainCamera.CreateResources(renderingDevice);
		
		// Transition images
		TransitionImageLayout(uiImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
		TransitionImageLayout(mainCamera.GetThumbnailImage(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
	}
	
	void DestroyResources() override {
		// universe.DestroyResources(renderingDevice);
		planetRenderer.DestroyResources(renderingDevice);
		// Destroy images
		uiImage.Destroy(renderingDevice);
		mainCamera.DestroyResources(renderingDevice);
	}
	
	void AllocateBuffers() override {
		// Staged Buffers
		AllocateBuffersStaged(transferQueue, stagedBuffers);
		
		// Other buffers
		cameraUniformBuffer.Allocate(renderingDevice, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, false);
		cameraUniformBuffer.MapMemory(renderingDevice);
		
		// universe.AllocateBuffers(renderingDevice);
		planetRenderer.AllocateBuffers(this, renderingDevice, transferQueue);
	}
	
	void FreeBuffers() override {
		// Staged Buffers
		for (auto& buffer : stagedBuffers) {
			buffer->Free(renderingDevice);
		}
		
		// Other buffers
		cameraUniformBuffer.UnmapMemory(renderingDevice);
		cameraUniformBuffer.Free(renderingDevice);
		
		// universe.FreeBuffers(this, renderingDevice);
		planetRenderer.FreeBuffers(this, renderingDevice);
	}

private: // Graphics configuration

	void CreatePipelines() override {
		std::vector<RasterShaderPipeline*> opaqueLightingShaders {
			&opaqueLightingShader
		};
		
		// universe.CreatePipelines(this, renderingDevice, opaqueLightingShaders);
		planetRenderer.CreatePipelines(this, renderingDevice, opaqueLightingShaders);
		
		standardPipelineLayout.Create(renderingDevice);
		lightingPipelineLayout.Create(renderingDevice);
		thumbnailPipelineLayout.Create(renderingDevice);
		postProcessingPipelineLayout.Create(renderingDevice);
		uiPipelineLayout.Create(renderingDevice);
		
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
			for (auto* shaderPipeline : opaqueLightingShaders) {
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
		// universe.DestroyPipelines(this, renderingDevice);
		planetRenderer.DestroyPipelines(this, renderingDevice);
		
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
		for (ShaderPipeline* shaderPipeline : {&opaqueLightingShader, &transparentLightingShader}) {
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
		
		// universe.RecordGraphicsCommandBuffer(commandBuffer, imageIndex);
		planetRenderer.RecordGraphicsCommandBuffer(commandBuffer, imageIndex);
	}
	void RunDynamicGraphics(VkCommandBuffer commandBuffer) override {
		
		// Prepass
		planetRenderer.RunDynamic(renderingDevice, commandBuffer);
		
		// Opaque Raster pass
		opaqueRasterPass.Begin(renderingDevice, commandBuffer, mainCamera.GetGBuffer(0), mainCamera.GetGBuffersClearValues());
			for (auto* shaderPipeline : opaqueRasterizationShaders) {
				shaderPipeline->Execute(renderingDevice, commandBuffer);
			}
		opaqueRasterPass.End(renderingDevice, commandBuffer);
	
		// Opaque Lighting pass
		opaqueLightingPass.Begin(renderingDevice, commandBuffer, mainCamera.GetTmpImage(), {{.0,.0,.0,.0}});
			// universe.RunInOpaqueLightingPass(renderingDevice, commandBuffer);
			planetRenderer.RunInOpaqueLightingPass(this, renderingDevice, commandBuffer, mainCamera);
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
		
		// universe.RunLowPriorityGraphics(renderingDevice, commandBuffer);
		planetRenderer.RunLowPriorityGraphics(renderingDevice, commandBuffer);
	}
	
public: // Scene configuration
	void LoadScene() override {
		planetRenderer.LoadScene();
	}
	
	void ReadShaders() override {
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
			
		// universe.ReadShaders();
		planetRenderer.ReadShaders();
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
		
		// universe.FrameUpdate(this, renderingDevice, mainCamera);
		planetRenderer.FrameUpdate(this, renderingDevice, mainCamera);
	}
	void LowPriorityFrameUpdate() override {
		// universe.LowPriorityFrameUpdate(this, renderingDevice, mainCamera, lowPriorityGraphicsQueue);
		planetRenderer.LowPriorityFrameUpdate(this, renderingDevice, mainCamera, lowPriorityGraphicsQueue);
	}
	
};
