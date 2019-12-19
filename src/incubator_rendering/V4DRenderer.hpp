#pragma once
#include <v4d.h>
#include "V4DRenderingPipeline.hh"
#include "Camera.hpp"

#include "../incubator_galaxy4d/Noise.hpp"
// #include "../incubator_galaxy4d/UniversalPosition.hpp"

using namespace v4d::graphics;

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
	
	CubeMapImage galaxyCubeMapImage { VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT };
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
	struct Galaxy {
		glm::vec4 posr;
		int seed;
		int numStars;
	};
	std::vector<Galaxy> galaxies {};
	Buffer galaxiesBuffer { VK_BUFFER_USAGE_VERTEX_BUFFER_BIT };
	bool galaxiesGenerated = false;
	#pragma endregion
	
public: // Camera
	Camera mainCamera;
	
private: // Init
	void ScorePhysicalDeviceSelection(int& score, PhysicalDevice* physicalDevice) override {}
	void Init() override {}
	void Info() override {}

	void InitLayouts() override { 
		
		// Galaxy Gen
		galaxyGenPipelineLayout.AddPushConstant<GalaxyGenPushConstant>(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_GEOMETRY_BIT);
		galaxyGenShader.AddVertexInputBinding(sizeof(Galaxy), VK_VERTEX_INPUT_RATE_VERTEX, {
			{0, offsetof(Galaxy, Galaxy::posr), VK_FORMAT_R32G32B32A32_SFLOAT},
			{1, offsetof(Galaxy, Galaxy::seed), VK_FORMAT_R32_UINT},
			{2, offsetof(Galaxy, Galaxy::numStars), VK_FORMAT_R32_UINT},
		});
		
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
		galaxyGenShader.inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
		galaxyGenShader.depthStencilState.depthWriteEnable = VK_FALSE;
		galaxyGenShader.depthStencilState.depthTestEnable = VK_FALSE;
		
		// Galaxy Fade
		galaxyFadeShader.inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
		galaxyFadeShader.depthStencilState.depthWriteEnable = VK_FALSE;
		galaxyFadeShader.depthStencilState.depthTestEnable = VK_FALSE;
		galaxyFadeShader.SetData(6);
		
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
	}
	
	void FreeBuffers() override {
		for (auto& buffer : stagedBuffers) {
			buffer->Free(renderingDevice);
		}
		
		// Galaxies
		galaxiesBuffer.Free(renderingDevice);
		galaxies.clear();
		galaxiesGenerated = false;

		// Other buffers
		cameraUniformBuffer.UnmapMemory(renderingDevice);
		cameraUniformBuffer.Free(renderingDevice);
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
		
		{// Galaxy Gen render pass
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
				/*srcColorBlendFactor*/		VK_BLEND_FACTOR_ONE,
				/*dstColorBlendFactor*/		VK_BLEND_FACTOR_ONE,
				/*colorBlendOp*/			VK_BLEND_OP_MAX,
				/*srcAlphaBlendFactor*/		VK_BLEND_FACTOR_ONE,
				/*dstAlphaBlendFactor*/		VK_BLEND_FACTOR_ONE,
				/*alphaBlendOp*/			VK_BLEND_OP_MAX
			);
			galaxyGenShader.CreatePipeline(renderingDevice);
		}
		
		{// Galaxy Fade render pass
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
				galaxyFadeRenderPass.AddAttachment(colorAttachment),
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
			};
			// SubPass
			VkSubpassDescription subpass = {};
				subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
				subpass.colorAttachmentCount = 1;
				subpass.pColorAttachments = &colorAttachmentRef;
				subpass.pDepthStencilAttachment = nullptr;
			galaxyFadeRenderPass.AddSubpass(subpass);
			
			// Create the render pass
			galaxyFadeRenderPass.Create(renderingDevice);
			galaxyFadeRenderPass.CreateFrameBuffers(renderingDevice, galaxyCubeMapImage);
			
			// Shader pipeline
			galaxyFadeShader.SetRenderPass(&galaxyCubeMapImage, galaxyFadeRenderPass.handle, 0);
			galaxyFadeShader.AddColorBlendAttachmentState(
				/*blendEnable*/				VK_TRUE,
				/*srcColorBlendFactor*/		VK_BLEND_FACTOR_ONE,
				/*dstColorBlendFactor*/		VK_BLEND_FACTOR_ONE,
				/*colorBlendOp*/			VK_BLEND_OP_REVERSE_SUBTRACT,
				/*srcAlphaBlendFactor*/		VK_BLEND_FACTOR_ONE,
				/*dstAlphaBlendFactor*/		VK_BLEND_FACTOR_ONE,
				/*alphaBlendOp*/			VK_BLEND_OP_MAX
			);
			// Shader stages
			galaxyFadeShader.CreatePipeline(renderingDevice);
		}
		
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
		galaxyGenShader.DestroyPipeline(renderingDevice);
		galaxyGenRenderPass.DestroyFrameBuffers(renderingDevice);
		galaxyGenRenderPass.Destroy(renderingDevice);
		
		// Galaxy Fade
		galaxyFadeShader.DestroyPipeline(renderingDevice);
		galaxyFadeRenderPass.DestroyFrameBuffers(renderingDevice);
		galaxyFadeRenderPass.Destroy(renderingDevice);
		
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
		
		if (continuousGalaxyGen || galaxyFrameIndex < galaxyConvergences) {
			// Galaxy Gen
			galaxyGenRenderPass.Begin(renderingDevice, commandBuffer, galaxyCubeMapImage);
			galaxyGenShader.Execute(renderingDevice, commandBuffer, &galaxyGenPushConstant);
			galaxyGenRenderPass.End(renderingDevice, commandBuffer);
			// Galaxy Fade
			galaxyFadeRenderPass.Begin(renderingDevice, commandBuffer, galaxyCubeMapImage);
			galaxyFadeShader.Execute(renderingDevice, commandBuffer);
			galaxyFadeRenderPass.End(renderingDevice, commandBuffer);
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
		if (!galaxiesGenerated && galaxies.size() == 0) {
			const int neighborGridsToLoadPerAxis = 1;
			galaxies.reserve((size_t)(pow(1+neighborGridsToLoadPerAxis*2, 3)*pow(32, 3)/10));
			// Galaxy Gen (temporary stuff)
			for (int gridX = -neighborGridsToLoadPerAxis; gridX <= neighborGridsToLoadPerAxis; ++gridX) {
				for (int gridY = -neighborGridsToLoadPerAxis; gridY <= neighborGridsToLoadPerAxis; ++gridY) {
					for (int gridZ = -neighborGridsToLoadPerAxis; gridZ <= neighborGridsToLoadPerAxis; ++gridZ) {
						int subGridSize = v4d::noise::UniverseSubGridSize({gridX,gridY,gridZ});
						for (int x = 0; x < subGridSize; ++x) {
							for (int y = 0; y < subGridSize; ++y) {
								for (int z = 0; z < subGridSize; ++z) {
									glm::vec3 galaxyPositionInGrid = {
										float(gridX) + float(x)/float(subGridSize),
										float(gridY) + float(y)/float(subGridSize),
										float(gridZ) + float(z)/float(subGridSize)
									};
									float galaxySizeFactor = v4d::noise::GalaxySizeFactorInUniverseGrid(galaxyPositionInGrid);
									if (galaxySizeFactor > 0.0f) {
										// For each existing galaxy
										galaxies.push_back({
											glm::vec4(galaxyPositionInGrid + v4d::noise::Noise3(galaxyPositionInGrid)/float(subGridSize), galaxySizeFactor/subGridSize/2.0f),
											x*y*z*3+5,
											80
										});
									}
								}
							}
						}
					}
				}
			}
			LOG("Number of Galaxies generated : " << galaxies.size())
			galaxiesBuffer.Free(renderingDevice);
			galaxiesBuffer.ResetSrcData();
			galaxiesBuffer.AddSrcDataPtr(&galaxies);
			galaxyGenShader.SetData(&galaxiesBuffer, galaxies.size());
			AllocateBufferStaged(lowPriorityGraphicsQueue, galaxiesBuffer);
			galaxiesGenerated = true;
		}
		
		// Galaxy convergence
		galaxyFrameIndex++;
		if (galaxyFrameIndex > galaxyConvergences) galaxyFrameIndex = continuousGalaxyGen? 0 : galaxyConvergences;
		
		// Update Push Constants
		galaxyGenPushConstant.cameraPosition = mainCamera.GetWorldPosition();
		galaxyGenPushConstant.frameIndex = galaxyFrameIndex;
		galaxyGenPushConstant.resolution = galaxyCubeMapImage.width;
	}
	
public: // ubo/conditional member variables
	int galaxyConvergences = 10;
	bool continuousGalaxyGen = true;
	int galaxyFrameIndex = 0;
	
};
