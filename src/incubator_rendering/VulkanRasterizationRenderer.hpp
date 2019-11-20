#pragma once

#include "VulkanRenderer.hpp"
#include "VulkanDescriptorSet.hpp"
#include "VulkanShaderProgram.hpp"
#include "VulkanPipelineLayout.hpp"
#include "Geometry.hpp"

// Test Object Vertex Data Structure
struct Vertex {
	// glm::dvec3 pos;
	double posX, posY, posZ;
	glm::vec4 color;
};

struct UBO {
	glm::dmat4 proj;
	glm::dmat4 view;
	glm::dmat4 model;
};

class VulkanRasterizationRenderer : public VulkanRenderer {
	using VulkanRenderer::VulkanRenderer;
	
	VulkanBuffer uniformBuffer {VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, sizeof(UBO)};
	std::vector<VulkanBuffer*> stagedBuffers {};
	std::vector<Geometry*> geometries {};
	
	VulkanShaderProgram* testShader;
	VulkanShaderProgram* ppShader;
	CombinedImageSampler postProcessingSampler;
	
private: // Rasterization Rendering
	struct RenderingPipeline {
		VulkanGraphicsPipeline* graphicsPipeline = nullptr;
		VulkanShaderProgram* shaderProgram;
		
		RenderingPipeline(VulkanShaderProgram* shaderProgram) : shaderProgram(shaderProgram) {}
		virtual void Configure() = 0;
		virtual void Draw(VulkanDevice* device, VkCommandBuffer commandBuffer) = 0;
		virtual ~RenderingPipeline() {}
	};
	struct VertexRasterizationPipeline : public RenderingPipeline {
		VulkanBuffer* vertexBuffer;
		VulkanBuffer* indexBuffer;
		
		VertexRasterizationPipeline(VulkanShaderProgram* shaderProgram, VulkanBuffer* vertexBuffer, VulkanBuffer* indexBuffer)
		 : RenderingPipeline(shaderProgram), vertexBuffer(vertexBuffer), indexBuffer(indexBuffer) {}
		
		void Configure() {
			
		}
		
		void Draw(VulkanDevice* device, VkCommandBuffer commandBuffer) {
			// Test Object
			VkDeviceSize offsets[] = {0};
			device->CmdBindVertexBuffers(commandBuffer, 0, 1, &vertexBuffer->buffer, offsets);
			// device->CmdDraw(commandBuffer,
			// 	static_cast<uint32_t>(testObjectVertices.size()), // vertexCount
			// 	1, // instanceCount
			// 	0, // firstVertex (defines the lowest value of gl_VertexIndex)
			// 	0  // firstInstance (defines the lowest value of gl_InstanceIndex)
			// );
			device->CmdBindIndexBuffer(commandBuffer, indexBuffer->buffer, 0, VK_INDEX_TYPE_UINT32);
			device->CmdDrawIndexed(commandBuffer,
				static_cast<uint32_t>(indexBuffer->size / sizeof(uint32_t)), // indexCount
				1, // instanceCount
				0, // firstVertex (defines the lowest value of gl_VertexIndex)
				0, // vertexOffset
				0  // firstInstance (defines the lowest value of gl_InstanceIndex)
			);
			
			// // simple draw triangle...
			// device->CmdDraw(commandBuffer,
			// 	3, // vertexCount
			// 	1, // instanceCount
			// 	0, // firstVertex (defines the lowest value of gl_VertexIndex)
			// 	0  // firstInstance (defines the lowest value of gl_InstanceIndex)
			// );
		}
	};
	struct PostProcessingPipeline : public RenderingPipeline {

		PostProcessingPipeline(VulkanShaderProgram* shaderProgram)
		 : RenderingPipeline(shaderProgram) {}
		
		void Configure() {
			graphicsPipeline->rasterizer.cullMode = VK_CULL_MODE_FRONT_BIT;
			graphicsPipeline->rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
		}
		
		void Draw(VulkanDevice* device, VkCommandBuffer commandBuffer) {
			device->CmdDraw(commandBuffer, 3, 1, 0, 0);
		}
	};
	std::vector<VertexRasterizationPipeline> rasterizationPipelines {};
	std::vector<PostProcessingPipeline> postProcessingPipelines {};
	VulkanRenderPass* renderPass = nullptr;
	VulkanRenderPass* postProcessingRenderPass = nullptr;
	std::vector<VkFramebuffer> swapChainFrameBuffers, swapChainPostProcessingFrameBuffers;
	VkClearColorValue clearColor = {0,0,0,1};
	// Render Target (Color Attachment)
	VkImage colorImage = VK_NULL_HANDLE;
	VkDeviceMemory colorImageMemory = VK_NULL_HANDLE;
	VkImageView colorImageView = VK_NULL_HANDLE;
	VkFormat colorImageFormat;
	// tmp resolve image for multisampled deferred rendering
	VkImage colorImage2 = VK_NULL_HANDLE;
	VkDeviceMemory colorImageMemory2 = VK_NULL_HANDLE;
	VkImageView colorImageView2 = VK_NULL_HANDLE;
	// Depth Buffer
	VkImage depthImage = VK_NULL_HANDLE;
	VkDeviceMemory depthImageMemory = VK_NULL_HANDLE;
	VkImageView depthImageView = VK_NULL_HANDLE;
	VkFormat depthImageFormat;
	// MultiSampling
	VkSampleCountFlagBits msaaSamples = VK_SAMPLE_COUNT_2_BIT;
	const bool postProcessingEnabled = true;
	float renderingResolutionScale = !postProcessingEnabled?1: 2.0f;
	
	void CreateRasterizationResources() {
		VkImageViewCreateInfo viewInfo = {};
		
		// Format
		colorImageFormat = postProcessingEnabled ?
			  renderingGPU->FindSupportedFormat({VK_FORMAT_R32G32B32A32_SFLOAT}, VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT)
			: swapChain->format.format;
		
		int width = (int)((float)swapChain->extent.width * renderingResolutionScale);
		int height = (int)((float)swapChain->extent.height * renderingResolutionScale);
		
		renderingDevice->CreateImage(
			width,
			height,
			1, msaaSamples,
			colorImageFormat,
			VK_IMAGE_TILING_OPTIMAL,
			VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
			(postProcessingEnabled? 
				  (VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT)
				: (VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT)
			),
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			colorImage,
			colorImageMemory
		);
		viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewInfo.image = colorImage;
		viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		viewInfo.format = colorImageFormat;
		viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		viewInfo.subresourceRange.baseMipLevel = 0;
		viewInfo.subresourceRange.levelCount = 1;
		viewInfo.subresourceRange.baseArrayLayer = 0;
		viewInfo.subresourceRange.layerCount = 1;
		if (renderingDevice->CreateImageView(&viewInfo, nullptr, &colorImageView) != VK_SUCCESS) {
			throw std::runtime_error("Failed to create texture image view");
		}
		TransitionImageLayout(colorImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 1);
		
		if (postProcessingEnabled) {
			// tmp resolve image for multisampled deferred rendering
			renderingDevice->CreateImage(
				width,
				height,
				1, VK_SAMPLE_COUNT_1_BIT,
				colorImageFormat,
				VK_IMAGE_TILING_OPTIMAL,
				VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
				colorImage2,
				colorImageMemory2
			);
			viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			viewInfo.image = colorImage2;
			viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
			viewInfo.format = colorImageFormat;
			viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			viewInfo.subresourceRange.baseMipLevel = 0;
			viewInfo.subresourceRange.levelCount = 1;
			viewInfo.subresourceRange.baseArrayLayer = 0;
			viewInfo.subresourceRange.layerCount = 1;
			if (renderingDevice->CreateImageView(&viewInfo, nullptr, &colorImageView2) != VK_SUCCESS) {
				throw std::runtime_error("Failed to create texture image view");
			}
			TransitionImageLayout(colorImage2, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 1);
			
			// Create sampler
			VkSamplerCreateInfo sampler {};
			sampler.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
			sampler.maxAnisotropy = 1.0f;
			sampler.magFilter = VK_FILTER_LINEAR;
			sampler.minFilter = VK_FILTER_LINEAR;
			sampler.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
			sampler.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
			sampler.addressModeV = sampler.addressModeU;
			sampler.addressModeW = sampler.addressModeU;
			sampler.mipLodBias = 0.0f;
			sampler.maxAnisotropy = 1.0f;
			sampler.compareOp = VK_COMPARE_OP_NEVER;
			sampler.minLod = 0.0f;
			sampler.maxLod = 1.0f;
			sampler.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
			if (renderingDevice->CreateSampler(&sampler, nullptr, &postProcessingSampler.sampler) != VK_SUCCESS)
				throw std::runtime_error("Failed to create sampler");
			postProcessingSampler.imageView = (msaaSamples != VK_SAMPLE_COUNT_1_BIT)? colorImageView2 : colorImageView;
		}
		
		// Depth image Format
		depthImageFormat = renderingGPU->FindSupportedFormat({VK_FORMAT_D32_SFLOAT_S8_UINT}, VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
		// Depth Image
		renderingDevice->CreateImage(
			width, 
			height, 
			1, msaaSamples,
			depthImageFormat, 
			VK_IMAGE_TILING_OPTIMAL, 
			VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, 
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 
			depthImage, 
			depthImageMemory
		);
		// Depth Image View
		viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewInfo.image = depthImage;
		viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		viewInfo.format = depthImageFormat;
		viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
		viewInfo.subresourceRange.baseMipLevel = 0;
		viewInfo.subresourceRange.levelCount = 1;
		viewInfo.subresourceRange.baseArrayLayer = 0;
		viewInfo.subresourceRange.layerCount = 1;
		if (renderingDevice->CreateImageView(&viewInfo, nullptr, &depthImageView) != VK_SUCCESS) {
			throw std::runtime_error("Failed to create texture image view");
		}
		// Transition Layout
		TransitionImageLayout(depthImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, 1);
	}
	void DestroyRasterizationResources() {
		if (colorImage != VK_NULL_HANDLE) {
			renderingDevice->DestroyImageView(colorImageView, nullptr);
			renderingDevice->DestroyImage(colorImage, nullptr);
			renderingDevice->FreeMemory(colorImageMemory, nullptr);
			colorImage = VK_NULL_HANDLE;
		}
		if (postProcessingEnabled && colorImage2 != VK_NULL_HANDLE) {
			renderingDevice->DestroySampler(postProcessingSampler.sampler, nullptr);
			renderingDevice->DestroyImageView(colorImageView2, nullptr);
			renderingDevice->DestroyImage(colorImage2, nullptr);
			renderingDevice->FreeMemory(colorImageMemory2, nullptr);
			colorImage2 = VK_NULL_HANDLE;
		}
		if (depthImage != VK_NULL_HANDLE) {
			renderingDevice->DestroyImageView(depthImageView, nullptr);
			renderingDevice->DestroyImage(depthImage, nullptr);
			renderingDevice->FreeMemory(depthImageMemory, nullptr);
			depthImage = VK_NULL_HANDLE;
		}
	}
	void CreateRasterizationPipelines() {
		renderPass = new VulkanRenderPass(renderingDevice);

		// Color Attachment (Fragment shader Standard Output)
		VkAttachmentDescription colorAttachment = {}; // defines the output data from the fragment shader (o_color)
			colorAttachment.format = colorImageFormat;
			colorAttachment.samples = msaaSamples; // Need more with multisampling
			// Color and depth data
			colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; // What to do with the attachment before rendering
								/*	VK_ATTACHMENT_LOAD_OP_LOAD = Preserve the existing contents of the attachment
									VK_ATTACHMENT_LOAD_OP_CLEAR = Clear the values to a constant at the start
									VK_ATTACHMENT_LOAD_OP_DONT_CARE = Existing contents are undefined, we dont care about them (faster but may cause glitches if not set properly I guess)
								*/
			colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE; // What to do with the attachment after rendering
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
			colorAttachment.finalLayout = (postProcessingEnabled || msaaSamples != VK_SAMPLE_COUNT_1_BIT)? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
									/*	VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL = Images used as color attachment (use this for color attachment with multisampling, adapt rendering pipeline)
										VK_IMAGE_LAYOUT_PRESENT_SRC_KHR = Images to be presented in the swap chain (use this for color attachment when no multisampling, adapt rendering pipeline)
										VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL = Images to be used as destination for a memory copy operation
									*/
			// A single render pass can consist of multiple subpasses.
			// Subpasses are subsequent rendering operations that depend on the contents of framebuffers in previous passes, for example a sequence of post-processing effects that are applied one after another. 
			// If you group these rendering operations into one render pass, then vulkan is able to reorder the operations and conserve memory bandwidth for possibly better preformance. 
			// Every subpass references one or more of the attachments that we've described using the structure in the previous sections. 
			// These references are themselves VkAttachmentReference structs that look like this:
		renderPass->AddAttachment(colorAttachment);
		VkAttachmentReference colorAttachmentRef = {
			0, // layout(location = 0) for output data in the fragment shader
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
		};
		// The attachment parameter specifies which attachment to reference by its index in the attachment descriptions array.
		// Our array consists of a single VkAttachmentDescription, so its index is 0. 
		// The layout specifies which layout we would like the attachment to have during a subpass that uses this reference.
		// Vulkan will automatically transition the attachment to this layout when the subpass is started.
		// We intend to use the attachment to function as a color buffer and the VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL layout will give us the best performance, as its name implies.

		// Depth Attachment
		VkAttachmentDescription depthAttachment = {};
			depthAttachment.format = depthImageFormat;
			depthAttachment.samples = msaaSamples;
			depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		renderPass->AddAttachment(depthAttachment);
		VkAttachmentReference depthAttachmentRef = {1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};

		VkAttachmentReference colorAttachmentResolveRef;
		if (msaaSamples != VK_SAMPLE_COUNT_1_BIT) {
			// Resolve Attachment (Final Output Color)
			VkAttachmentDescription colorAttachmentResolve = {};
				colorAttachmentResolve.format = postProcessingEnabled? colorImageFormat : swapChain->format.format;
				colorAttachmentResolve.samples = VK_SAMPLE_COUNT_1_BIT;
				colorAttachmentResolve.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
				colorAttachmentResolve.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
				colorAttachmentResolve.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
				colorAttachmentResolve.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
				colorAttachmentResolve.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
				colorAttachmentResolve.finalLayout = postProcessingEnabled? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
			renderPass->AddAttachment(colorAttachmentResolve);
			colorAttachmentResolveRef = {2, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
		}
		
		// SubPass
		VkSubpassDescription subpass = {};
			subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS; // VK_PIPELINE_BIND_POINT_COMPUTE is also possible !!!
			subpass.colorAttachmentCount = 1;
			subpass.pColorAttachments = &colorAttachmentRef;
			subpass.pDepthStencilAttachment = &depthAttachmentRef;
			if (msaaSamples != VK_SAMPLE_COUNT_1_BIT) subpass.pResolveAttachments = &colorAttachmentResolveRef;
		renderPass->AddSubpass(subpass);
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
		VkSubpassDependency dependency = {};
			dependency.srcSubpass = VK_SUBPASS_EXTERNAL; // special value to refer to the implicit subpass before or after the render pass depending on wether it is specified in srcSubpass or dstSubpass.
			dependency.dstSubpass = 0; // index 0 refers to our subpass, which is the first and only one. It must always be higher than srcSubpass to prevent cucles in the dependency graph.
			// These two specify the operations to wait on and the stages in which these operations occur. 
			// We need to wait for the swap chain to finish reading from the image before we can access it. 
			// This can be accomplished by waiting on the color attachment output stage itself.
			dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			dependency.srcAccessMask = 0;
			// The operations that should wait on this are in the color attachment stage and involve reading and writing of the color attachment.
			// These settings will prevent the transition from happening until it's actually necessary (and allowed): when we want to start writing colors to it.
			dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		// Set dependency to the render pass info
		renderPass->renderPassInfo.dependencyCount = 1;
		renderPass->renderPassInfo.pDependencies = &dependency;

		// Create the render pass
		renderPass->Create();
		
		int width = (int)((float)swapChain->extent.width * renderingResolutionScale);
		int height = (int)((float)swapChain->extent.height * renderingResolutionScale);
		
		VkPipelineViewportStateCreateInfo viewportState = swapChain->viewportState;
		VkViewport viewport {};
		VkRect2D scissor {};
		if (postProcessingEnabled) {
			viewport.x = 0;
			viewport.y = 0;
			viewport.width = (float) width;
			viewport.height = (float) height;
			viewport.minDepth = 0;
			viewport.maxDepth = 1;
			scissor.offset = {0, 0};
			scissor.extent = {(uint)width, (uint)height};
			viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
			viewportState.viewportCount = 1;
			viewportState.scissorCount = 1;
			viewportState.pViewports = &viewport;
			viewportState.pScissors = &scissor;
		}
		
		// Frame Buffers
		swapChainFrameBuffers.resize(swapChain->imageViews.size());
		for (size_t i = 0; i < swapChain->imageViews.size(); i++) {
			
			std::vector<VkImageView> attachments {
				(msaaSamples != VK_SAMPLE_COUNT_1_BIT || postProcessingEnabled)? colorImageView : swapChain->imageViews[i],
				depthImageView,
			};
			if (msaaSamples != VK_SAMPLE_COUNT_1_BIT) attachments.push_back(postProcessingEnabled? colorImageView2 : swapChain->imageViews[i]);
			
			// You can only use a framebuffer with the render passes that it is compatible with, which roughly means that they use the same number and type of attachments
			VkFramebufferCreateInfo framebufferCreateInfo = {};
			framebufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			framebufferCreateInfo.renderPass = renderPass->handle;
			framebufferCreateInfo.attachmentCount = attachments.size();
			framebufferCreateInfo.pAttachments = attachments.data(); // Specifies the VkImageView objects that should be bound to the respective attachment descriptions in the render pass pAttachment array.
			framebufferCreateInfo.width = width;
			framebufferCreateInfo.height = height;
			framebufferCreateInfo.layers = 1; // refers to the number of layers in image arrays
			if (renderingDevice->CreateFramebuffer(&framebufferCreateInfo, nullptr, &swapChainFrameBuffers[i]) != VK_SUCCESS) {
				throw std::runtime_error("Failed to create framebuffer");
			}
		}
		
		// Shaders
		for (auto& pipeline : rasterizationPipelines) {
			auto* graphicsPipeline = renderPass->NewGraphicsPipeline(renderingDevice, 0);
			// Multisampling (AntiAliasing)
			graphicsPipeline->multisampling.rasterizationSamples = msaaSamples;
			// Pipeline Create Info
			graphicsPipeline->pipelineCreateInfo.pViewportState = &viewportState;
			// Color Blending
			graphicsPipeline->AddAlphaBlendingAttachment();
			// Shader stages
			pipeline.shaderProgram->CreateShaderStages(renderingDevice);
			graphicsPipeline->SetShaderProgram(pipeline.shaderProgram);
			// Configure
			pipeline.graphicsPipeline = graphicsPipeline;
			pipeline.Configure();
		}
		
		// Create Graphics Pipelines !
		renderPass->CreateGraphicsPipelines();
	}
	void DestroyRasterizationPipelines() {
		// Frame Buffer
		for (auto framebuffer : swapChainFrameBuffers) {
			renderingDevice->DestroyFramebuffer(framebuffer, nullptr);
		}
		swapChainFrameBuffers.clear();
		
		// Render pass
		renderPass->DestroyGraphicsPipelines();
		delete renderPass;
		
		// Shaders
		for (auto& pipeline : rasterizationPipelines) {
			pipeline.shaderProgram->DestroyShaderStages(renderingDevice);
		}
	}

private: // Renderer Configuration methods
	void Init() override {
		// Set all device features that you may want to use, then the unsupported features will be disabled, you may check via this object later.
		deviceFeatures.geometryShader = VK_TRUE;
		deviceFeatures.samplerAnisotropy = VK_TRUE;
		deviceFeatures.sampleRateShading = VK_TRUE;
		
		// Preferences
		preferredPresentModes = {
			// VK_PRESENT_MODE_MAILBOX_KHR,	// TripleBuffering (No Tearing, low latency)
			// VK_PRESENT_MODE_FIFO_KHR,		// VSync ON (No Tearing, more latency)
			VK_PRESENT_MODE_IMMEDIATE_KHR,	// VSync OFF (With Tearing, no latency)
		};
	}
	
	void ScoreGPUSelection(int& score, VulkanGPU* gpu) {
		// Build up a score here and the GPU with the highest score will be selected.
		// Add to the score optional specs, then multiply with mandatory specs.
		
		// Optional specs  -->  score += points * CONDITION
		score += 10 * (gpu->GetProperties().deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU); // Is a Dedicated GPU
		score += 20 * gpu->GetFeatures().tessellationShader; // Supports Tessellation
		score += gpu->GetProperties().limits.framebufferColorSampleCounts; // Add sample counts to the score (1-64)

		// Mandatory specs  -->  score *= CONDITION
		score *= gpu->GetFeatures().geometryShader; // Supports Geometry Shaders
		score *= gpu->GetFeatures().samplerAnisotropy; // Supports Anisotropic filtering
		score *= gpu->GetFeatures().sampleRateShading; // Supports Sample Shading
	}
	
	void Info() override {
		// MultiSampling
		msaaSamples = std::min(msaaSamples, renderingGPU->GetMaxUsableSampleCount());
	}

	void CreateResources() override {
		CreateRasterizationResources();
	}
	
	void DestroyResources() override {
		DestroyRasterizationResources();
	}
	
public: // Scene configuration methods
	void LoadScene() override {
		// Descriptor sets & Pipeline Layouts
		auto* descriptorSet = descriptorSets.emplace_back(new VulkanDescriptorSet(0));
		descriptorSet->AddBinding_uniformBuffer(0, &uniformBuffer, VK_SHADER_STAGE_VERTEX_BIT);
		VulkanPipelineLayout* mainPipelineLayout = pipelineLayouts.emplace_back(new VulkanPipelineLayout());
		mainPipelineLayout->AddDescriptorSet(descriptorSet);
		VulkanPipelineLayout* postProcessingPipelineLayout = nullptr;
		if (postProcessingEnabled) {
			auto* ppDescriptorSet = descriptorSets.emplace_back(new VulkanDescriptorSet(0));
			ppDescriptorSet->AddBinding_combinedImageSampler(0, &postProcessingSampler, VK_SHADER_STAGE_FRAGMENT_BIT);
			postProcessingPipelineLayout = pipelineLayouts.emplace_back(new VulkanPipelineLayout());
			postProcessingPipelineLayout->AddDescriptorSet(ppDescriptorSet);
		}
		
		VulkanBuffer* vertexBuffer = stagedBuffers.emplace_back(new VulkanBuffer(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT));
		VulkanBuffer* indexBuffer = stagedBuffers.emplace_back(new VulkanBuffer(VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT));
		
		// Add triangle geometries
		auto* trianglesGeometry1 = new TriangleGeometry<Vertex>({
			{/*pos*/-1.5,-1.5, 0.0, /*color*/{1.0, 0.0, 0.0, 1.0}},
			{/*pos*/ 1.5,-1.5, 0.0, /*color*/{1.0, 0.0, 0.0, 1.0}},
			{/*pos*/ 1.5, 1.5, 0.0, /*color*/{1.0, 0.0, 0.0, 1.0}},
			{/*pos*/-1.5, 1.5, 0.0, /*color*/{1.0, 0.0, 0.0, 1.0}},
			//
			{/*pos*/-1.5,-1.5,-0.2, /*color*/{0.0, 1.0, 0.0, 1.0}},
			{/*pos*/ 1.5,-1.5,-0.2, /*color*/{0.0, 1.0, 0.0, 1.0}},
			{/*pos*/ 1.5, 1.5,-0.2, /*color*/{0.0, 1.0, 0.0, 1.0}},
			{/*pos*/-1.5, 1.5,-0.2, /*color*/{0.0, 1.0, 0.0, 1.0}},
			
			// bottom white
			/*  8 */{/*pos*/-8.0,-8.0,-2.0, /*color*/{1.0,1.0,1.0, 1.0}},
			/*  9 */{/*pos*/ 8.0,-8.0,-2.0, /*color*/{1.0,1.0,1.0, 1.0}},
			/* 10 */{/*pos*/ 8.0, 8.0,-2.0, /*color*/{1.0,1.0,1.0, 1.0}},
			/* 11 */{/*pos*/-8.0, 8.0,-2.0, /*color*/{1.0,1.0,1.0, 1.0}},
			
			// top gray
			/* 12 */{/*pos*/-8.0,-8.0, 4.0, /*color*/{0.5,0.5,0.5, 1.0}},
			/* 13 */{/*pos*/ 8.0,-8.0, 4.0, /*color*/{0.5,0.5,0.5, 1.0}},
			/* 14 */{/*pos*/ 8.0, 8.0, 4.0, /*color*/{0.5,0.5,0.5, 1.0}},
			/* 15 */{/*pos*/-8.0, 8.0, 4.0, /*color*/{0.5,0.5,0.5, 1.0}},
			
			// left red
			/* 16 */{/*pos*/ 8.0, 8.0,-2.0, /*color*/{1.0,0.0,0.0, 1.0}},
			/* 17 */{/*pos*/ 8.0,-8.0,-2.0, /*color*/{1.0,0.0,0.0, 1.0}},
			/* 18 */{/*pos*/ 8.0, 8.0, 4.0, /*color*/{1.0,0.0,0.0, 1.0}},
			/* 19 */{/*pos*/ 8.0,-8.0, 4.0, /*color*/{1.0,0.0,0.0, 1.0}},
			
			// back blue
			/* 20 */{/*pos*/ 8.0,-8.0, 4.0, /*color*/{0.0,0.0,1.0, 1.0}},
			/* 21 */{/*pos*/ 8.0,-8.0,-2.0, /*color*/{0.0,0.0,1.0, 1.0}},
			/* 22 */{/*pos*/-8.0,-8.0, 4.0, /*color*/{0.0,0.0,1.0, 1.0}},
			/* 23 */{/*pos*/-8.0,-8.0,-2.0, /*color*/{0.0,0.0,1.0, 1.0}},
			
			// right green
			/* 24 */{/*pos*/-8.0, 8.0,-2.0, /*color*/{0.0,1.0,0.0, 1.0}},
			/* 25 */{/*pos*/-8.0,-8.0,-2.0, /*color*/{0.0,1.0,0.0, 1.0}},
			/* 26 */{/*pos*/-8.0, 8.0, 4.0, /*color*/{0.0,1.0,0.0, 1.0}},
			/* 27 */{/*pos*/-8.0,-8.0, 4.0, /*color*/{0.0,1.0,0.0, 1.0}},
		}, {
			0, 1, 2, 2, 3, 0,
			4, 5, 6, 6, 7, 4,
			8, 9, 10, 10, 11, 8,
			//
			13, 12, 14, 14, 12, 15,
			16, 17, 18, 18, 17, 19,
			20, 21, 22, 22, 21, 23,
			25, 24, 26, 26, 27, 25,
		}, vertexBuffer, 0, indexBuffer, 0);
		geometries.push_back(trianglesGeometry1);
		
		// Assign buffer data
		vertexBuffer->AddSrcDataPtr(&trianglesGeometry1->vertexData);
		indexBuffer->AddSrcDataPtr(&trianglesGeometry1->indexData);

		// Shader program
		testShader = new VulkanShaderProgram(mainPipelineLayout, {
			{"incubator_rendering/assets/shaders/raster.vert"},
			{"incubator_rendering/assets/shaders/raster.frag"},
		});

		// Vertex Input structure
		testShader->AddVertexInputBinding(sizeof(Vertex), VK_VERTEX_INPUT_RATE_VERTEX /*VK_VERTEX_INPUT_RATE_INSTANCE*/, {
			{0, offsetof(Vertex, Vertex::posX), VK_FORMAT_R64_SFLOAT},
			{1, offsetof(Vertex, Vertex::posY), VK_FORMAT_R64_SFLOAT},
			{2, offsetof(Vertex, Vertex::posZ), VK_FORMAT_R64_SFLOAT},
			{3, offsetof(Vertex, Vertex::color), VK_FORMAT_R32G32B32A32_SFLOAT},
		});
		rasterizationPipelines.emplace_back(testShader, vertexBuffer, indexBuffer);

		
		// Post processing
		if (postProcessingEnabled) {
			// Shader program
			ppShader = new VulkanShaderProgram(postProcessingPipelineLayout, {
				{"incubator_rendering/assets/shaders/postProcessing.vert"},
				{"incubator_rendering/assets/shaders/postProcessing.frag"},
			});
			postProcessingPipelines.emplace_back(ppShader);

			ppShader->LoadShaders();
		}
		
		testShader->LoadShaders();
	}

	void UnloadScene() override {
		// Shaders
		delete testShader;
		if (postProcessingEnabled) {
			delete ppShader;
		}
		
		// Geometries
		for (auto* geometry : geometries) {
			delete geometry;
		}
		geometries.clear();
		
		// Staged buffers
		for (auto* buffer : stagedBuffers) {
			delete buffer;
		}
		stagedBuffers.clear();
		
		// Pipeline Layouts
		for (auto* layout : pipelineLayouts) {
			delete layout;
		}
		pipelineLayouts.clear();
		
		// Descriptor sets
		for (auto* set : descriptorSets) {
			delete set;
		}
		descriptorSets.clear();
	}

protected: // Graphics configuration
	void CreateSceneGraphics() override {
		// Staged Buffers
		AllocateBuffersStaged(commandPool, stagedBuffers);
		// Uniform buffer
		uniformBuffer.Allocate(renderingDevice, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, false);
	}

	void DestroySceneGraphics() override {
		for (auto& buffer : stagedBuffers) {
			buffer->Free(renderingDevice);
		}

		// Uniform buffers
		uniformBuffer.Free(renderingDevice);
	}
	
	void CreateGraphicsPipelines() override {
		CreateRasterizationPipelines();
		
		if (postProcessingEnabled) {
			// Post-Processing Pipelines
			VkPipelineVertexInputStateCreateInfo emptyInputState {};
			emptyInputState.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
			emptyInputState.vertexAttributeDescriptionCount = 0;
			emptyInputState.pVertexAttributeDescriptions = nullptr;
			emptyInputState.vertexBindingDescriptionCount = 0;
			emptyInputState.pVertexBindingDescriptions = nullptr;
			
			// Create the render pass
			postProcessingRenderPass = new VulkanRenderPass(renderingDevice);
			
			// Color Attachment (Fragment shader Standard Output)
			VkAttachmentDescription colorAttachment = {}; // defines the output data from the fragment shader (o_color)
				colorAttachment.format = swapChain->format.format;
				colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT; // Need more with multisampling
				colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; // What to do with the attachment before rendering
				colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE; // What to do with the attachment after rendering
				colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
				colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
			postProcessingRenderPass->AddAttachment(colorAttachment);
			VkAttachmentReference colorAttachmentRef = {
				0, // layout(location = 0) for output data in the fragment shader
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
			};
			// SubPass
			VkSubpassDescription subpass = {};
				subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS; // VK_PIPELINE_BIND_POINT_COMPUTE is also possible !!!
				subpass.colorAttachmentCount = 1;
				subpass.pColorAttachments = &colorAttachmentRef;
			postProcessingRenderPass->AddSubpass(subpass);
			//
			VkSubpassDependency dependency = {};
				dependency.srcSubpass = VK_SUBPASS_EXTERNAL; // special value to refer to the implicit subpass before or after the render pass depending on wether it is specified in srcSubpass or dstSubpass.
				dependency.dstSubpass = 0; // index 0 refers to our subpass, which is the first and only one. It must always be higher than srcSubpass to prevent cucles in the dependency graph.
				// These two specify the operations to wait on and the stages in which these operations occur. 
				// We need to wait for the swap chain to finish reading from the image before we can access it. 
				// This can be accomplished by waiting on the color attachment output stage itself.
				dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
				dependency.srcAccessMask = 0;
				// The operations that should wait on this are in the color attachment stage and involve reading and writing of the color attachment.
				// These settings will prevent the transition from happening until it's actually necessary (and allowed): when we want to start writing colors to it.
				dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
				dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			// Set dependency to the render pass info
			postProcessingRenderPass->renderPassInfo.dependencyCount = 1;
			postProcessingRenderPass->renderPassInfo.pDependencies = &dependency;

			postProcessingRenderPass->Create();

			// Frame Buffers
			swapChainPostProcessingFrameBuffers.resize(swapChain->imageViews.size());
			for (size_t i = 0; i < swapChain->imageViews.size(); i++) {
				// You can only use a framebuffer with the render passes that it is compatible with, which roughly means that they use the same number and type of attachments
				VkFramebufferCreateInfo framebufferCreateInfo = {};
				framebufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
				framebufferCreateInfo.renderPass = postProcessingRenderPass->handle;
				framebufferCreateInfo.attachmentCount = 1;
				framebufferCreateInfo.pAttachments = &swapChain->imageViews[i]; // Specifies the VkImageView objects that should be bound to the respective attachment descriptions in the render pass pAttachment array.
				framebufferCreateInfo.width = swapChain->extent.width;
				framebufferCreateInfo.height = swapChain->extent.height;
				framebufferCreateInfo.layers = 1; // refers to the number of layers in image arrays
				if (renderingDevice->CreateFramebuffer(&framebufferCreateInfo, nullptr, &swapChainPostProcessingFrameBuffers[i]) != VK_SUCCESS) {
					throw std::runtime_error("Failed to create framebuffer");
				}
			}
			
			// Shaders
			for (auto& pipeline : postProcessingPipelines) {
				auto* graphicsPipeline = postProcessingRenderPass->NewGraphicsPipeline(renderingDevice, 0);
				// pipeline configuration
				graphicsPipeline->multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
				graphicsPipeline->pipelineCreateInfo.pViewportState = &swapChain->viewportState;
				graphicsPipeline->pipelineCreateInfo.pVertexInputState = &emptyInputState;
				graphicsPipeline->AddAlphaBlendingAttachment();
				// Shader stages
				pipeline.shaderProgram->CreateShaderStages(renderingDevice);
				graphicsPipeline->SetShaderProgram(pipeline.shaderProgram);
				// Configure
				pipeline.graphicsPipeline = graphicsPipeline;
				pipeline.Configure();
			}

			// Create Graphics Pipelines !
			postProcessingRenderPass->CreateGraphicsPipelines();
		}
	}
	
	void DestroyGraphicsPipelines() override {
		DestroyRasterizationPipelines();
		
		// Post-Processing Pipelines
		if (postProcessingEnabled) {
			// Frame Buffer
			for (auto framebuffer : swapChainPostProcessingFrameBuffers) {
				renderingDevice->DestroyFramebuffer(framebuffer, nullptr);
			}
			swapChainPostProcessingFrameBuffers.clear();
			
			// Render pass
			postProcessingRenderPass->DestroyGraphicsPipelines();
			delete postProcessingRenderPass;

			// Shaders
			for (auto& pipeline : postProcessingPipelines) {
				pipeline.shaderProgram->DestroyShaderStages(renderingDevice);
			}
		}
	}
	
	void RenderingCommandBuffer(VkCommandBuffer commandBuffer, int imageIndex) override {
		
		int width = (int)((float)swapChain->extent.width * renderingResolutionScale);
		int height = (int)((float)swapChain->extent.height * renderingResolutionScale);
		
		// Begin Render Pass
		VkRenderPassBeginInfo renderPassInfo = {};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassInfo.renderPass = renderPass->handle;
		renderPassInfo.framebuffer = swapChainFrameBuffers[imageIndex]; // We create a framebuffer for each swap chain image that specifies it as color attachment
		// Defines the size of the render area, which defines where shader loads and stores will take place. The pixels outside this region will have undefined values. It should match the size of the attachments for best performance.
		renderPassInfo.renderArea.offset = {0, 0};
		renderPassInfo.renderArea.extent = {(uint)width, (uint)height};
		// Related to VK_ATTACHMENT_LOAD_OP_CLEAR for the color attachment
		std::array<VkClearValue, 2> clearValues = {};
		clearValues[0].color = clearColor;
		clearValues[1].depthStencil = {1.0f/*depth*/, 0/*stencil*/}; // The range of depth is 0 to 1 in Vulkan, where 1 is far and 0 is near. We want to clear to the Far value.
		renderPassInfo.clearValueCount = clearValues.size();
		renderPassInfo.pClearValues = clearValues.data();
		//
		renderingDevice->CmdBeginRenderPass(commandBuffers[imageIndex], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE); // Returns void, so no error handling for any vkCmdXxx functions until we've finished recording.
															/*	the last parameter controls how the drawing commands within the render pass will be provided.
																VK_SUBPASS_CONTENTS_INLINE = The render pass commands will be embedded in the primary command buffer itself and no secondary command buffers will be executed
																VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS = The render pass commands will be executed from secondary command buffers
															*/
		
		/////////////////////////////////////////
		
		for (auto& pipeline : rasterizationPipelines) {
			pipeline.graphicsPipeline->Bind(renderingDevice, commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS);
			// Draw
			pipeline.Draw(renderingDevice, commandBuffer);
		}
		/////////////////////////////////////////
		
		renderingDevice->CmdEndRenderPass(commandBuffers[imageIndex]);
		
		if (postProcessingEnabled) {
			auto resolvedImage = (msaaSamples != VK_SAMPLE_COUNT_1_BIT)? colorImage2 : colorImage;
			TransitionImageLayout(commandBuffer, resolvedImage, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, 1);
			
			// Begin Render Pass
			VkRenderPassBeginInfo ppRenderPassInfo = {};
			ppRenderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
			ppRenderPassInfo.renderPass = postProcessingRenderPass->handle;
			ppRenderPassInfo.framebuffer = swapChainPostProcessingFrameBuffers[imageIndex]; // We create a framebuffer for each swap chain image that specifies it as color attachment
			// Defines the size of the render area, which defines where shader loads and stores will take place. The pixels outside this region will have undefined values. It should match the size of the attachments for best performance.
			ppRenderPassInfo.renderArea.offset = {0, 0};
			ppRenderPassInfo.renderArea.extent = {swapChain->extent.width, swapChain->extent.height};
			// Related to VK_ATTACHMENT_LOAD_OP_CLEAR for the color attachment
			std::array<VkClearValue, 2> clearValues = {};
			clearValues[0].color = clearColor;
			clearValues[1].depthStencil = {1.0f/*depth*/, 0/*stencil*/}; // The range of depth is 0 to 1 in Vulkan, where 1 is far and 0 is near. We want to clear to the Far value.
			ppRenderPassInfo.clearValueCount = clearValues.size();
			ppRenderPassInfo.pClearValues = clearValues.data();
			//
			renderingDevice->CmdBeginRenderPass(commandBuffers[imageIndex], &ppRenderPassInfo, VK_SUBPASS_CONTENTS_INLINE); // Returns void, so no error handling for any vkCmdXxx functions until we've finished recording.
			
			// post processing here
			for (auto& pipeline : postProcessingPipelines) {
				pipeline.graphicsPipeline->Bind(renderingDevice, commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS);
				pipeline.Draw(renderingDevice, commandBuffer);
			}
			
			renderingDevice->CmdEndRenderPass(commandBuffers[imageIndex]);
		}
	}
	
protected: // Methods executed on every frame
	void FrameUpdate(uint imageIndex) override {
		static auto startTime = std::chrono::high_resolution_clock::now();
		auto currentTime = std::chrono::high_resolution_clock::now();
		double time = std::chrono::duration<double, std::chrono::seconds::period>(currentTime - startTime).count();
		UBO ubo = {};
		// Slowly rotate the test object
		ubo.model = glm::rotate(glm::dmat4(1.0), time * glm::radians(10.0), glm::dvec3(0.0,0.0,1.0));
		
		// Current camera position
		ubo.view = glm::lookAt(camPosition, camPosition + camDirection, glm::dvec3(0,0,1));
		// Projection
		ubo.proj = glm::perspective(glm::radians(80.0), (double) swapChain->extent.width / (double) swapChain->extent.height, 0.01, 1.5e17); // 1cm - 1 000 000 UA  (WTF!!! seems to be working great..... 32bit z-buffer is enough???)
		ubo.proj[1][1] *= -1;

		// Update memory
		VulkanBuffer::CopyDataToBuffer(renderingDevice, &ubo, &uniformBuffer);
	}

public: // user-defined state variables
	glm::dvec3 camPosition = glm::dvec3(2,2,2);
	glm::dvec3 camDirection = glm::dvec3(-2,-2,-2);
	
};
