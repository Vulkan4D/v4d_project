#pragma once
#include <v4d.h>
#include "V4DRenderingPipeline.hh"
#include "Camera.hpp"

using namespace v4d::graphics;

class V4DRenderer : public v4d::graphics::Renderer {
	using v4d::graphics::Renderer::Renderer;
	
	// Graphics configuration
	float uiImageScale = 1.0;
	
	// Buffers
	Buffer cameraUniformBuffer {VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, sizeof(CameraUBO), true};
	std::vector<Buffer*> stagedBuffers {};
	
	// UI
	Image uiImage { VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT ,1,1, { VK_FORMAT_R8G8B8_SNORM, VK_FORMAT_R8G8B8A8_SNORM }};
	
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
	
	// Main render passes
	RenderPass	opaqueRasterPass,
				opaqueLightingPass,
				transparentRasterPass,
				transparentLightingPass,
				postProcessingRenderPass;
	
	#pragma region Shaders
	
	std::vector<RasterShaderPipeline*> opaqueRasterizationShaders {};
	std::vector<RasterShaderPipeline*> transparentRasterizationShaders {};
	PipelineLayout lightingPipelineLayout, postProcessingPipelineLayout;
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
	
	#pragma endregion
	
	#pragma region Temporary galaxy stuff
	
	struct Galaxy {
		glm::vec4 posr;
		int seed;
		int numStars;
	};
	std::vector<Galaxy> galaxies {};
	uint RandomInt(uint& seed) {
		// LCG values from Numerical Recipes
		return (seed = 1664525 * seed + 1013904223);
	}
	float RandomFloat(uint& seed) {
		// Float version using bitmask from Numerical Recipes
		const uint one = 0x3f800000;
		const uint msk = 0x007fffff;
		return glm::uintBitsToFloat(one | (msk & (RandomInt(seed) >> 9))) - 1;
	}
	glm::vec3 RandomInUnitSphere(uint& seed) {
		for (;;) {
			glm::vec3 p = 2.0f * glm::vec3(RandomFloat(seed), RandomFloat(seed), RandomFloat(seed)) - 1.0f;
			if (dot(p, p) < 1) return p;
		}
	}
	
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
		
		// Lighting pass
		auto* gBuffersDescriptorSet_1 = descriptorSets.emplace_back(new DescriptorSet(1));
		for (int i = 0; i < Camera::GBUFFER_NB_IMAGES; ++i)
			gBuffersDescriptorSet_1->AddBinding_inputAttachment(i, &mainCamera.GetGBuffer(i).view, VK_SHADER_STAGE_FRAGMENT_BIT);
		lightingPipelineLayout.AddDescriptorSet(baseDescriptorSet_0);
		lightingPipelineLayout.AddDescriptorSet(gBuffersDescriptorSet_1);
		
		// Post-Processing
		auto* postProcessingDescriptorSet_1 = descriptorSets.emplace_back(new DescriptorSet(1));
		postProcessingDescriptorSet_1->AddBinding_combinedImageSampler(0, &mainCamera.GetTmpImage(), VK_SHADER_STAGE_FRAGMENT_BIT);
		postProcessingPipelineLayout.AddDescriptorSet(baseDescriptorSet_0);
		postProcessingPipelineLayout.AddDescriptorSet(postProcessingDescriptorSet_1);
		
	}
	
	void ConfigureShaders() override {
		// Galaxy Gen
		galaxyGenShader.inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
		galaxyGenShader.rasterizer.cullMode = VK_CULL_MODE_NONE;
		galaxyGenShader.depthStencilState.depthWriteEnable = VK_FALSE;
		galaxyGenShader.depthStencilState.depthTestEnable = VK_FALSE;
		
		// Galaxy Box
		galaxyBoxShader.inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
		galaxyBoxShader.rasterizer.cullMode = VK_CULL_MODE_NONE;
		galaxyBoxShader.depthStencilState.depthWriteEnable = VK_FALSE;
		galaxyBoxShader.depthStencilState.depthTestEnable = VK_FALSE;
		galaxyBoxShader.SetData(4);
		
		// Galaxy Fade
		galaxyFadeShader.inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
		galaxyFadeShader.rasterizer.cullMode = VK_CULL_MODE_NONE;
		galaxyFadeShader.depthStencilState.depthWriteEnable = VK_FALSE;
		galaxyFadeShader.depthStencilState.depthTestEnable = VK_FALSE;
		galaxyFadeShader.SetData(6);
		
		// Post-Processing
		postProcessingShader.inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
		postProcessingShader.rasterizer.cullMode = VK_CULL_MODE_NONE;
		postProcessingShader.depthStencilState.depthTestEnable = VK_FALSE;
		postProcessingShader.depthStencilState.depthWriteEnable = VK_FALSE;
		postProcessingShader.SetData(3);
		
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

		// Other buffers
		cameraUniformBuffer.UnmapMemory(renderingDevice);
		cameraUniformBuffer.Free(renderingDevice);
	}

private: // Graphics configuration

	void CreatePipelines() override {
		galaxyGenPipelineLayout.Create(renderingDevice);
		galaxyBoxPipelineLayout.Create(renderingDevice);
		postProcessingPipelineLayout.Create(renderingDevice);
		lightingPipelineLayout.Create(renderingDevice);
		
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
			std::array<VkAttachmentDescription, Camera::GBUFFER_NB_IMAGES> colorAttachments {};
			std::array<VkAttachmentReference, Camera::GBUFFER_NB_IMAGES> colorAttachmentRefs {};
			for (int i = 0; i < Camera::GBUFFER_NB_IMAGES; ++i) {
				// Format
				colorAttachments[i].format = mainCamera.GetGBuffer(i).format;
				colorAttachments[i].samples = VK_SAMPLE_COUNT_1_BIT;
				// Color
				colorAttachments[i].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
				colorAttachments[i].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
				// Stencil
				colorAttachments[i].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
				colorAttachments[i].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
				// Layout
				colorAttachments[i].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
				colorAttachments[i].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
				colorAttachmentRefs[i] = {
					opaqueRasterPass.AddAttachment(colorAttachments[i]),
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
			VkAttachmentDescription colorAttachment = {};
				colorAttachment.format = mainCamera.GetTmpImage().format;
				colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
				// Color and depth data
				colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
				colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
				// Stencil data
				colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
				colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
				colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
				colorAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			std::array<VkAttachmentReference, 1> colorAttachmentRefs {
				{
					opaqueLightingPass.AddAttachment(colorAttachment),
					VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
				}
			};
			std::array<VkAttachmentReference, Camera::GBUFFER_NB_IMAGES> inputAttachmentRefs {};
			for (int i = 0; i < Camera::GBUFFER_NB_IMAGES; ++i)
				inputAttachmentRefs[i] = {(uint)i, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
			
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
			opaqueLightingPass.CreateFrameBuffers(renderingDevice, mainCamera.GetTmpImage());
			
			// Shaders
			for (auto* shaderPipeline : {&galaxyBoxShader, &opaqueLightingShader}) {
				shaderPipeline->SetRenderPass(&mainCamera.GetTmpImage(), opaqueLightingPass.handle, 0);
				shaderPipeline->AddColorBlendAttachmentState();
				shaderPipeline->CreatePipeline(renderingDevice);
			}
		}
		
		{// Transparent Raster pass
			std::array<VkAttachmentDescription, Camera::GBUFFER_NB_IMAGES> colorAttachments {};
			std::array<VkAttachmentReference, Camera::GBUFFER_NB_IMAGES> colorAttachmentRefs {};
			for (int i = 0; i < Camera::GBUFFER_NB_IMAGES; ++i) {
				// Format
				colorAttachments[i].format = mainCamera.GetGBuffer(i).format;
				colorAttachments[i].samples = VK_SAMPLE_COUNT_1_BIT;
				// Color
				colorAttachments[i].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
				colorAttachments[i].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
				// Stencil
				colorAttachments[i].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
				colorAttachments[i].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
				// Layout
				colorAttachments[i].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
				colorAttachments[i].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
				colorAttachmentRefs[i] = {
					transparentRasterPass.AddAttachment(colorAttachments[i]),
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
			VkAttachmentDescription colorAttachment = {};
				colorAttachment.format = mainCamera.GetTmpImage().format;
				colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
				// Color and depth data
				colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
				colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
				// Stencil data
				colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
				colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
				colorAttachment.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
				colorAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			std::array<VkAttachmentReference, 1> colorAttachmentRefs {
				{
					transparentLightingPass.AddAttachment(colorAttachment),
					VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
				}
			};
			std::array<VkAttachmentReference, Camera::GBUFFER_NB_IMAGES> inputAttachmentRefs {};
			for (int i = 0; i < Camera::GBUFFER_NB_IMAGES; ++i)
				inputAttachmentRefs[i] = {(uint)i, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
			
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
			transparentLightingPass.CreateFrameBuffers(renderingDevice, mainCamera.GetTmpImage());
			
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
		
	}
	
	void DestroyPipelines() override {
		for (ShaderPipeline* shaderPipeline : {&galaxyBoxShader, &opaqueLightingShader, &transparentLightingShader}) {
			shaderPipeline->DestroyPipeline(renderingDevice);
		}
		for (ShaderPipeline* shaderPipeline : opaqueRasterizationShaders) {
			shaderPipeline->DestroyPipeline(renderingDevice);
		}
		for (ShaderPipeline* shaderPipeline : transparentRasterizationShaders) {
			shaderPipeline->DestroyPipeline(renderingDevice);
		}
		opaqueRasterPass.DestroyFrameBuffers(renderingDevice);
		opaqueRasterPass.Destroy(renderingDevice);
		opaqueLightingPass.DestroyFrameBuffers(renderingDevice);
		opaqueLightingPass.Destroy(renderingDevice);
		transparentRasterPass.DestroyFrameBuffers(renderingDevice);
		transparentRasterPass.Destroy(renderingDevice);
		transparentLightingPass.DestroyFrameBuffers(renderingDevice);
		transparentLightingPass.Destroy(renderingDevice);
		
		// Galaxy Gen
		galaxyGenShader.DestroyPipeline(renderingDevice);
		galaxyGenRenderPass.DestroyFrameBuffers(renderingDevice);
		galaxyGenRenderPass.Destroy(renderingDevice);
		
		// Galaxy Fade
		galaxyFadeShader.DestroyPipeline(renderingDevice);
		galaxyFadeRenderPass.DestroyFrameBuffers(renderingDevice);
		galaxyFadeRenderPass.Destroy(renderingDevice);
		
		// Post-processing
		for (auto* shaderPipeline : {&postProcessingShader}) {
			shaderPipeline->DestroyPipeline(renderingDevice);
		}
		postProcessingRenderPass.DestroyFrameBuffers(renderingDevice);
		postProcessingRenderPass.Destroy(renderingDevice);
		
		////////////////////////////
		// Pipeline layouts
		galaxyGenPipelineLayout.Destroy(renderingDevice);
		galaxyBoxPipelineLayout.Destroy(renderingDevice);
		postProcessingPipelineLayout.Destroy(renderingDevice);
		lightingPipelineLayout.Destroy(renderingDevice);
	}
	
private: // Commands
	void RecordComputeCommandBuffer(VkCommandBuffer, int imageIndex) override {}
	void RecordGraphicsCommandBuffer(VkCommandBuffer commandBuffer, int imageIndex) override {
		// Post Processing
		TransitionImageLayout(commandBuffer, mainCamera.GetTmpImage(), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL);
		postProcessingRenderPass.Begin(renderingDevice, commandBuffer, swapChain, {{.0,.0,.0,.0}}, imageIndex);
		postProcessingShader.Execute(renderingDevice, commandBuffer);
		postProcessingRenderPass.End(renderingDevice, commandBuffer);
		
	}
	void RecordLowPriorityComputeCommandBuffer(VkCommandBuffer) override {}
	void RecordLowPriorityGraphicsCommandBuffer(VkCommandBuffer) override {}
	void RunDynamicCompute(VkCommandBuffer) override {}
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
		if (continuousGalaxyGen) {
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
		
		// Galaxy Gen (temporary stuff)
		uint seed = 1;
		for (int x = -64; x < 63; ++x) {
			for (int y = -64; y < 63; ++y) {
				for (int z = -8; z < 7; ++z) {
					float centerFactor = 1.0f - glm::length(glm::vec3((float)x,(float)y,(float)z))/180.0f;
					if (centerFactor > 0.0) {
						auto offset = RandomInUnitSphere(seed)*2.84f;
						galaxies.push_back({
							{x*5+offset.x, y*5+offset.y, z*5+offset.z, glm::pow(centerFactor, 25)*10}, x*y*z*3+5, (int)(glm::pow(centerFactor, 8)*50.0f)
						});
					}
				}
			}
		}
		
		// Galaxy Gen
		Buffer* galaxiesBuffer = stagedBuffers.emplace_back(new Buffer(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT));
		galaxiesBuffer->AddSrcDataPtr(&galaxies);
		galaxyGenShader.SetData(galaxiesBuffer, galaxies.size());
		
	}
	
	void ReadShaders() override {
		galaxyGenShader.ReadShaders();
		galaxyBoxShader.ReadShaders();
		galaxyFadeShader.ReadShaders();
		postProcessingShader.ReadShaders();
	}
	
	void UnloadScene() override {
		// Staged buffers
		for (auto* buffer : stagedBuffers) {
			delete buffer;
		}
		stagedBuffers.clear();
		
		// Galaxies
		galaxies.clear();
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
		// Galaxy convergence
		const int convergences = 10;
		galaxyFrameIndex++;
		if (galaxyFrameIndex > convergences) galaxyFrameIndex = continuousGalaxyGen? 0 : convergences;
		
		// Update Push Constants
		galaxyGenPushConstant.cameraPosition = mainCamera.GetWorldPosition();
		galaxyGenPushConstant.frameIndex = galaxyFrameIndex;
	}
	
public: // ubo/conditional member variables
	bool continuousGalaxyGen = true;
	int galaxyFrameIndex = 0;
	
};
