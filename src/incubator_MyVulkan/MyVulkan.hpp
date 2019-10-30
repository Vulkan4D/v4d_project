#pragma once

#include "Vulkan.hpp"
#include "Texture2D.hpp"

#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

// Test Object Vertex Data Structure
struct Vertex {
	glm::vec3 pos;
	glm::vec4 color;
	glm::vec2 uv;
	
	bool operator==(const Vertex& other) const {
		return pos == other.pos && color == other.color && uv == other.uv;
	}
};
namespace std {
	template<> struct hash<Vertex> {
		size_t operator()(Vertex const &vertex) const {
			return ((hash<glm::vec3>()(vertex.pos) ^
					(hash<glm::vec3>()(vertex.color) << 1)) >> 1) ^
					(hash<glm::vec2>()(vertex.uv) << 1);
		}
	};
}

class MyVulkan : public Vulkan {
private: // class members
	Window* window;

	// Main Render Surface
	VulkanSurface* surface;

	// Main Graphics Card
	VulkanGPU* mainGPU = nullptr; // automatically deleted in base class
	VulkanDevice* device;

	// Queues
	VulkanQueue graphicsQueue;
	VulkanQueue presentationQueue;

	// Pools
	VkCommandPool graphicsCommandPool, transferCommandPool;
	VkDescriptorPool descriptorPool;

	// Sync objects
	std::vector<VkSemaphore> imageAvailableSemaphores;
	std::vector<VkSemaphore> renderFinishedSemaphores;
	std::vector<VkFence> inFlightFences;
	size_t currentFrameInFlight = 0;

	// Swap Chains
	VulkanSwapChain* swapChain = nullptr; // make sure this one is initialized to nullptr

	// Render pass (and graphics pipelines)
	VulkanRenderPass* renderPass;

	// Framebuffers
	std::vector<VkFramebuffer> swapChainFrameBuffers;

	// Command buffers
	std::vector<VkCommandBuffer> commandBuffers;

	// Render Target (Color Attachment)
	VkImage colorImage;
	VkDeviceMemory colorImageMemory;
	VkImageView colorImageView;

	// MultiSampling
	VkSampleCountFlagBits msaaSamples = VK_SAMPLE_COUNT_8_BIT;

	// Depth Buffer
	VkImage depthImage;
	VkDeviceMemory depthImageMemory;
	VkImageView depthImageView;
	VkFormat depthImageFormat;

	// Constants
	const int MAX_FRAMES_IN_FLIGHT = 2;

private: // Test Object members
	VulkanShaderProgram* testShader;
	// Vertex Buffers for test object
	std::vector<Vertex> testObjectVertices;
	std::vector<uint32_t> testObjectIndices;
	std::unordered_map<Vertex, uint32_t> testObjectUniqueVertices;
	VkBuffer vertexBuffer, indexBuffer;
	VkDeviceMemory vertexBufferMemory, indexBufferMemory;
	struct UBO {
		glm::mat4 model;
		glm::mat4 view;
		glm::mat4 proj;
	};
	std::vector<VkBuffer> uniformBuffers;
	std::vector<VkDeviceMemory> uniformBuffersMemory;
	std::vector<VkDescriptorSet> descriptorSets;
	// Textures
	Texture2D* texture;


protected: // Virtual INIT Methods

	virtual void CreateGraphicsDevices() {
		// Select The Best Main GPU using a score system
		mainGPU = SelectSuitableGPU([surface=surface](int &score, VulkanGPU* gpu){
			// Build up a score here and the GPU with the highest score will be selected.
			// Add to the score optional specs, then multiply with mandatory specs.

			// Optional specs  -->  score += points * CONDITION
			score += 100 * (gpu->GetProperties().deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU); // Is a Dedicated GPU
			score += 20 * gpu->GetFeatures().tessellationShader; // Supports Tessellation
			score += gpu->GetProperties().limits.framebufferColorSampleCounts; // Add sample counts to the score (1-64)

			// Mandatory specs  -->  score *= CONDITION
			score *= gpu->QueueFamiliesContainsFlags(VK_QUEUE_GRAPHICS_BIT, 1, surface->GetHandle()); // Supports Graphics Queues
			score *= gpu->GetFeatures().geometryShader; // Supports Geometry Shaders
			score *= gpu->GetFeatures().samplerAnisotropy; // Supports Anisotropic filtering
			score *= gpu->GetFeatures().sampleRateShading; // Supports Sample Shading
			score *= gpu->SupportsExtension(VK_KHR_SWAPCHAIN_EXTENSION_NAME/*"VK_KHR_swapchain"*/); // Supports SwapChains
			score *= gpu->SupportsExtension(VK_NV_RAY_TRACING_EXTENSION_NAME/*"VK_NV_ray_tracing"*/); // Supports RayTracing
		});

		LOG("Selected Main GPU: " << mainGPU->GetDescription());

		// Prepare Device Features
		VkPhysicalDeviceFeatures deviceFeatures = {};
		deviceFeatures.geometryShader = VK_TRUE;
		deviceFeatures.samplerAnisotropy = VK_TRUE;
		deviceFeatures.sampleRateShading = VK_TRUE;
		// Create Logical Device
		device = new VulkanDevice(
			mainGPU,
			deviceFeatures,
			{ // Device-specific Extensions string[] (NOT VULKAN INSTANCE EXTs)
				VK_KHR_SWAPCHAIN_EXTENSION_NAME, // "VK_KHR_swapchain"
				VK_NV_RAY_TRACING_EXTENSION_NAME, // "VK_NV_ray_tracing"
			},
			vulkanRequiredLayers,
			{// Queues
				{
					"graphics",
					VK_QUEUE_GRAPHICS_BIT, // Flags
					1, // Count
					{1.0f}, // Priorities (one per queue count)
					surface->GetHandle() // Putting a surface here forces the need for a presentation feature on that specific queue, null if no need, or get presentation queue separately with device->GetPresentationQueue(surface)
				},
			}
		);

		// Get Graphics Queue
		graphicsQueue = device->GetQueue("graphics");

		// Get Presentation Queue
		presentationQueue = graphicsQueue; // Use same as graphics (as configured above), otherwise uncomment block below to use a separate queue for presentation (apparently, using the same queue is supposed to be faster...)
		// presentationQueue = device->GetPresentationQueue(surface);
		// if (presentationQueue == nullptr) {
		// 	throw std::runtime_error("Failed to get Presentation Queue for surface");
		// }

		// MultiSampling
		msaaSamples = std::min(VK_SAMPLE_COUNT_8_BIT, mainGPU->GetMaxUsableSampleCount());
	}

	virtual void DestroyGraphicsDevices() {
		delete device;
	}

	virtual void CreatePools() {
		device->CreateCommandPool(graphicsQueue.index, 0, &graphicsCommandPool);
		device->CreateCommandPool(graphicsQueue.index, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, &transferCommandPool);
		device->CreateDescriptorPool({VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER}, swapChain->imageViews.size(), descriptorPool, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT);
	}

	virtual void DestroyPools() {
		device->DestroyCommandPool(graphicsCommandPool);
		device->DestroyCommandPool(transferCommandPool);
		device->DestroyDescriptorPool(descriptorPool);
	}

	virtual void CreateSyncObjects() {
		// each of the events in the RenderFrame method are set in motion using a single function call, but they are executed asynchronously. 
		// The function calls will return before the operations are actually finished and the order of execution is also undefined.
		// Hence, we need to synchronize them.
		// There are two ways of synchronizing swap chain events : Fences and Semaphores.
		// The state of fences can be accessed from our program using calls like vkWaitForFences, but not for semaphores.
		// Semaphores are used to synchronize operations within or across command queues.
		// We have one semaphore to signal that an image has been aquired and is ready for rendering, and another one to signal that rendering has finished and presentation can happen.

		imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
		renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
		inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

		VkSemaphoreCreateInfo semaphoreInfo = {};
		semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

		VkFenceCreateInfo fenceInfo = {};
		fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT; // Initialize in the signaled state so that we dont get stuck on the first frame

		for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
			if (device->CreateSemaphore(&semaphoreInfo, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS) {
				throw std::runtime_error("Failed to create semaphore for ImageAvailable");
			}
			if (device->CreateSemaphore(&semaphoreInfo, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS) {
				throw std::runtime_error("Failed to create semaphore for RenderFinished");
			}
			if (device->CreateFence(&fenceInfo, nullptr, &inFlightFences[i]) != VK_SUCCESS) {
				throw std::runtime_error("Failed to create InFlightFence");
			}
		}
	}

	virtual void DestroySyncObjects() {
		for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
			device->DestroySemaphore(renderFinishedSemaphores[i], nullptr);
			device->DestroySemaphore(imageAvailableSemaphores[i], nullptr);
			device->DestroyFence(inFlightFences[i], nullptr);
		}
	}

	virtual void CreateSwapChain() {
		// Pause if screen is minimized
		while (window->GetWidth() == 0 || window->GetHeight() == 0) {
			window->WaitEvents();
		}

		// Put old swapchain in a temporary pointer and delete it after creating new swapchain
		VulkanSwapChain* oldSwapChain = swapChain;

		// Create the new swapchain object
		swapChain = new VulkanSwapChain(
			device,
			surface,
			{ // Preferred Extent (Screen Resolution)
				(uint32_t)window->GetWidth(),
				(uint32_t)window->GetHeight()
			},
			{ // Preferred Formats
				{VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR},
			},
			{ // Preferred Present Modes
				VK_PRESENT_MODE_MAILBOX_KHR,	// TripleBuffering (No Tearing, low latency)
				VK_PRESENT_MODE_IMMEDIATE_KHR,	// VSync OFF (With Tearing, no latency)
				VK_PRESENT_MODE_FIFO_KHR,		// VSync ON (No Tearing, more latency)
			}
		);
		// Assign queues
		swapChain->AssignQueues({graphicsQueue.index, presentationQueue.index});
		// Set custom params
		// swapChain->createInfo.xxxx = xxxxxx...
		// swapChain->imageViewsCreateInfo.xxxx = xxxxxx...

		// Create the swapchain handle and corresponding imageviews
		swapChain->Create(oldSwapChain);

		// Destroy the old swapchain
		if (oldSwapChain != nullptr) delete oldSwapChain;
	}

	virtual void DestroySwapChain() {
		delete swapChain;
	}

	// Graphics Pipeline

	virtual void CreateRenderPass() {
		renderPass = new VulkanRenderPass(device);

		// Color Attachment (Fragment shader Standard Output)
		VkAttachmentDescription colorAttachment = {}; // defines the output data from the fragment shader (o_color)
			colorAttachment.format = swapChain->format.format;
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

		// Resolve Attachment (Final Output Color)
		VkAttachmentDescription colorAttachmentResolve = {};
			colorAttachmentResolve.format = swapChain->format.format;
			colorAttachmentResolve.samples = VK_SAMPLE_COUNT_1_BIT;
			colorAttachmentResolve.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			colorAttachmentResolve.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			colorAttachmentResolve.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			colorAttachmentResolve.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			colorAttachmentResolve.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			colorAttachmentResolve.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		renderPass->AddAttachment(colorAttachmentResolve);
		VkAttachmentReference colorAttachmentResolveRef = {2, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};

		// SubPass
		VkSubpassDescription subpass = {};
			subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS; // VK_PIPELINE_BIND_POINT_COMPUTE is also possible !!!
			subpass.colorAttachmentCount = 1;
			subpass.pColorAttachments = &colorAttachmentRef;
			subpass.pDepthStencilAttachment = &depthAttachmentRef;
			subpass.pResolveAttachments = &colorAttachmentResolveRef;
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

		//////////////
		// Not sure if this block is really necessary......
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
		//////////////
		
		// Set dependency to the render pass info
		renderPass->renderPassInfo.dependencyCount = 1;
		renderPass->renderPassInfo.pDependencies = &dependency;

		renderPass->Create();

	}

	virtual void DestroyRenderPass() {
		delete renderPass;
	}

	virtual void CreateGraphicsPipelines() { 
		auto graphicsPipeline1 = new VulkanGraphicsPipeline(device); // if we use renderPass->AddGraphicsPipeline(), it will be deleted with renderPass->DestroyGraphicsPipelines()
		//////////////////////////////////////////////////////////////////////////////////////////////////////////
		// Shader stages (programmable stages of the graphics pipeline)
		
		graphicsPipeline1->SetShaderProgram(testShader);

		// Input Assembly
		graphicsPipeline1->inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		graphicsPipeline1->inputAssembly.primitiveRestartEnable = VK_FALSE; // If set to VK_TRUE, then it's possible to break up lines and triangles in the _STRIP topology modes by using a special index of 0xFFFF or 0xFFFFFFFF.

		// Color Blending
		graphicsPipeline1->AddAlphaBlendingAttachment(); // Fragment Shader output 0

		//////////////////////////////////////////////////////////////////////////////////////////////////////////
		// Fixed-function state
		// Global graphics settings when starting the game

		// Rasterizer
		graphicsPipeline1->rasterizer.depthClampEnable = VK_FALSE; // if set to VK_TRUE, then fragments that are beyond the near and far planes are clamped to them as opposed to discarding them. This is useful in some special cases like shadow maps. Using this requires enabling a GPU feature.
		graphicsPipeline1->rasterizer.rasterizerDiscardEnable = VK_FALSE; // if set to VK_TRUE, then geometry never passes through the rasterizer stage. This basically disables any output to the framebuffer.
		graphicsPipeline1->rasterizer.polygonMode = VK_POLYGON_MODE_FILL; // Using any mode other than fill requires enabling a GPU feature.
		graphicsPipeline1->rasterizer.lineWidth = 1; // The maximum line width that is supported depends on the hardware and any line thicker than 1.0f requires you to enable the wideLines GPU feature.
		graphicsPipeline1->rasterizer.cullMode = VK_CULL_MODE_BACK_BIT; // Face culling
		graphicsPipeline1->rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE; // Vertex faces draw order
		graphicsPipeline1->rasterizer.depthBiasEnable = VK_FALSE;
		graphicsPipeline1->rasterizer.depthBiasConstantFactor = 0;
		graphicsPipeline1->rasterizer.depthBiasClamp = 0;
		graphicsPipeline1->rasterizer.depthBiasSlopeFactor = 0;

		// Multisampling (AntiAliasing)
		graphicsPipeline1->multisampling.sampleShadingEnable = VK_TRUE;
		graphicsPipeline1->multisampling.minSampleShading = 0.2f; // Min fraction for sample shading (0-1). Closer to one is smoother.
		graphicsPipeline1->multisampling.rasterizationSamples = msaaSamples;
		graphicsPipeline1->multisampling.pSampleMask = nullptr;
		graphicsPipeline1->multisampling.alphaToCoverageEnable = VK_FALSE;
		graphicsPipeline1->multisampling.alphaToOneEnable = VK_FALSE;

		// Depth and Stencil testing
		VkPipelineDepthStencilStateCreateInfo depthStencil = {};
			depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
			depthStencil.depthTestEnable = VK_TRUE;
			depthStencil.depthWriteEnable = VK_TRUE;
			depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
			// Used for the optional depth bound test. This allows to only keep fragments that fall within the specified depth range.
			depthStencil.depthBoundsTestEnable = VK_FALSE;
				// depthStencil.minDepthBounds = 0.0f;
				// depthStencil.maxDepthBounds = 1.0f;
			// Stencil Buffer operations
			depthStencil.stencilTestEnable = VK_FALSE;
			depthStencil.front = {};
			depthStencil.back = {};

		//////////////////////////////////////////////////////////////////////////////////////////////////////////
		// Dynamic settings that CAN be changed at runtime but NOT every frame

		// // Dynamic State
		// VkDynamicState dynamicStates[] = {
		// 	VK_DYNAMIC_STATE_VIEWPORT,
		// 	VK_DYNAMIC_STATE_SCISSOR,
		// };
		// VkPipelineDynamicStateCreateInfo dynamicState = {};
		// dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
		// dynamicState.dynamicStateCount = 2;
		// dynamicState.pDynamicStates = dynamicStates;

		//////////////////////////////////////////////////////////////////////////////////////////////////////////
		// Create DAS GRAFIKS PIPELINE !!!
		// Fixed-function stage
		graphicsPipeline1->pipelineCreateInfo.pViewportState = &swapChain->viewportState;
		graphicsPipeline1->pipelineCreateInfo.pDepthStencilState = &depthStencil;
		graphicsPipeline1->pipelineCreateInfo.pDynamicState = nullptr; // or &dynamicState
		
		// Optional.
		// Vulkan allows you to create a new graphics pipeline by deriving from an existing pipeline. 
		// The idea of pipeline derivatives is that it is less expensive to set up pipelines when they have much functionality in common with an existing pipeline and switching between pipelines from the same parent can also be done quicker.
		// You can either specify the handle of an existing pipeline with basePipelineHandle or reference another pipeline that is about to be created by index with basePipelineIndex. 
		// These are only used if VK_PIPELINE_CREATE_DERIVATIVE_BIT flag is also specified in the flags field of VkGraphicsPipelineCreateInfo.
		graphicsPipeline1->pipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;
		graphicsPipeline1->pipelineCreateInfo.basePipelineIndex = -1;

		graphicsPipeline1->Prepare();
		renderPass->AddGraphicsPipeline(graphicsPipeline1, 0);

		// Create the Graphics Pipeline !
		renderPass->CreateGraphicsPipelines();
	}

	virtual void DestroyGraphicsPipelines() {
		renderPass->DestroyGraphicsPipelines();
	}

	virtual void CreateFrameBuffers() {
		swapChainFrameBuffers.resize(swapChain->imageViews.size());
		for (size_t i = 0; i < swapChain->imageViews.size(); i++) {
			std::array<VkImageView, 3> attachments = {
				colorImageView,
				depthImageView,
				swapChain->imageViews[i],
			};
			// You can only use a framebuffer with the render passes that it is compatible with, which roughly means that they use the same number and type of attachments
			VkFramebufferCreateInfo framebufferCreateInfo = {};
			framebufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			framebufferCreateInfo.renderPass = renderPass->handle;
			framebufferCreateInfo.attachmentCount = attachments.size();
			framebufferCreateInfo.pAttachments = attachments.data(); // Specifies the VkImageView objects that should be bound to the respective attachment descriptions in the render pass pAttachment array.
			framebufferCreateInfo.width = swapChain->extent.width;
			framebufferCreateInfo.height = swapChain->extent.height;
			framebufferCreateInfo.layers = 1; // refers to the number of layers in image arrays
			if (device->CreateFramebuffer(&framebufferCreateInfo, nullptr, &swapChainFrameBuffers[i]) != VK_SUCCESS) {
				throw std::runtime_error("Failed to create framebuffer");
			}
		}
	}

	virtual void DestroyFrameBuffers() {
		for (auto framebuffer : swapChainFrameBuffers) {
			device->DestroyFramebuffer(framebuffer, nullptr);
		}
	}

	virtual void CreateCommandBuffers() {
		// Because one of the drawing commands involves binding the right VkFramebuffer, we'll actually have to record a command buffer for every image in the swap chain once again.
		// Command buffers will be automatically freed when their command pool is destroyed, so we don't need an explicit cleanup
		commandBuffers.resize(swapChainFrameBuffers.size());
		VkCommandBufferAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.commandPool = graphicsCommandPool;
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
						/*	VK_COMMAND_BUFFER_LEVEL_PRIMARY = Can be submitted to a queue for execution, but cannot be called from other command buffers
							VK_COMMAND_BUFFER_LEVEL_SECONDARY = Cannot be submitted directly, but can be called from primary command buffers
						*/
		allocInfo.commandBufferCount = (uint) commandBuffers.size();
		if (device->AllocateCommandBuffers(&allocInfo, commandBuffers.data()) != VK_SUCCESS) {
			throw std::runtime_error("Failed to allocate command buffers");
		}

		// Starting command buffer recording...
		for (size_t i = 0; i < commandBuffers.size(); i++) {
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
			if (device->BeginCommandBuffer(commandBuffers[i], &beginInfo) != VK_SUCCESS) {
				throw std::runtime_error("Faild to begin recording command buffer");
			}

			// Begin Render Pass
			VkRenderPassBeginInfo renderPassInfo = {};
			renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
			renderPassInfo.renderPass = renderPass->handle;
			renderPassInfo.framebuffer = swapChainFrameBuffers[i]; // We create a framebuffer for each swap chain image that specifies it as color attachment
			// Defines the size of the render area, which defines where shader loads and stores will take place. The pixels outside this region will have undefined values. It should match the size of the attachments for best performance.
			renderPassInfo.renderArea.offset = {0, 0};
			renderPassInfo.renderArea.extent = swapChain->extent;
			// Related to VK_ATTACHMENT_LOAD_OP_CLEAR for the color attachment
			std::array<VkClearValue, 2> clearValues = {};
			clearValues[0].color = {0,0,0,1}; // Clear Color
			clearValues[1].depthStencil = {1.0f/*depth*/, 0/*stencil*/}; // The range of depth is 0 to 1 in Vulkan, where 1 is far and 0 is near. We want to clear to the Far value.
			renderPassInfo.clearValueCount = clearValues.size();
			renderPassInfo.pClearValues = clearValues.data();
			//
			device->CmdBeginRenderPass(commandBuffers[i], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE); // Returns void, so no error handling for any vkCmdXxx functions until we've finished recording.
																/*	the last parameter controls how the drawing commands within the render pass will be provided.
																	VK_SUBPASS_CONTENTS_INLINE = The render pass commands will be embedded in the primary command buffer itself and no secondary command buffers will be executed
																	VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS = The render pass commands will be executed from secondary command buffers
																*/
			// We can now bind the graphics pipeline
			device->CmdBindPipeline(commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, renderPass->graphicsPipelines[0]->handle); // The second parameter specifies if the pipeline object is a graphics or compute pipeline.
			
			// Bind Descriptor Sets (Uniforms)
			device->CmdBindDescriptorSets(commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, renderPass->graphicsPipelines[0]->pipelineLayout, 0/*firstSet*/, 1/*count*/, &descriptorSets[i], 0, nullptr);

			// Test Object
			VkBuffer vertexBuffers[] = {vertexBuffer};
			VkDeviceSize offsets[] = {0};
			device->CmdBindVertexBuffers(commandBuffers[i], 0, 1, vertexBuffers, offsets);
			// device->CmdDraw(commandBuffers[i],
			// 	static_cast<uint32_t>(testObjectVertices.size()), // vertexCount
			// 	1, // instanceCount
			// 	0, // firstVertex (defines the lowest value of gl_VertexIndex)
			// 	0  // firstInstance (defines the lowest value of gl_InstanceIndex)
			// );
			device->CmdBindIndexBuffer(commandBuffers[i], indexBuffer, 0, VK_INDEX_TYPE_UINT32);
			device->CmdDrawIndexed(commandBuffers[i],
				static_cast<uint32_t>(testObjectIndices.size()), // indexCount
				1, // instanceCount
				0, // firstVertex (defines the lowest value of gl_VertexIndex)
				0, // vertexOffset
				0  // firstInstance (defines the lowest value of gl_InstanceIndex)
			);


			// // Draw One Single F***ing Triangle !
			// device->CmdDraw(commandBuffers[i],
			// 	3, // vertexCount
			// 	1, // instanceCount
			// 	0, // firstVertex (defines the lowest value of gl_VertexIndex)
			// 	0  // firstInstance (defines the lowest value of gl_InstanceIndex)
			// );


			// End the render pass
			device->CmdEndRenderPass(commandBuffers[i]);
			if (device->EndCommandBuffer(commandBuffers[i]) != VK_SUCCESS) {
				throw std::runtime_error("Failed to record command buffer");
			}
		}
	}

	virtual void DestroyCommandBuffers() {
		device->FreeCommandBuffers(graphicsCommandPool, static_cast<uint32_t>(commandBuffers.size()), commandBuffers.data());
	}

	virtual void CreateColorResources() {
		VkFormat colorFormat = swapChain->format.format;
		device->CreateImage(
			swapChain->extent.width, 
			swapChain->extent.height, 
			1, msaaSamples,
			colorFormat,
			VK_IMAGE_TILING_OPTIMAL,
			VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			colorImage,
			colorImageMemory
		);

		VkImageViewCreateInfo viewInfo = {};
			viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			viewInfo.image = colorImage;
			viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
			viewInfo.format = colorFormat;
			viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			viewInfo.subresourceRange.baseMipLevel = 0;
			viewInfo.subresourceRange.levelCount = 1;
			viewInfo.subresourceRange.baseArrayLayer = 0;
			viewInfo.subresourceRange.layerCount = 1;
		if (device->CreateImageView(&viewInfo, nullptr, &colorImageView) != VK_SUCCESS) {
			throw std::runtime_error("Failed to create texture image view");
		}
	}

	virtual void DestroyColorResources() {
		device->DestroyImageView(colorImageView, nullptr);
		device->DestroyImage(colorImage, nullptr);
		device->FreeMemory(colorImageMemory, nullptr);
	}

	virtual void CreateDepthResources() {
		// Format
		depthImageFormat = mainGPU->FindSupportedFormat({VK_FORMAT_D32_SFLOAT_S8_UINT}, VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);

		// Image
		device->CreateImage(
			swapChain->extent.width, 
			swapChain->extent.height, 
			1, msaaSamples,
			depthImageFormat, 
			VK_IMAGE_TILING_OPTIMAL, 
			VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, 
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 
			depthImage, 
			depthImageMemory
		);

		// Image View
		VkImageViewCreateInfo viewInfo = {};
		viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewInfo.image = depthImage;
		viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		viewInfo.format = depthImageFormat;
		viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
		viewInfo.subresourceRange.baseMipLevel = 0;
		viewInfo.subresourceRange.levelCount = 1;
		viewInfo.subresourceRange.baseArrayLayer = 0;
		viewInfo.subresourceRange.layerCount = 1;
		if (device->CreateImageView(&viewInfo, nullptr, &depthImageView) != VK_SUCCESS) {
			throw std::runtime_error("Failed to create texture image view");
		}

		// Transition Layout
		TransitionImageLayout(depthImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, 1);
	}

	virtual void DestroyDepthResources() {
		device->DestroyImageView(depthImageView, nullptr);
		device->DestroyImage(depthImage, nullptr);
		device->FreeMemory(depthImageMemory, nullptr);
	}

private: // Helper methods

	VkCommandBuffer BeginSingleTimeCommands(VkCommandPool commandPool) {
		VkCommandBufferAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandPool = commandPool;
		allocInfo.commandBufferCount = 1;

		VkCommandBuffer commandBuffer;
		device->AllocateCommandBuffers(&allocInfo, &commandBuffer);

		VkCommandBufferBeginInfo beginInfo = {};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

		device->BeginCommandBuffer(commandBuffer, &beginInfo);

		return commandBuffer;
	}

	void EndSingleTimeCommands(VkCommandPool commandPool, VkCommandBuffer commandBuffer) {
		device->EndCommandBuffer(commandBuffer);

		VkSubmitInfo submitInfo = {};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &commandBuffer;

		device->QueueSubmit(graphicsQueue.handle, 1, &submitInfo, VK_NULL_HANDLE);
		device->QueueWaitIdle(graphicsQueue.handle); // TODO: Using a fence instead would allow to schedule multiple transfers simultaneously and wait for all of them to complete, instead of executing one at a time.

		device->FreeCommandBuffers(commandPool, 1, &commandBuffer);
	}

	void TransitionImageLayout(VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t mipLevels) {
		auto commandBuffer = BeginSingleTimeCommands(transferCommandPool);

		VkImageMemoryBarrier barrier = {};
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.oldLayout = oldLayout; // VK_IMAGE_LAYOUT_UNDEFINED if we dont care about existing contents of the image
		barrier.newLayout = newLayout;
		// If we are using the barrier to transfer queue family ownership, these two fields should be the indices of the queue families. Otherwise VK_QUEUE_FAMILY_IGNORED
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		//
		barrier.image = image;
		barrier.subresourceRange.baseMipLevel = 0;
		barrier.subresourceRange.levelCount = mipLevels;
		barrier.subresourceRange.baseArrayLayer = 0;
		barrier.subresourceRange.layerCount = 1;
		//
		if (newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
			barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
		} else {
			barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		}
		//
		VkPipelineStageFlags srcStage, dstStage;
		// Texture Transfer
		if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
			barrier.srcAccessMask = 0;
			barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
			dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		} else 
		// Texture Fragment Shader
		if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
			dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		} else 
		// Depth Buffer
		if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
			barrier.srcAccessMask = 0;
			barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
			srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
			dstStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
		} else {
			throw std::invalid_argument("Unsupported layout transition");
		}
		/*
		Transfer writes must occur in the pipeline transfer stage. 
		Since the writes dont have to wait on anything, we mayy specify an empty access mask and the earliest possible pipeline stage VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT for the pre-barrier operations.
		It should be noted that VK_PIPELINE_STAGE_TRANSFER_BIT is not a Real stage within the graphics and compute pipelines.
		It is more of a pseudo-stage where transfers happen.
		
		The image will be written in the same pipeline stage and subsequently read by the fragment shader, which is why we specify shader reading access in the fragment pipeline stage.
		If we need to do more transitions in the future, then we'll extend the function.
		
		One thing to note is that command buffer submission results in implicit VK_ACCESS_HOST_WRITE_BIT synchronization at the beginning.
		Since the TransitionImageLayout function executes a command buffer with only a single command, we could use this implicit synchronization and set srcAccessMask to 0 if we ever needed a VK_ACCESS_HOST_WRITE_BIT dependency in a layout transition.

		There is actually a special type of image layout that supports all operations, VK_IMAGE_LAYOUT_GENERAL.
		The problem with it is that it doesnt necessarily offer the best performance for any operation.
		It is required for some special cases, like using an image as both input and output, or for reading an image after it has left the preinitialized layout.
		*/
		//
		device->CmdPipelineBarrier(
			commandBuffer,
			srcStage, dstStage,
			0,
			0, nullptr,
			0, nullptr,
			1, &barrier
		);

		EndSingleTimeCommands(transferCommandPool, commandBuffer);
	}

	void CopyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) {
		auto commandBuffer = BeginSingleTimeCommands(transferCommandPool);

		VkBufferCopy copyRegion = {};
		copyRegion.srcOffset = 0;
		copyRegion.dstOffset = 0;
		copyRegion.size = size;
		device->CmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

		EndSingleTimeCommands(transferCommandPool, commandBuffer);
	}

	void CopyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height) {
		auto commandBuffer = BeginSingleTimeCommands(transferCommandPool);

		VkBufferImageCopy region = {};
		region.bufferOffset = 0;
		region.bufferRowLength = 0;
		region.bufferImageHeight = 0;
		
		region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		region.imageSubresource.mipLevel = 0;
		region.imageSubresource.baseArrayLayer = 0;
		region.imageSubresource.layerCount = 1;

		region.imageOffset = {0, 0, 0};
		region.imageExtent = {width, height, 1};

		device->CmdCopyBufferToImage(
			commandBuffer,
			buffer,
			image,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			1,
			&region
		);

		EndSingleTimeCommands(transferCommandPool, commandBuffer);
	}

	void GenerateMipmaps(Texture2D* texture) {
		GenerateMipmaps(texture->GetImage(), texture->GetFormat(), texture->GetWidth(), texture->GetHeight(), texture->GetMipLevels());
	}

	void GenerateMipmaps(VkImage image, VkFormat imageFormat, int32_t width, int32_t height, uint32_t mipLevels) {
		VkFormatProperties formatProperties;
		mainGPU->GetPhysicalDeviceFormatProperties(imageFormat, &formatProperties);
		if (!(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT)) {
			throw std::runtime_error("Texture image format does not support linear blitting");
		}

		auto commandBuffer = BeginSingleTimeCommands(transferCommandPool);

		VkImageMemoryBarrier barrier = {};
			barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			barrier.image = image;
			barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			barrier.subresourceRange.baseArrayLayer = 0;
			barrier.subresourceRange.layerCount = 1;
			barrier.subresourceRange.levelCount = 1;

		int32_t mipWidth = width;
		int32_t mipHeight = height;

		for (uint32_t i = 1; i < mipLevels; i++) {
			barrier.subresourceRange.baseMipLevel = i - 1;
			barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

			device->CmdPipelineBarrier(
				commandBuffer, 
				VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
				0, nullptr,
				0, nullptr,
				1, &barrier
			);

			VkImageBlit blit = {};
				blit.srcOffsets[0] = { 0, 0, 0 };
				blit.srcOffsets[1] = { mipWidth, mipHeight, 1 };
				blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				blit.srcSubresource.mipLevel = i - 1;
				blit.srcSubresource.baseArrayLayer = 0;
				blit.srcSubresource.layerCount = 1;
				blit.dstOffsets[0] = { 0, 0, 0 };
				blit.dstOffsets[1] = { mipWidth>1? mipWidth/2 : 1, mipHeight>1? mipHeight/2 : 1, 1 };
				blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				blit.dstSubresource.mipLevel = i;
				blit.dstSubresource.baseArrayLayer = 0;
				blit.dstSubresource.layerCount = 1;

			device->CmdBlitImage(
				commandBuffer,
				image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
				image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				1, &blit,
				VK_FILTER_LINEAR
			);

			barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
			barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
			barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

			device->CmdPipelineBarrier(
				commandBuffer,
				VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
				0, nullptr,
				0, nullptr,
				1, &barrier
			);

			if (mipWidth > 1) mipWidth /= 2;
			if (mipHeight > 1) mipHeight /= 2;
		}

		barrier.subresourceRange.baseMipLevel = mipLevels - 1;
		barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		device->CmdPipelineBarrier(
			commandBuffer,
			VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
			0, nullptr,
			0, nullptr,
			1, &barrier
		);

		EndSingleTimeCommands(transferCommandPool, commandBuffer);
	}

private: // Test Object methods

	void LoadTestObject() {
		// 3D Model
		tinyobj::attrib_t attrib;
		std::vector<tinyobj::shape_t> shapes;
		std::vector<tinyobj::material_t> materials;
		std::string err, warn;
		if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, "../../src/incubator_MyVulkan/models/chalet.obj")) {
			throw std::runtime_error(err);
		}
		if (warn != "") LOG_WARN(warn);
		for (const auto &shape : shapes) {
			for (const auto &index : shape.mesh.indices) {
				Vertex vertex = {};
				vertex.pos = {
					attrib.vertices[3 * index.vertex_index + 0],
					attrib.vertices[3 * index.vertex_index + 1],
					attrib.vertices[3 * index.vertex_index + 2],
				};
				vertex.uv = {
					attrib.texcoords[2 * index.texcoord_index + 0],
					1.0f - attrib.texcoords[2 * index.texcoord_index + 1], // flipped vertical component
				};
				vertex.color = {1, 1, 1, 1};
				if (testObjectUniqueVertices.count(vertex) == 0) {
					testObjectUniqueVertices[vertex] = testObjectVertices.size();
					testObjectVertices.push_back(vertex);
				}
				testObjectIndices.push_back(testObjectUniqueVertices[vertex]);
			}
		}
		// testObjectVertices = {
		// 	{{-0.5f,-0.5f, 0.0f}, {1.0f, 0.0f, 0.0f, 1}, {1.0f, 0.0f}},
		// 	{{ 0.5f,-0.5f, 0.0f}, {0.0f, 1.0f, 0.0f, 1}, {0.0f, 0.0f}},
		// 	{{ 0.5f, 0.5f, 0.0f}, {0.0f, 0.0f, 1.0f, 1}, {0.0f, 1.0f}},
		// 	{{-0.5f, 0.5f, 0.0f}, {0.0f, 1.0f, 1.0f, 1}, {1.0f, 1.0f}},
		// 	//
		// 	{{-0.5f,-0.5f,-0.5f}, {1.0f, 0.0f, 0.0f, 1}, {1.0f, 0.0f}},
		// 	{{ 0.5f,-0.5f,-0.5f}, {0.0f, 1.0f, 0.0f, 1}, {0.0f, 0.0f}},
		// 	{{ 0.5f, 0.5f,-0.5f}, {0.0f, 0.0f, 1.0f, 1}, {0.0f, 1.0f}},
		// 	{{-0.5f, 0.5f,-0.5f}, {0.0f, 1.0f, 1.0f, 1}, {1.0f, 1.0f}},
		// };
		// testObjectIndices = {
		// 	0, 1, 2, 2, 3, 0,
		// 	4, 5, 6, 6, 7, 4,
		// };

		// Texture
		texture = new Texture2D("../../src/incubator_MyVulkan/models/chalet.jpg", STBI_rgb_alpha);
		texture->SetMipLevels();
		texture->SetSamplerAnisotropy(16);
		texture->AllocateVulkanStagingMemory(device);
		texture->CreateVulkanImage(device, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		texture->CreateVulkanImageView(device);
		texture->CreateVulkanSampler(device);
		TransitionImageLayout(texture->GetImage(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, texture->GetMipLevels());
		CopyBufferToImage(texture->GetStagingBuffer(), texture->GetImage(), texture->GetWidth(), texture->GetHeight());
		// TransitionImageLayout(texture->GetImage(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, texture->GetMipLevels());
		GenerateMipmaps(texture); // This automatically does the transition to VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
		texture->FreeVulkanStagingMemory(device);

		// Shader program
		testShader = new VulkanShaderProgram(device, {
			{"../../src/incubator_MyVulkan/shaders/test.vert"},
			{"../../src/incubator_MyVulkan/shaders/test.frag"},
		});

		// Vertex Input structure
		testShader->AddVertexInputBinding(sizeof(Vertex), VK_VERTEX_INPUT_RATE_VERTEX /*VK_VERTEX_INPUT_RATE_INSTANCE*/, {
			{0, offsetof(Vertex, Vertex::pos), VK_FORMAT_R32G32B32_SFLOAT},
			{1, offsetof(Vertex, Vertex::color), VK_FORMAT_R32G32B32A32_SFLOAT},
			{2, offsetof(Vertex, Vertex::uv), VK_FORMAT_R32G32_SFLOAT},
		});

		// Uniforms
		testShader->AddLayoutBindings({
			{// ubo
				0, // binding
				VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, // descriptorType
				1, // count (for array)
				VK_SHADER_STAGE_VERTEX_BIT, // VK_SHADER_STAGE_FRAGMENT_BIT, VK_SHADER_STAGE_GEOMETRY_BIT, VK_SHADER_STAGE_ALL_GRAPHICS, ...
				nullptr, // pImmutableSamplers
			},
			{// tex
				1, // binding
				VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, // descriptorType
				1, // count (for array)
				VK_SHADER_STAGE_FRAGMENT_BIT, // VK_SHADER_STAGE_FRAGMENT_BIT, VK_SHADER_STAGE_GEOMETRY_BIT, VK_SHADER_STAGE_ALL_GRAPHICS, ...
				nullptr, // pImmutableSamplers
			},
		});
		
	}

	void UnloadTestObject() {
		texture->DestroyVulkanSampler(device);
		texture->DestroyVulkanImageView(device);
		texture->DestroyVulkanImage(device);
		delete texture;
		delete testShader;
	}

	void AddTestObject() {
		// Vertices
		VkDeviceSize bufferSize = sizeof(testObjectVertices[0]) * testObjectVertices.size();
		VkBuffer stagingBuffer;
		VkDeviceMemory stagingBufferMemory;
		device->CreateBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);
		void *data;
		device->MapMemory(stagingBufferMemory, 0, bufferSize, 0, &data);
			memcpy(data, testObjectVertices.data(), bufferSize);
		device->UnmapMemory(stagingBufferMemory);
		device->CreateBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, vertexBuffer, vertexBufferMemory);
		CopyBuffer(stagingBuffer, vertexBuffer, bufferSize);
		device->DestroyBuffer(stagingBuffer, nullptr);
		device->FreeMemory(stagingBufferMemory, nullptr);

		// Indices
		/*VkDeviceSize*/ bufferSize = sizeof(testObjectIndices[0]) * testObjectIndices.size();
		// VkBuffer stagingBuffer;
		// VkDeviceMemory stagingBufferMemory;
		device->CreateBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);
		// void *data;
		device->MapMemory(stagingBufferMemory, 0, bufferSize, 0, &data);
			memcpy(data, testObjectIndices.data(), bufferSize);
		device->UnmapMemory(stagingBufferMemory);
		device->CreateBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, indexBuffer, indexBufferMemory);
		CopyBuffer(stagingBuffer, indexBuffer, bufferSize);
		device->DestroyBuffer(stagingBuffer, nullptr);
		device->FreeMemory(stagingBufferMemory, nullptr);

		// Uniform buffers
		/*VkDeviceSize*/ bufferSize = sizeof(UBO);
		uniformBuffers.resize(swapChain->imageViews.size());
		uniformBuffersMemory.resize(uniformBuffers.size());
		for (size_t i = 0; i < uniformBuffers.size(); i++) {
			device->CreateBuffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, uniformBuffers[i], uniformBuffersMemory[i]);
		}

		// Descriptor sets (for Uniform Buffers)
		std::vector<VkDescriptorSetLayout> layouts;
		layouts.reserve(swapChain->imageViews.size() * testShader->GetDescriptorSetLayouts().size());
		for (size_t i = 0; i < swapChain->imageViews.size(); i++) {
			for (auto layout : testShader->GetDescriptorSetLayouts()) {
				layouts.push_back(layout);
			}
		}
		VkDescriptorSetAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.descriptorPool = descriptorPool;
		allocInfo.descriptorSetCount = layouts.size();
		allocInfo.pSetLayouts = layouts.data();
		descriptorSets.resize(swapChain->imageViews.size());
		if (device->AllocateDescriptorSets(&allocInfo, descriptorSets.data()) != VK_SUCCESS) {
			throw std::runtime_error("Failed to allocate descriptor sets");
		}
		for (size_t i = 0; i < swapChain->imageViews.size(); i++) {
			std::array<VkWriteDescriptorSet, 2> descriptorWrites = {};

			// ubo
			descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			descriptorWrites[0].dstSet = descriptorSets[i];
			descriptorWrites[0].dstBinding = 0; // layout(binding = 0) uniform...
			descriptorWrites[0].dstArrayElement = 0; // array
			descriptorWrites[0].descriptorCount = 1; // array
			descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			VkDescriptorBufferInfo bufferInfo = {uniformBuffers[i], 0, sizeof(UBO)};
			descriptorWrites[0].pBufferInfo = &bufferInfo;

			// tex
			descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			descriptorWrites[1].dstSet = descriptorSets[i];
			descriptorWrites[1].dstBinding = 1; // layout(binding = 0) uniform...
			descriptorWrites[1].dstArrayElement = 0; // array
			descriptorWrites[1].descriptorCount = 1; // array
			descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			VkDescriptorImageInfo imageInfo = {texture->GetSampler(), texture->GetImageView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
			descriptorWrites[1].pImageInfo = &imageInfo;
			
			// Update Descriptor Sets
			device->UpdateDescriptorSets(descriptorWrites.size(), descriptorWrites.data(), 0, nullptr);
		}
	}

	void RemoveTestObject() {
		// Uniform buffers
		for (size_t i = 0; i < uniformBuffers.size(); i++) {
			device->DestroyBuffer(uniformBuffers[i], nullptr);
			device->FreeMemory(uniformBuffersMemory[i], nullptr);
		}

		// Descriptor Sets
		device->FreeDescriptorSets(descriptorPool, descriptorSets.size(), descriptorSets.data());

		// Vertices
		device->DestroyBuffer(vertexBuffer, nullptr);
		device->FreeMemory(vertexBufferMemory, nullptr);

		// Indices
		device->DestroyBuffer(indexBuffer, nullptr);
		device->FreeMemory(indexBufferMemory, nullptr);
	}

	void UpdateUniformBuffer(uint i) {
		static auto startTime = std::chrono::high_resolution_clock::now();
		auto currentTime = std::chrono::high_resolution_clock::now();
		float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();
		UBO ubo = {};
		ubo.model = glm::rotate(glm::mat4(1), time * glm::radians(10.0f), glm::vec3(0,0,1));
		ubo.view = glm::lookAt(glm::vec3(2,2,2), glm::vec3(0,0,0), glm::vec3(0,0,1));
		ubo.proj = glm::perspective(glm::radians(45.0f), (float) swapChain->extent.width / (float) swapChain->extent.height, 0.1f, 10.0f);
		ubo.proj[1][1] *= -1;
		// Update memory
		void *data;
		device->MapMemory(uniformBuffersMemory[i], 0/*offset*/, sizeof(ubo), 0/*flags*/, &data);
			memcpy(data, &ubo, sizeof(ubo));
		device->UnmapMemory(uniformBuffersMemory[i]);
	}

protected: // Reset Methods
	virtual void RecreateSwapChains() {
		device->DeviceWaitIdle();

		// Destroy graphics pipeline
		DestroyCommandBuffers();
		DestroyFrameBuffers();
		DestroyGraphicsPipelines();
		DestroyRenderPass();
		DestroyDepthResources();
		DestroyColorResources();

		// Test Object
		RemoveTestObject();

		// SwapChain
		CreateSwapChain();

		// Test Object
		AddTestObject();

		// Graphics pipeline
		CreateColorResources();
		CreateDepthResources();
		CreateRenderPass(); // shader attachments are assigned here
		CreateGraphicsPipelines(); // shaders are assigned here
		CreateFrameBuffers();
		CreateCommandBuffers(); // objects are rendered here
	}
public: // Constructor & Destructor
	MyVulkan(xvk::Loader* loader, const char* applicationName, uint applicationVersion, Window* window) : Vulkan(loader, applicationName, applicationVersion), window(window) {

		// Surfaces
		surface = new VulkanSurface(this, window);

		// Create Objects
		CreateGraphicsDevices();
		CreateSyncObjects();
		CreateSwapChain();
		CreatePools();

		// Test Shader
		LoadTestObject();

		// Test Object
		AddTestObject();

		// Graphics pipeline
		CreateColorResources();
		CreateDepthResources();
		CreateRenderPass(); // shader attachments are assigned here
		CreateGraphicsPipelines(); // shaders are assigned here
		CreateFrameBuffers();
		CreateCommandBuffers(); // objects are rendered here

		// Ready to render!
		LOG_SUCCESS("VULKAN IS READY TO RENDER!");
	}

	virtual ~MyVulkan() override {
		// Wait for device to be idle before destroying everything
		device->DeviceWaitIdle(); // We can also wait for operations in a specific command queue to be finished with vkQueueWaitIdle. These functions can be used as a very rudimentary way to perform synchronization. 
		
		// Destroy graphics pipeline
		DestroyCommandBuffers();
		DestroyFrameBuffers();
		DestroyGraphicsPipelines();
		DestroyRenderPass();
		DestroyDepthResources();
		DestroyColorResources();

		// Test Object
		RemoveTestObject();

		// Test Shader
		UnloadTestObject();

		// Destroy remaining stuff
		DestroyPools();
		DestroySwapChain();
		DestroySyncObjects();
		DestroyGraphicsDevices();
		delete surface;
	}

public: // Public Methods
	virtual void RenderFrame() {
		// Wait for previous frame to be finished
		device->WaitForFences(1/*fencesCount*/, &inFlightFences[currentFrameInFlight]/*fences array*/, VK_TRUE/*wait for all fences in this array*/, std::numeric_limits<uint64_t>::max()/*timeout*/);

		// Get an image from the swapchain
		uint imageIndex;
		VkResult result = device->AcquireNextImageKHR(
			swapChain->GetHandle(), // swapChain
			std::numeric_limits<uint64_t>::max(), // timeout in nanoseconds (using max disables the timeout)
			imageAvailableSemaphores[currentFrameInFlight], // semaphore
			VK_NULL_HANDLE, // fence
			&imageIndex // output the index of the swapchain image in there
		);

		// Check for errors
		if (result == VK_ERROR_OUT_OF_DATE_KHR) {
			// SwapChain is out of date, for instance if the window was resized, stop here and ReCreate the swapchain.
			RecreateSwapChains();
			return;
		} else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
			throw std::runtime_error("Failed to acquire swap chain images");
		}

		// Update Uniforms
		UpdateUniformBuffer(imageIndex);

		// Submit the command buffer
		VkSubmitInfo submitInfo = {};
		// first 3 params specify which semaphores to wait on before execution begins and in which stage(s) of the pipeline to wait.
		// We want to wait with writing colors to the image until it's available, so we're specifying the stage of the graphics pipeline that writes to the color attachment.
		// That means that theoretically the implementation can already start executing our vertex shader and such while the image is not yet available.
		// Each entry in the waitStages array corresponds to the semaphore with the same index in pWaitSemaphores.
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		VkSemaphore waitSemaphores[] = {imageAvailableSemaphores[currentFrameInFlight]};
		VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
		submitInfo.waitSemaphoreCount = 1;
		submitInfo.pWaitSemaphores = waitSemaphores;
		submitInfo.pWaitDstStageMask = waitStages;
		// specify which command buffers to actually submit for execution
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &commandBuffers[imageIndex];
		// specify which semaphore to signal once the command buffer(s) have finished execution.
		VkSemaphore signalSemaphores[] = {renderFinishedSemaphores[currentFrameInFlight]};
		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = signalSemaphores;
		
		// Reset the fence and Submit the queue
		device->ResetFences(1, &inFlightFences[currentFrameInFlight]); // Unlike the semaphores, we manually need to restore the fence to the unsignaled state
		if (device->QueueSubmit(graphicsQueue.handle, 1/*count, for use of the next param*/, &submitInfo/*array, can have multiple!*/, inFlightFences[currentFrameInFlight]/*optional fence to be signaled*/) != VK_SUCCESS) {
			throw std::runtime_error("Failed to submit draw command buffer");
		}

		// Presentation
		VkPresentInfoKHR presentInfo = {};
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		// Specify which semaphore to wait on before presentation can happen
		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pWaitSemaphores = signalSemaphores;
		// Specify the swap chains to present images to and the index for each swap chain. (almost always a single one)
		VkSwapchainKHR swapChains[] = {swapChain->GetHandle()};
		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = swapChains;
		presentInfo.pImageIndices = &imageIndex;
		// The next param allows to specify an array of VkResult values to check for every individual swap chain if presentation was successful.
		// its not necessary if only using a single swap chain, because we can simply use the return value of the present function.
		presentInfo.pResults = nullptr;

		// Send the present info to the presentation queue !
		// This submits the request to present an image to the swap chain.
		result = device->QueuePresentKHR(presentationQueue.handle, &presentInfo);

		// Check for errors
		if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || window->WasResized()) {
			// SwapChain is out of date or not optimal, for instance if the window was resized, ReCreate the swapchain.
			window->ResetResize();
			RecreateSwapChains();
		} else if (result != VK_SUCCESS) {
			throw std::runtime_error("Failed to present swap chain image");
		}

		// Increment currentFrameInFlight
		currentFrameInFlight = (currentFrameInFlight + 1) % MAX_FRAMES_IN_FLIGHT;
	}

};
