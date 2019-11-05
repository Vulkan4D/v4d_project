#pragma once
#include <common/pch.hh>
#include <v4d.h>

#include "Vulkan.hpp"

/////////////////////////////////////////////

class MyVulkanRenderer : public Vulkan {
protected: // class members
	Window* window;

	// Main Render Surface
	VkSurfaceKHR surface;

	// Main Graphics Card
	VulkanGPU* renderingGPU = nullptr; // automatically deleted in base class
	VulkanDevice* renderingDevice;

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
	
	// States
	std::recursive_mutex renderingMutex;
	bool windowResized = false;
	uint windowWidth, windowHeight;
	bool renderTypeDirty = false;
	
	VkClearColorValue clearColor {0,0,0,1};
	
	// Ray Tracing
	bool useRayTracing = true;
	VkPhysicalDeviceRayTracingPropertiesNV rayTracingProperties{};
	VkAccelerationStructureNV rayTracingBottomLevelAccelerationStructure;
	VkDeviceMemory rayTracingBottomLevelAccelerationStructureMemory;
	uint64_t rayTracingBottomLevelAccelerationStructureHandle;
	VkAccelerationStructureNV rayTracingTopLevelAccelerationStructure;
	VkDeviceMemory rayTracingTopLevelAccelerationStructureMemory;
	uint64_t rayTracingTopLevelAccelerationStructureHandle;
	VkPipelineLayout rayTracingPipelineLayout;
	VkPipeline rayTracingPipeline;
	VkBuffer rayTracingShaderBindingTableBuffer;
	VkDeviceMemory rayTracingShaderBindingTableBufferMemory;

protected: // Abstract methods
	virtual void Init() = 0;
	virtual void FrameUpdate(uint imageIndex) = 0;
	// Scene
	virtual void LoadScene() = 0;
	virtual void UnloadScene() = 0;
	virtual void SendSceneToDevice() = 0;
	virtual void DeleteSceneFromDevice() = 0;
	// Rendering
	virtual void ConfigureGraphicsPipelines() = 0;
	virtual void ConfigureRayTracingPipelines() = 0;
	virtual void ConfigureCommandBuffer(VkCommandBuffer commandBuffer, int imageIndex) = 0;
	virtual void ConfigureRayTracingCommandBuffer(VkCommandBuffer commandBuffer, int imageIndex) = 0;

protected: // Virtual INIT Methods

	void CreateSurface() {
		surface = window->CreateVulkanSurface(handle);
		window->AddResizeCallback("vulkanSurface", [this](int width, int height){
			renderingMutex.lock();
			if (width != 0 && height != 0) {
				windowResized = true;
				windowWidth = (uint)width;
				windowHeight = (uint)height;
				renderingMutex.unlock();
			}
			// Rendering is paused while window is minimized
		});
	}
	
	void DestroySurface() {
		window->RemoveResizeCallback("vulkanSurface");
		DestroySurfaceKHR(surface, nullptr);
	}

	void CreateDevices() {
		// Select The Best Main GPU using a score system
		renderingGPU = SelectSuitableGPU([surface=surface](int &score, VulkanGPU* gpu){
			// Build up a score here and the GPU with the highest score will be selected.
			// Add to the score optional specs, then multiply with mandatory specs.

			// Optional specs  -->  score += points * CONDITION
			score += 100 * (gpu->GetProperties().deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU); // Is a Dedicated GPU
			score += 20 * gpu->GetFeatures().tessellationShader; // Supports Tessellation
			score += gpu->GetProperties().limits.framebufferColorSampleCounts; // Add sample counts to the score (1-64)

			// Mandatory specs  -->  score *= CONDITION
			score *= gpu->QueueFamiliesContainsFlags(VK_QUEUE_GRAPHICS_BIT, 1, surface); // Supports Graphics Queues
			score *= gpu->GetFeatures().geometryShader; // Supports Geometry Shaders
			score *= gpu->GetFeatures().samplerAnisotropy; // Supports Anisotropic filtering
			score *= gpu->GetFeatures().sampleRateShading; // Supports Sample Shading
			score *= gpu->SupportsExtension(VK_KHR_SWAPCHAIN_EXTENSION_NAME/*"VK_KHR_swapchain"*/); // Supports SwapChains
			score *= gpu->SupportsExtension(VK_NV_RAY_TRACING_EXTENSION_NAME/*"VK_NV_ray_tracing"*/); // Supports RayTracing
			score *= gpu->SupportsExtension(VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME); // Needed for RayTracing
		});

		LOG("Selected Rendering GPU: " << renderingGPU->GetDescription());

		// Prepare Device Features
		VkPhysicalDeviceFeatures deviceFeatures = {};
		deviceFeatures.geometryShader = VK_TRUE;
		deviceFeatures.samplerAnisotropy = VK_TRUE;
		deviceFeatures.sampleRateShading = VK_TRUE;
		// Create Logical Device
		renderingDevice = new VulkanDevice(
			renderingGPU,
			deviceFeatures,
			{ // Device-specific Extensions string[] (NOT VULKAN INSTANCE EXTs)
				VK_KHR_SWAPCHAIN_EXTENSION_NAME, // "VK_KHR_swapchain"
				VK_NV_RAY_TRACING_EXTENSION_NAME, // "VK_NV_ray_tracing"
				VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME, 
			},
			vulkanLoader->requiredInstanceLayers,
			{// Queues
				{
					"graphics",
					VK_QUEUE_GRAPHICS_BIT, // Flags
					1, // Count
					{1.0f}, // Priorities (one per queue count)
					surface // Putting a surface here forces the need for a presentation feature on that specific queue, null if no need, or get presentation queue separately with renderingDevice->GetPresentationQueue(surface)
				},
			}
		);

		// Get Graphics Queue
		graphicsQueue = renderingDevice->GetQueue("graphics");

		// Get Presentation Queue
		presentationQueue = graphicsQueue; // Use same as graphics (as configured above), otherwise uncomment block below to use a separate queue for presentation (apparently, using the same queue is supposed to be faster...)
		// presentationQueue = renderingDevice->GetPresentationQueue(surface);
		// if (presentationQueue.handle == nullptr) {
		// 	throw std::runtime_error("Failed to get Presentation Queue for surface");
		// }

		// MultiSampling
		msaaSamples = std::min(VK_SAMPLE_COUNT_8_BIT, renderingGPU->GetMaxUsableSampleCount());
	}

	void DestroyDevices() {
		delete renderingDevice;
	}

	void CreatePools() {
		renderingDevice->CreateCommandPool(graphicsQueue.index, 0, &graphicsCommandPool);
		renderingDevice->CreateCommandPool(graphicsQueue.index, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, &transferCommandPool);
		if (useRayTracing) {
			renderingDevice->CreateDescriptorPool(
				{
				VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_NV, 
				VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
				VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER
				}, 
				swapChain->imageViews.size(), 
				descriptorPool, 
				0
			);
		} else {
			renderingDevice->CreateDescriptorPool(
				{
					VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 
					VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
				}, 
				swapChain->imageViews.size(), 
				descriptorPool, 
				VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT
			);
		}
	}

	void DestroyPools() {
		renderingDevice->DestroyCommandPool(graphicsCommandPool);
		renderingDevice->DestroyCommandPool(transferCommandPool);
		renderingDevice->DestroyDescriptorPool(descriptorPool);
	}

	void CreateSyncObjects() {
		// each of the events in the RenderFrame method re set in motion using a single function call, but they are executed asynchronously. 
		// The function calls will return before the opeations are actually finished and the order of execution is also undefined.
		// Hence, we need to synchronize them.
		// There are two ways of synchronizing swap chai events : Fences and Semaphores.
		// The state of fences can be accessed from our rogram using calls like vkWaitForFences, but not for semaphores.
		// Semaphores are used to synchronize operationswithin or across command queues.
		// We have one semaphore to signal that an imagehas been aquired and is ready for rendering, and another one to signal that rendering has finished and presentation can happen.

		imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
		renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
		inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

		VkSemaphoreCreateInfo semaphoreInfo = {};
		semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

		VkFenceCreateInfo fenceInfo = {};
		fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT; // Initialize in the signaled state so that we dont get stuck on the first frame

		for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
			if (renderingDevice->CreateSemaphore(&semaphoreInfo, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS) {
				throw std::runtime_error("Failed to create semaphore for ImageAvailable");
			}
			if (renderingDevice->CreateSemaphore(&semaphoreInfo, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS) {
				throw std::runtime_error("Failed to create semaphore for RenderFinished");
			}
			if (renderingDevice->CreateFence(&fenceInfo, nullptr, &inFlightFences[i]) != VK_SUCCESS) {
				throw std::runtime_error("Failed to create InFlightFence");
			}
		}
	}

	void DestroySyncObjects() {
		for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
			renderingDevice->DestroySemaphore(renderFinishedSemaphores[i], nullptr);
			renderingDevice->DestroySemaphore(imageAvailableSemaphores[i], nullptr);
			renderingDevice->DestroyFence(inFlightFences[i], nullptr);
		}
	}

	void CreateSwapChain() {
		std::lock_guard lock(renderingMutex);
		windowResized = false;
		
		// Put old swapchain in a temporary pointer and delete it after creating new swapchain
		VulkanSwapChain* oldSwapChain = swapChain;

		// Create the new swapchain object
		swapChain = new VulkanSwapChain(
			renderingDevice,
			surface,
			{ // Preferred Extent (Screen Resolution)
				windowWidth,
				windowHeight
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

	void DestroySwapChain() {
		delete swapChain;
	}

	// Graphics Pipeline

	virtual void CreateRenderPass() {
		if (useRayTracing) return;
		
		renderPass = new VulkanRenderPass(renderingDevice);

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
		if (renderPass)
			delete renderPass;
		renderPass = nullptr;
	}

	virtual void CreateGraphicsPipelines() {
		if (useRayTracing) {
			ConfigureRayTracingPipelines();
		} else {
			ConfigureGraphicsPipelines();
			// Create the Graphics Pipeline !
			renderPass->CreateGraphicsPipelines();
		}
	}

	virtual void DestroyGraphicsPipelines() {
		if (renderPass) {
			renderPass->DestroyGraphicsPipelines();
		}
	}

	virtual void CreateFrameBuffers() {
		std::lock_guard lock(renderingMutex);
		
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
			framebufferCreateInfo.renderPass = useRayTracing? VK_NULL_HANDLE : renderPass->handle;
			framebufferCreateInfo.attachmentCount = attachments.size();
			framebufferCreateInfo.pAttachments = attachments.data(); // Specifies the VkImageView objects that should be bound to the respective attachment descriptions in the render pass pAttachment array.
			framebufferCreateInfo.width = swapChain->extent.width;
			framebufferCreateInfo.height = swapChain->extent.height;
			framebufferCreateInfo.layers = 1; // refers to the number of layers in image arrays
			if (renderingDevice->CreateFramebuffer(&framebufferCreateInfo, nullptr, &swapChainFrameBuffers[i]) != VK_SUCCESS) {
				throw std::runtime_error("Failed to create framebuffer");
			}
		}
	}

	virtual void DestroyFrameBuffers() {
		for (auto framebuffer : swapChainFrameBuffers) {
			renderingDevice->DestroyFramebuffer(framebuffer, nullptr);
		}
	}
	
	virtual void CreateCommandBuffers() {
		std::lock_guard lock(renderingMutex);
		
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
		if (renderingDevice->AllocateCommandBuffers(&allocInfo, commandBuffers.data()) != VK_SUCCESS) {
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
			if (renderingDevice->BeginCommandBuffer(commandBuffers[i], &beginInfo) != VK_SUCCESS) {
				throw std::runtime_error("Faild to begin recording command buffer");
			}

			if (useRayTracing) {
				
				// Commands to submit on each draw
				ConfigureRayTracingCommandBuffer(commandBuffers[i], i);
				
			} else {
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
				clearValues[0].color = clearColor;
				clearValues[1].depthStencil = {1.0f/*depth*/, 0/*stencil*/}; // The range of depth is 0 to 1 in Vulkan, where 1 is far and 0 is near. We want to clear to the Far value.
				renderPassInfo.clearValueCount = clearValues.size();
				renderPassInfo.pClearValues = clearValues.data();
				//
				renderingDevice->CmdBeginRenderPass(commandBuffers[i], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE); // Returns void, so no error handling for any vkCmdXxx functions until we've finished recording.
																	/*	the last parameter controls how the drawing commands within the render pass will be provided.
																		VK_SUBPASS_CONTENTS_INLINE = The render pass commands will be embedded in the primary command buffer itself and no secondary command buffers will be executed
																		VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS = The render pass commands will be executed from secondary command buffers
																	*/
				
				// Commands to submit on each draw
				ConfigureCommandBuffer(commandBuffers[i], i);
				
				renderingDevice->CmdEndRenderPass(commandBuffers[i]);
			}
			
			if (renderingDevice->EndCommandBuffer(commandBuffers[i]) != VK_SUCCESS) {
				throw std::runtime_error("Failed to record command buffer");
			}
		}
	}

	virtual void DestroyCommandBuffers() {
		renderingDevice->FreeCommandBuffers(graphicsCommandPool, static_cast<uint32_t>(commandBuffers.size()), commandBuffers.data());
	}

	virtual void CreateColorResources() {
		VkFormat colorFormat = swapChain->format.format;
		renderingDevice->CreateImage(
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
		if (renderingDevice->CreateImageView(&viewInfo, nullptr, &colorImageView) != VK_SUCCESS) {
			throw std::runtime_error("Failed to create texture image view");
		}
	}

	virtual void DestroyColorResources() {
		renderingDevice->DestroyImageView(colorImageView, nullptr);
		renderingDevice->DestroyImage(colorImage, nullptr);
		renderingDevice->FreeMemory(colorImageMemory, nullptr);
	}

	virtual void CreateDepthResources() {
		// Format
		depthImageFormat = renderingGPU->FindSupportedFormat({VK_FORMAT_D32_SFLOAT_S8_UINT}, VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);

		// Image
		renderingDevice->CreateImage(
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
		if (renderingDevice->CreateImageView(&viewInfo, nullptr, &depthImageView) != VK_SUCCESS) {
			throw std::runtime_error("Failed to create texture image view");
		}

		// Transition Layout
		TransitionImageLayout(depthImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, 1);
	}

	virtual void DestroyDepthResources() {
		renderingDevice->DestroyImageView(depthImageView, nullptr);
		renderingDevice->DestroyImage(depthImage, nullptr);
		renderingDevice->FreeMemory(depthImageMemory, nullptr);
	}

protected: // Helper methods

	VkCommandBuffer BeginSingleTimeCommands(VkCommandPool commandPool) {
		VkCommandBufferAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandPool = commandPool;
		allocInfo.commandBufferCount = 1;

		VkCommandBuffer commandBuffer;
		renderingDevice->AllocateCommandBuffers(&allocInfo, &commandBuffer);

		VkCommandBufferBeginInfo beginInfo = {};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

		renderingDevice->BeginCommandBuffer(commandBuffer, &beginInfo);

		return commandBuffer;
	}

	void EndSingleTimeCommands(VkCommandPool commandPool, VkCommandBuffer commandBuffer) {
		renderingDevice->EndCommandBuffer(commandBuffer);

		VkSubmitInfo submitInfo = {};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &commandBuffer;

		// renderingDevice->QueueSubmit(graphicsQueue.handle, 1, &submitInfo, VK_NULL_HANDLE);
		// renderingDevice->QueueWaitIdle(graphicsQueue.handle); // Using a fence instead would allow to schedule multiple transfers simultaneously and wait for all of them to complete, instead of executing one at a time.

		VkFenceCreateInfo fenceInfo {};
		fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fenceInfo.flags = 0;
		VkFence fence;
		if (renderingDevice->CreateFence(&fenceInfo, nullptr, &fence) != VK_SUCCESS)
			throw std::runtime_error("Failed to create fence");

		if (renderingDevice->QueueSubmit(graphicsQueue.handle, 1, &submitInfo, fence) != VK_SUCCESS)
			throw std::runtime_error("Failed to submit queue");

		if (renderingDevice->WaitForFences(1, &fence, VK_TRUE, 1000000000l*3600 /* nanoseconds */))
			throw std::runtime_error("Failed to wait for fence to signal");

		renderingDevice->DestroyFence(fence, nullptr);
		
		renderingDevice->FreeCommandBuffers(commandPool, 1, &commandBuffer);
	}

	void TransitionImageLayout(VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t mipLevels) {
		auto commandBuffer = BeginSingleTimeCommands(transferCommandPool);
		TransitionImageLayout(commandBuffer, image, oldLayout, newLayout, mipLevels);
		EndSingleTimeCommands(transferCommandPool, commandBuffer);
	}
	
	void TransitionImageLayout(VkCommandBuffer commandBuffer, VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t mipLevels) {
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
		renderingDevice->CmdPipelineBarrier(
			commandBuffer,
			srcStage, dstStage,
			0,
			0, nullptr,
			0, nullptr,
			1, &barrier
		);
	}

	void CopyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) {
		auto commandBuffer = BeginSingleTimeCommands(transferCommandPool);

		VkBufferCopy copyRegion = {};
		copyRegion.srcOffset = 0;
		copyRegion.dstOffset = 0;
		copyRegion.size = size;
		renderingDevice->CmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

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

		renderingDevice->CmdCopyBufferToImage(
			commandBuffer,
			buffer,
			image,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			1,
			&region
		);

		EndSingleTimeCommands(transferCommandPool, commandBuffer);
	}

	VkDeviceSize CopyShaderIdentifier(uint8_t* data, const uint8_t* shaderHandleStorage, uint32_t groupIndex) {
		const uint32_t shaderGroupHandleSize = rayTracingProperties.shaderGroupHandleSize;
		memcpy(data, shaderHandleStorage + groupIndex * shaderGroupHandleSize, shaderGroupHandleSize);
		data += shaderGroupHandleSize;
		return shaderGroupHandleSize;
	}

	// void GenerateMipmaps(Texture2D* texture) {
	// 	GenerateMipmaps(texture->GetImage(), texture->GetFormat(), texture->GetWidth(), texture->GetHeight(), texture->GetMipLevels());
	// }

	void GenerateMipmaps(VkImage image, VkFormat imageFormat, int32_t width, int32_t height, uint32_t mipLevels) {
		VkFormatProperties formatProperties;
		renderingGPU->GetPhysicalDeviceFormatProperties(imageFormat, &formatProperties);
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

			renderingDevice->CmdPipelineBarrier(
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

			renderingDevice->CmdBlitImage(
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

			renderingDevice->CmdPipelineBarrier(
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

		renderingDevice->CmdPipelineBarrier(
			commandBuffer,
			VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
			0, nullptr,
			0, nullptr,
			1, &barrier
		);

		EndSingleTimeCommands(transferCommandPool, commandBuffer);
	}

public: // Init/Reset Methods
	virtual void RecreateSwapChains() {
		std::lock_guard lock(renderingMutex);
		
		renderTypeDirty = false;
		renderingDevice->DeviceWaitIdle();

		// Destroy graphics pipeline
		DestroyCommandBuffers();
		DestroyFrameBuffers();
		DestroyGraphicsPipelines();
		DestroyRenderPass();
		DestroyDepthResources();
		DestroyColorResources();
		// Remove Scene data
		DeleteSceneFromDevice();

		// Re-Create the SwapChain
		DestroyPools();
		CreateSwapChain();
		CreatePools();

		// Add Scene data
		SendSceneToDevice();
		// Graphics pipeline
		CreateColorResources();
		CreateDepthResources();
		CreateRenderPass(); // shader attachments are assigned here
		CreateGraphicsPipelines(); // shaders are assigned here
		CreateFrameBuffers();
		CreateCommandBuffers(); // objects are rendered here
	}
	
	virtual void LoadRenderer() {
		std::lock_guard lock(renderingMutex);
		
		Init();
		
		CreatePools();
		
		// Load scene assets
		LoadScene();
		SendSceneToDevice();
		
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
	
	virtual void UnloadRenderer() {
		std::lock_guard lock(renderingMutex);
		
		// Wait for renderingDevice to be idle before destroying everything
		renderingDevice->DeviceWaitIdle(); // We can also wait for operations in a specific command queue to be finished with vkQueueWaitIdle. These functions can be used as a very rudimentary way to perform synchronization. 

		DestroyCommandBuffers();
		DestroyFrameBuffers();
		DestroyGraphicsPipelines();
		DestroyRenderPass();
		DestroyDepthResources();
		DestroyColorResources();
		
		DeleteSceneFromDevice();
		UnloadScene();
		
		DestroyPools();
	}
	
public: // Constructor & Destructor
	MyVulkanRenderer(VulkanLoader* loader, const char* applicationName, uint applicationVersion, Window* window) : Vulkan(loader, applicationName, applicationVersion), window(window) {
		CreateSurface();
		CreateDevices();
		CreateSyncObjects();
		CreateSwapChain();
		
		// Query the ray tracing properties of the current implementation
		rayTracingProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PROPERTIES_NV;
		VkPhysicalDeviceProperties2 deviceProps2{};
		deviceProps2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
		deviceProps2.pNext = &rayTracingProperties;
		GetPhysicalDeviceProperties2(renderingDevice->GetGPU()->GetHandle(), &deviceProps2);
	}

	virtual ~MyVulkanRenderer() override {
		DestroySwapChain();
		DestroySyncObjects();
		DestroyDevices();
		DestroySurface();
	}


public: // Public Methods

	virtual void RenderFrame() {
		std::lock_guard lock(renderingMutex);
		
		// Wait for previous frame to be finished
		renderingDevice->WaitForFences(1/*fencesCount*/, &inFlightFences[currentFrameInFlight]/*fences array*/, VK_TRUE/*wait for all fences in this array*/, std::numeric_limits<uint64_t>::max()/*timeout*/);

		// Get an image from the swapchain
		uint imageIndex;
		VkResult result = renderingDevice->AcquireNextImageKHR(
			swapChain->GetHandle(), // swapChain
			std::numeric_limits<uint64_t>::max(), // timeout in nanoseconds (using max disables the timeout)
			imageAvailableSemaphores[currentFrameInFlight], // semaphore
			VK_NULL_HANDLE, // fence
			&imageIndex // output the index of the swapchain image in there
		);

		// Check for errors
		if (result == VK_ERROR_OUT_OF_DATE_KHR || renderTypeDirty) {
			// SwapChain is out of date, for instance if the window was resized, stop here and ReCreate the swapchain.
			RecreateSwapChains();
			return;
		} else if (result == VK_SUBOPTIMAL_KHR) {
			// LOG_VERBOSE("Swapchain is suboptimal...")
		} else if (result != VK_SUCCESS) {
			throw std::runtime_error("Failed to acquire swap chain images");
		}

		// Update data every frame
		FrameUpdate(imageIndex);

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
		renderingDevice->ResetFences(1, &inFlightFences[currentFrameInFlight]); // Unlike the semaphores, we manually need to restore the fence to the unsignaled state
		if (renderingDevice->QueueSubmit(graphicsQueue.handle, 1/*count, for use of the next param*/, &submitInfo/*array, can have multiple!*/, inFlightFences[currentFrameInFlight]/*optional fence to be signaled*/) != VK_SUCCESS) {
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
		result = renderingDevice->QueuePresentKHR(presentationQueue.handle, &presentInfo);

		// Check for errors
		if (result == VK_ERROR_OUT_OF_DATE_KHR || renderTypeDirty) {
			// SwapChain is out of date, for instance if the window was resized, stop here and ReCreate the swapchain.
			RecreateSwapChains();
		} else if (result == VK_SUBOPTIMAL_KHR) {
			// LOG_VERBOSE("Swapchain is suboptimal...")
		} else if (result != VK_SUCCESS) {
			throw std::runtime_error("Failed to present swap chain images");
		}

		// Increment currentFrameInFlight
		currentFrameInFlight = (currentFrameInFlight + 1) % MAX_FRAMES_IN_FLIGHT;
	}
	
	void UseRayTracing(bool rayTracingEnabled = true) {
		std::lock_guard lock(renderingMutex);
		useRayTracing = rayTracingEnabled;
		renderTypeDirty = true;
	}

};
