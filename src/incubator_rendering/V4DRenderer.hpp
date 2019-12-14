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
	// Buffer viewUniformBuffer {VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, sizeof(ViewUBO), true};
	std::vector<Buffer*> stagedBuffers {};
	
	// Galaxy rendering
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
	
	PipelineLayout postProcessingPipelineLayout;
	RasterShaderPipeline postProcessingShader {postProcessingPipelineLayout, {
		"incubator_rendering/assets/shaders/v4d_postProcessing.vert",
		"incubator_rendering/assets/shaders/v4d_postProcessing.frag",
	}};
	RenderPass postProcessingRenderPass;
	
	// UI
	Image uiImage { VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT ,1,1, { VK_FORMAT_R8G8B8_SNORM, VK_FORMAT_R8G8B8A8_SNORM }};
	
	// temporary render pass
	RenderPass	opaqueRasterPass,
				opaqueLightingPass,
				transparentRasterPass,
				transparentLightingPass;
	
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
		
		/////////
		
		
		// Galaxy Box
		auto* galaxyBoxDescriptorSet = descriptorSets.emplace_back(new DescriptorSet(0));
		galaxyBoxDescriptorSet->AddBinding_combinedImageSampler(0, &galaxyCubeMapImage, VK_SHADER_STAGE_FRAGMENT_BIT);
		galaxyBoxPipelineLayout.AddDescriptorSet(galaxyBoxDescriptorSet);
		galaxyBoxPipelineLayout.AddPushConstant<GalaxyBoxPushConstant>(VK_SHADER_STAGE_VERTEX_BIT);
		
		
		/////////
		
		// Post-Processing
		auto* postProcessingDescriptorSet = descriptorSets.emplace_back(new DescriptorSet(0));
		postProcessingDescriptorSet->AddBinding_combinedImageSampler(0, mainCamera.GetTmpImage(), VK_SHADER_STAGE_FRAGMENT_BIT);
		postProcessingPipelineLayout.AddDescriptorSet(postProcessingDescriptorSet);
		
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
		// viewUniformBuffer.Allocate(renderingDevice, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, false);
		// viewUniformBuffer.MapMemory(renderingDevice);
	}
	
	void FreeBuffers() override {
		for (auto& buffer : stagedBuffers) {
			buffer->Free(renderingDevice);
		}

		// Other buffers
		// viewUniformBuffer.UnmapMemory(renderingDevice);
		// viewUniformBuffer.Free(renderingDevice);
	}

private: // Graphics configuration

	void CreatePipelines() override {
		galaxyGenPipelineLayout.Create(renderingDevice);
		galaxyBoxPipelineLayout.Create(renderingDevice);
		postProcessingPipelineLayout.Create(renderingDevice);
		
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
			// Color Attachment (Fragment shader Standard Output)
			VkAttachmentDescription colorAttachment = {};
				colorAttachment.format = mainCamera.GetTmpImage()->format;
				colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
				// Color and depth data
				colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
									/*	VK_ATTACHMENT_LOAD_OP_LOAD = Preserve the existing contents of the attachment
										VK_ATTACHMENT_LOAD_OP_CLEAR = Clear the values to a constant at the start
										VK_ATTACHMENT_LOAD_OP_DONT_CARE = Existing contents are undefined, we dont care about them (faster but may cause glitches if not set properly I guess)
									*/
				colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
									/*	VK_ATTACHMENT_STORE_OP_STORE = Rendered contents contents will be stored in memory and can be renad later (Need this to see something on the screen in the case of o_color)
										VK_ATTACHMENT_STORE_OP_DONT_CARE = Contents of the framebuffer will be undefined after rendering operation (Ignore this data ??)
									*/
				// Stencil data
				colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
				colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
				// Textures and framebuffers in Vulkan are represented by VkImage objects with a certain pixel format, however the layout of the pixels in memory can change based on what you're trying to do with an image.
				// The InitialLayout specifies wich layout the image will have before the render pass begins. 
				// The finalLayout specifies the layout to automatically transition to when the render pass finishes. 
				// Using VK_IMAGE_LAYOUT_UNDEFINED for initialLayout means that we dont care what previous layout the image was in. 
				// The caveat of this special value is that the contents of the image are not guaranteed to be preserved, but that doesnt matter since were going to clear it anyway. 
				// We want the image to be ready for presentation using the swap chain after rendering, which is why we use VK_IMAGE_LAYOUT_PRESENT_SRC_KHR as finalLayout.
				colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
				colorAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
										/*	VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL = Images used as color attachment (use this for color attachment with multisampling, adapt rendering pipeline)
											VK_IMAGE_LAYOUT_PRESENT_SRC_KHR = Images to be presented in the swap chain (use this for color attachment when no multisampling, adapt rendering pipeline)
											VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL = Images to be used as destination for a memory copy operation
										*/
				// A single render pass can consist of multiple subpasses.
				// Subpasses are subsequent rendering operations that depend on the contents of framebuffers in previous passes, for example a sequence of post-processing effects that are applied one after another. 
				// If you group these rendering operations into one render pass, then vulkan is able to reorder the operations and conserve memory bandwidth for possibly better preformance. 
				// Every subpass references one or more of the attachments that we've described using the structure in the previous sections. 
				// These references are themselves VkAttachmentReference structs that look like this:
			VkAttachmentReference colorAttachmentRef = {
				opaqueRasterPass.AddAttachment(colorAttachment), // layout(location = 0) for output data in the fragment shader
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
			};
			// The attachment parameter specifies which attachment to reference by its index in the attachment descriptions array.
			// Our array consists of a single VkAttachmentDescription, so its index is 0. 
			// The layout specifies which layout we would like the attachment to have during a subpass that uses this reference.
			// Vulkan will automatically transition the attachment to this layout when the subpass is started.
			// We intend to use the attachment to function as a color buffer and the VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL layout will give us the best performance, as its name implies.

			
			// SubPass
			VkSubpassDescription subpass = {};
				subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
				subpass.colorAttachmentCount = 1;
				subpass.pColorAttachments = &colorAttachmentRef;
			opaqueRasterPass.AddSubpass(subpass);
			// The following other types of attachments can be referenced by a subpass :
				// pInputAttachments : attachments that are read from a shader
				// pResolveAttachments : attachments used for multisampling color attachments
				// pDepthStencilAttachments : attachments for depth and stencil data
				// pPreserveAttachments : attachments that are not used by this subpass, but for which the data must be preserved
			// Render pass
			// Now that the attachment and a basic subpass refencing it have been described, we can create the render pass itself.
			// The render pass object can then be created by filling in the VkRenderPassCreateInfo structure with an array of attachments and subpasses. 
			// The VkAttachmentReference objects reference attachments using the indices of this array.

			// Render Pass Dependencies
			// There are two built-in dependencies that take care of the transition at the start of the render pass and at the end of the render pass, 
			// but the former does not occur at the right time. It assumes that the transition occurs at the start of the pipeline, but we haven't aquired the image yet at that point.
			// There are two ways to deal with this problem.
			// We could change the waitStage for the imageAvailableSemaphore to VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT to ensure that the render passes dont begin until the image is available, 
			// or we can make the render pass wait for the VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT stage, which is the option that we are using here.

			// Create the render pass
			opaqueRasterPass.Create(renderingDevice);
			opaqueRasterPass.CreateFrameBuffers(renderingDevice, *mainCamera.GetTmpImage());
			
			// Shaders
			for (auto* shaderPipeline : {&galaxyBoxShader}) {
				shaderPipeline->SetRenderPass(mainCamera.GetTmpImage(), opaqueRasterPass.handle, 0);
				shaderPipeline->AddColorBlendAttachmentState();
				shaderPipeline->CreatePipeline(renderingDevice);
			}
		}
		
		if (false) {// Opaque Lighting pass
			// Color Attachment (Fragment shader Standard Output)
			VkAttachmentDescription colorAttachment = {};
				colorAttachment.format = mainCamera.GetTmpImage()->format;
				colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
				// Color and depth data
				colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
									/*	VK_ATTACHMENT_LOAD_OP_LOAD = Preserve the existing contents of the attachment
										VK_ATTACHMENT_LOAD_OP_CLEAR = Clear the values to a constant at the start
										VK_ATTACHMENT_LOAD_OP_DONT_CARE = Existing contents are undefined, we dont care about them (faster but may cause glitches if not set properly I guess)
									*/
				colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
									/*	VK_ATTACHMENT_STORE_OP_STORE = Rendered contents contents will be stored in memory and can be renad later (Need this to see something on the screen in the case of o_color)
										VK_ATTACHMENT_STORE_OP_DONT_CARE = Contents of the framebuffer will be undefined after rendering operation (Ignore this data ??)
									*/
				// Stencil data
				colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
				colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
				// Textures and framebuffers in Vulkan are represented by VkImage objects with a certain pixel format, however the layout of the pixels in memory can change based on what you're trying to do with an image.
				// The InitialLayout specifies wich layout the image will have before the render pass begins. 
				// The finalLayout specifies the layout to automatically transition to when the render pass finishes. 
				// Using VK_IMAGE_LAYOUT_UNDEFINED for initialLayout means that we dont care what previous layout the image was in. 
				// The caveat of this special value is that the contents of the image are not guaranteed to be preserved, but that doesnt matter since were going to clear it anyway. 
				// We want the image to be ready for presentation using the swap chain after rendering, which is why we use VK_IMAGE_LAYOUT_PRESENT_SRC_KHR as finalLayout.
				colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
				colorAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
										/*	VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL = Images used as color attachment (use this for color attachment with multisampling, adapt rendering pipeline)
											VK_IMAGE_LAYOUT_PRESENT_SRC_KHR = Images to be presented in the swap chain (use this for color attachment when no multisampling, adapt rendering pipeline)
											VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL = Images to be used as destination for a memory copy operation
										*/
				// A single render pass can consist of multiple subpasses.
				// Subpasses are subsequent rendering operations that depend on the contents of framebuffers in previous passes, for example a sequence of post-processing effects that are applied one after another. 
				// If you group these rendering operations into one render pass, then vulkan is able to reorder the operations and conserve memory bandwidth for possibly better preformance. 
				// Every subpass references one or more of the attachments that we've described using the structure in the previous sections. 
				// These references are themselves VkAttachmentReference structs that look like this:
			VkAttachmentReference colorAttachmentRef = {
				opaqueLightingPass.AddAttachment(colorAttachment), // layout(location = 0) for output data in the fragment shader
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
			};
			// The attachment parameter specifies which attachment to reference by its index in the attachment descriptions array.
			// Our array consists of a single VkAttachmentDescription, so its index is 0. 
			// The layout specifies which layout we would like the attachment to have during a subpass that uses this reference.
			// Vulkan will automatically transition the attachment to this layout when the subpass is started.
			// We intend to use the attachment to function as a color buffer and the VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL layout will give us the best performance, as its name implies.

			
			// SubPass
			VkSubpassDescription subpass = {};
				subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
				subpass.colorAttachmentCount = 1;
				subpass.pColorAttachments = &colorAttachmentRef;
			opaqueLightingPass.AddSubpass(subpass);
			// The following other types of attachments can be referenced by a subpass :
				// pInputAttachments : attachments that are read from a shader
				// pResolveAttachments : attachments used for multisampling color attachments
				// pDepthStencilAttachments : attachments for depth and stencil data
				// pPreserveAttachments : attachments that are not used by this subpass, but for which the data must be preserved
			// Render pass
			// Now that the attachment and a basic subpass refencing it have been described, we can create the render pass itself.
			// The render pass object can then be created by filling in the VkRenderPassCreateInfo structure with an array of attachments and subpasses. 
			// The VkAttachmentReference objects reference attachments using the indices of this array.

			// Render Pass Dependencies
			// There are two built-in dependencies that take care of the transition at the start of the render pass and at the end of the render pass, 
			// but the former does not occur at the right time. It assumes that the transition occurs at the start of the pipeline, but we haven't aquired the image yet at that point.
			// There are two ways to deal with this problem.
			// We could change the waitStage for the imageAvailableSemaphore to VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT to ensure that the render passes dont begin until the image is available, 
			// or we can make the render pass wait for the VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT stage, which is the option that we are using here.

			// Create the render pass
			opaqueLightingPass.Create(renderingDevice);
			opaqueLightingPass.CreateFrameBuffers(renderingDevice, *mainCamera.GetTmpImage());
			
			// Shaders
			for (auto* shaderPipeline : {&galaxyBoxShader}) {
				shaderPipeline->SetRenderPass(mainCamera.GetTmpImage(), opaqueLightingPass.handle, 0);
				shaderPipeline->AddColorBlendAttachmentState();
				shaderPipeline->CreatePipeline(renderingDevice);
			}
		}
		
		if (false) {// Transparent Raster pass
			// Color Attachment (Fragment shader Standard Output)
			VkAttachmentDescription colorAttachment = {};
				colorAttachment.format = mainCamera.GetTmpImage()->format;
				colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
				// Color and depth data
				colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
									/*	VK_ATTACHMENT_LOAD_OP_LOAD = Preserve the existing contents of the attachment
										VK_ATTACHMENT_LOAD_OP_CLEAR = Clear the values to a constant at the start
										VK_ATTACHMENT_LOAD_OP_DONT_CARE = Existing contents are undefined, we dont care about them (faster but may cause glitches if not set properly I guess)
									*/
				colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
									/*	VK_ATTACHMENT_STORE_OP_STORE = Rendered contents contents will be stored in memory and can be renad later (Need this to see something on the screen in the case of o_color)
										VK_ATTACHMENT_STORE_OP_DONT_CARE = Contents of the framebuffer will be undefined after rendering operation (Ignore this data ??)
									*/
				// Stencil data
				colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
				colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
				// Textures and framebuffers in Vulkan are represented by VkImage objects with a certain pixel format, however the layout of the pixels in memory can change based on what you're trying to do with an image.
				// The InitialLayout specifies wich layout the image will have before the render pass begins. 
				// The finalLayout specifies the layout to automatically transition to when the render pass finishes. 
				// Using VK_IMAGE_LAYOUT_UNDEFINED for initialLayout means that we dont care what previous layout the image was in. 
				// The caveat of this special value is that the contents of the image are not guaranteed to be preserved, but that doesnt matter since were going to clear it anyway. 
				// We want the image to be ready for presentation using the swap chain after rendering, which is why we use VK_IMAGE_LAYOUT_PRESENT_SRC_KHR as finalLayout.
				colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
				colorAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
										/*	VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL = Images used as color attachment (use this for color attachment with multisampling, adapt rendering pipeline)
											VK_IMAGE_LAYOUT_PRESENT_SRC_KHR = Images to be presented in the swap chain (use this for color attachment when no multisampling, adapt rendering pipeline)
											VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL = Images to be used as destination for a memory copy operation
										*/
				// A single render pass can consist of multiple subpasses.
				// Subpasses are subsequent rendering operations that depend on the contents of framebuffers in previous passes, for example a sequence of post-processing effects that are applied one after another. 
				// If you group these rendering operations into one render pass, then vulkan is able to reorder the operations and conserve memory bandwidth for possibly better preformance. 
				// Every subpass references one or more of the attachments that we've described using the structure in the previous sections. 
				// These references are themselves VkAttachmentReference structs that look like this:
			VkAttachmentReference colorAttachmentRef = {
				transparentRasterPass.AddAttachment(colorAttachment), // layout(location = 0) for output data in the fragment shader
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
			};
			// The attachment parameter specifies which attachment to reference by its index in the attachment descriptions array.
			// Our array consists of a single VkAttachmentDescription, so its index is 0. 
			// The layout specifies which layout we would like the attachment to have during a subpass that uses this reference.
			// Vulkan will automatically transition the attachment to this layout when the subpass is started.
			// We intend to use the attachment to function as a color buffer and the VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL layout will give us the best performance, as its name implies.

			
			// SubPass
			VkSubpassDescription subpass = {};
				subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
				subpass.colorAttachmentCount = 1;
				subpass.pColorAttachments = &colorAttachmentRef;
			transparentRasterPass.AddSubpass(subpass);
			// The following other types of attachments can be referenced by a subpass :
				// pInputAttachments : attachments that are read from a shader
				// pResolveAttachments : attachments used for multisampling color attachments
				// pDepthStencilAttachments : attachments for depth and stencil data
				// pPreserveAttachments : attachments that are not used by this subpass, but for which the data must be preserved
			// Render pass
			// Now that the attachment and a basic subpass refencing it have been described, we can create the render pass itself.
			// The render pass object can then be created by filling in the VkRenderPassCreateInfo structure with an array of attachments and subpasses. 
			// The VkAttachmentReference objects reference attachments using the indices of this array.

			// Render Pass Dependencies
			// There are two built-in dependencies that take care of the transition at the start of the render pass and at the end of the render pass, 
			// but the former does not occur at the right time. It assumes that the transition occurs at the start of the pipeline, but we haven't aquired the image yet at that point.
			// There are two ways to deal with this problem.
			// We could change the waitStage for the imageAvailableSemaphore to VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT to ensure that the render passes dont begin until the image is available, 
			// or we can make the render pass wait for the VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT stage, which is the option that we are using here.

			// Create the render pass
			transparentRasterPass.Create(renderingDevice);
			transparentRasterPass.CreateFrameBuffers(renderingDevice, *mainCamera.GetTmpImage());
			
			// Shaders
			for (auto* shaderPipeline : {&galaxyBoxShader}) {
				shaderPipeline->SetRenderPass(mainCamera.GetTmpImage(), transparentRasterPass.handle, 0);
				shaderPipeline->AddColorBlendAttachmentState();
				shaderPipeline->CreatePipeline(renderingDevice);
			}
		}
		
		if (false) {// Transparent Lighting pass
			// Color Attachment (Fragment shader Standard Output)
			VkAttachmentDescription colorAttachment = {};
				colorAttachment.format = mainCamera.GetTmpImage()->format;
				colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
				// Color and depth data
				colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
									/*	VK_ATTACHMENT_LOAD_OP_LOAD = Preserve the existing contents of the attachment
										VK_ATTACHMENT_LOAD_OP_CLEAR = Clear the values to a constant at the start
										VK_ATTACHMENT_LOAD_OP_DONT_CARE = Existing contents are undefined, we dont care about them (faster but may cause glitches if not set properly I guess)
									*/
				colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
									/*	VK_ATTACHMENT_STORE_OP_STORE = Rendered contents contents will be stored in memory and can be renad later (Need this to see something on the screen in the case of o_color)
										VK_ATTACHMENT_STORE_OP_DONT_CARE = Contents of the framebuffer will be undefined after rendering operation (Ignore this data ??)
									*/
				// Stencil data
				colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
				colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
				// Textures and framebuffers in Vulkan are represented by VkImage objects with a certain pixel format, however the layout of the pixels in memory can change based on what you're trying to do with an image.
				// The InitialLayout specifies wich layout the image will have before the render pass begins. 
				// The finalLayout specifies the layout to automatically transition to when the render pass finishes. 
				// Using VK_IMAGE_LAYOUT_UNDEFINED for initialLayout means that we dont care what previous layout the image was in. 
				// The caveat of this special value is that the contents of the image are not guaranteed to be preserved, but that doesnt matter since were going to clear it anyway. 
				// We want the image to be ready for presentation using the swap chain after rendering, which is why we use VK_IMAGE_LAYOUT_PRESENT_SRC_KHR as finalLayout.
				colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
				colorAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
										/*	VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL = Images used as color attachment (use this for color attachment with multisampling, adapt rendering pipeline)
											VK_IMAGE_LAYOUT_PRESENT_SRC_KHR = Images to be presented in the swap chain (use this for color attachment when no multisampling, adapt rendering pipeline)
											VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL = Images to be used as destination for a memory copy operation
										*/
				// A single render pass can consist of multiple subpasses.
				// Subpasses are subsequent rendering operations that depend on the contents of framebuffers in previous passes, for example a sequence of post-processing effects that are applied one after another. 
				// If you group these rendering operations into one render pass, then vulkan is able to reorder the operations and conserve memory bandwidth for possibly better preformance. 
				// Every subpass references one or more of the attachments that we've described using the structure in the previous sections. 
				// These references are themselves VkAttachmentReference structs that look like this:
			VkAttachmentReference colorAttachmentRef = {
				transparentLightingPass.AddAttachment(colorAttachment), // layout(location = 0) for output data in the fragment shader
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
			};
			// The attachment parameter specifies which attachment to reference by its index in the attachment descriptions array.
			// Our array consists of a single VkAttachmentDescription, so its index is 0. 
			// The layout specifies which layout we would like the attachment to have during a subpass that uses this reference.
			// Vulkan will automatically transition the attachment to this layout when the subpass is started.
			// We intend to use the attachment to function as a color buffer and the VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL layout will give us the best performance, as its name implies.

			
			// SubPass
			VkSubpassDescription subpass = {};
				subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
				subpass.colorAttachmentCount = 1;
				subpass.pColorAttachments = &colorAttachmentRef;
			transparentLightingPass.AddSubpass(subpass);
			// The following other types of attachments can be referenced by a subpass :
				// pInputAttachments : attachments that are read from a shader
				// pResolveAttachments : attachments used for multisampling color attachments
				// pDepthStencilAttachments : attachments for depth and stencil data
				// pPreserveAttachments : attachments that are not used by this subpass, but for which the data must be preserved
			// Render pass
			// Now that the attachment and a basic subpass refencing it have been described, we can create the render pass itself.
			// The render pass object can then be created by filling in the VkRenderPassCreateInfo structure with an array of attachments and subpasses. 
			// The VkAttachmentReference objects reference attachments using the indices of this array.

			// Render Pass Dependencies
			// There are two built-in dependencies that take care of the transition at the start of the render pass and at the end of the render pass, 
			// but the former does not occur at the right time. It assumes that the transition occurs at the start of the pipeline, but we haven't aquired the image yet at that point.
			// There are two ways to deal with this problem.
			// We could change the waitStage for the imageAvailableSemaphore to VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT to ensure that the render passes dont begin until the image is available, 
			// or we can make the render pass wait for the VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT stage, which is the option that we are using here.

			// Create the render pass
			transparentLightingPass.Create(renderingDevice);
			transparentLightingPass.CreateFrameBuffers(renderingDevice, *mainCamera.GetTmpImage());
			
			// Shaders
			for (auto* shaderPipeline : {&galaxyBoxShader}) {
				shaderPipeline->SetRenderPass(mainCamera.GetTmpImage(), transparentLightingPass.handle, 0);
				shaderPipeline->AddColorBlendAttachmentState();
				shaderPipeline->CreatePipeline(renderingDevice);
			}
		}
		
		{// Post Processing render pass
			// Color Attachment (Fragment shader Standard Output)
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
		for (auto* shaderPipeline : {&galaxyBoxShader}) {
			shaderPipeline->DestroyPipeline(renderingDevice);
		}
		opaqueRasterPass.DestroyFrameBuffers(renderingDevice);
		opaqueRasterPass.Destroy(renderingDevice);
		if (false) {
			opaqueLightingPass.DestroyFrameBuffers(renderingDevice);
			opaqueLightingPass.Destroy(renderingDevice);
		}
		if (false) {
			transparentRasterPass.DestroyFrameBuffers(renderingDevice);
			transparentRasterPass.Destroy(renderingDevice);
		}
		if (false) {
			transparentLightingPass.DestroyFrameBuffers(renderingDevice);
			transparentLightingPass.Destroy(renderingDevice);
		}
		
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
	}
	
private: // Commands
	void RecordComputeCommandBuffer(VkCommandBuffer, int imageIndex) override {}
	void RecordGraphicsCommandBuffer(VkCommandBuffer commandBuffer, int imageIndex) override {
		
		// Post Processing
		TransitionImageLayout(commandBuffer, *mainCamera.GetTmpImage(), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL);
		postProcessingRenderPass.Begin(renderingDevice, commandBuffer, swapChain, {{.0,.0,.0,.0}}, imageIndex);
		postProcessingShader.Execute(renderingDevice, commandBuffer);
		postProcessingRenderPass.End(renderingDevice, commandBuffer);
		
	}
	void RecordLowPriorityComputeCommandBuffer(VkCommandBuffer) override {}
	void RecordLowPriorityGraphicsCommandBuffer(VkCommandBuffer) override {}
	void RunDynamicCompute(VkCommandBuffer) override {}
	void RunDynamicGraphics(VkCommandBuffer commandBuffer) override {
		
		// Opaque Raster pass
		opaqueRasterPass.Begin(renderingDevice, commandBuffer, *mainCamera.GetTmpImage(), {{.0,.0,.0,.0}, {1.0,.0}});
		galaxyBoxShader.Execute(renderingDevice, commandBuffer, &galaxyBoxPushConstant);
		opaqueRasterPass.End(renderingDevice, commandBuffer);
		
		// // Opaque Lighting pass
		// opaqueLightingPass.Begin(renderingDevice, commandBuffer, ******, {{.0,.0,.0,.0}, {1.0,.0}});
		// ***.Execute(renderingDevice, commandBuffer);
		// opaqueLightingPass.End(renderingDevice, commandBuffer);
		
		// // Transparent Raster pass
		// transparentRasterPass.Begin(renderingDevice, commandBuffer, ******, {{.0,.0,.0,.0}, {1.0,.0}});
		// ***.Execute(renderingDevice, commandBuffer);
		// transparentRasterPass.End(renderingDevice, commandBuffer);
		
		// // Transparent Lighting pass
		// transparentLightingPass.Begin(renderingDevice, commandBuffer, ******, {{.0,.0,.0,.0}, {1.0,.0}});
		// ***.Execute(renderingDevice, commandBuffer);
		// transparentLightingPass.End(renderingDevice, commandBuffer);
		
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
		
		// // Update UBO
		// ViewUBO ubo {
		// 	mainCamera.GetProjectionMatrix(),
		// 	mainCamera.GetViewMatrix()
		// };
		// viewUniformBuffer.WriteToMappedData(renderingDevice, &ubo);
		
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
