#pragma once
#include <v4d.h>

#include "Instance.hpp"
#include "PipelineLayout.hpp"

/////////////////////////////////////////////

class VulkanRenderer : public Instance {
protected: // class members

	// Main Render Surface
	VkSurfaceKHR surface;

	// Main Graphics Card
	PhysicalDevice* renderingPhysicalDevice = nullptr;
	Device* renderingDevice = nullptr;
	
	// Queues
	VulkanQueue graphicsQueue;
	VulkanQueue presentationQueue;

	// Pools
	VkCommandPool commandPool;
	VkDescriptorPool descriptorPool;

	// Command buffers
	std::vector<VkCommandBuffer> commandBuffers;

	// Swap Chains
	SwapChain* swapChain = nullptr;

	// Sync objects
	std::vector<VkSemaphore> imageAvailableSemaphores;
	std::vector<VkSemaphore> renderFinishedSemaphores;
	std::vector<VkFence> inFlightFences;
	size_t currentFrameInFlight = 0;

	// Constants
	const int MAX_FRAMES_IN_FLIGHT = 2;
	
	// States
	std::recursive_mutex renderingMutex;
	std::recursive_mutex uboMutex;
	bool swapChainDirty = false;
	
	// Descriptor sets
	std::vector<DescriptorSet*> descriptorSets {};
	std::vector<VkDescriptorSet> vkDescriptorSets {};
	std::vector<PipelineLayout*> pipelineLayouts {};

	// Preferences
	std::vector<VkPresentModeKHR> preferredPresentModes {
		// VK_PRESENT_MODE_MAILBOX_KHR,	// TripleBuffering (No Tearing, low latency)
		// VK_PRESENT_MODE_FIFO_KHR,		// VSync ON (No Tearing, more latency)
		// VK_PRESENT_MODE_IMMEDIATE_KHR,	// VSync OFF (With Tearing, no latency)
	};

private: // Device Extensions and features
	std::vector<const char*> requiredDeviceExtensions { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
	std::vector<const char*> optionalDeviceExtensions {};
	std::vector<const char*> deviceExtensions {};
	std::unordered_map<std::string, bool> enabledDeviceExtensions {};
public:
	VkPhysicalDeviceFeatures deviceFeatures {}; // This object will be modified to keep only the enabled values.
	
	void RequiredDeviceExtension(const char* ext) {
		requiredDeviceExtensions.push_back(std::move(ext));
	}

	void OptionalDeviceExtension(const char* ext) {
		optionalDeviceExtensions.push_back(std::move(ext));
	}
	
	bool IsDeviceExtensionEnabled(const char* ext) {
		return enabledDeviceExtensions.find(ext) != enabledDeviceExtensions.end();
	}

protected: // Pure Virtual (abstract) methods
	// Init
	virtual void ScorePhysicalDeviceSelection(int& score, PhysicalDevice* physicalDevice) = 0;
	virtual void Init() = 0;
	virtual void Info() = 0;
	virtual void CreateResources() = 0;
	virtual void DestroyResources() = 0;
	// Update
	virtual void FrameUpdate(uint imageIndex) = 0;
	// Scene
	virtual void LoadScene() = 0;
	virtual void UnloadScene() = 0;
	virtual void CreateSceneGraphics() = 0;
	virtual void DestroySceneGraphics() = 0;
	// Rendering
	virtual void CreateGraphicsPipelines() = 0;
	virtual void DestroyGraphicsPipelines() = 0;
	virtual void RenderingCommandBuffer(VkCommandBuffer, int imageIndex) = 0;

protected: // Virtual INIT Methods

	virtual void CreateDevices() {
		// Select The Best Main PhysicalDevice using a score system
		renderingPhysicalDevice = SelectSuitablePhysicalDevice([this](int& score, PhysicalDevice* physicalDevice){
			// Build up a score here and the PhysicalDevice with the highest score will be selected.

			// Mandatory physicalDevice requirements for rendering graphics
			if (!physicalDevice->QueueFamiliesContainsFlags(VK_QUEUE_GRAPHICS_BIT, 1, surface))
				return;
			// User-defined required extensions
			for (auto& ext : requiredDeviceExtensions) if (!physicalDevice->SupportsExtension(ext))
				return;
			
			score = 1;
			
			// Each Optional extensions adds one point to the score
			for (auto& ext : optionalDeviceExtensions) if (physicalDevice->SupportsExtension(ext))
				++score;

			// User-defined score function
			ScorePhysicalDeviceSelection(score, physicalDevice);
		});

		LOG("Selected Rendering PhysicalDevice: " << renderingPhysicalDevice->GetDescription());

		// Prepare Device Features (remove unsupported features from list of features to enable)
		auto supportedDeviceFeatures = renderingPhysicalDevice->GetFeatures();
		const size_t deviceFeaturesArraySize = sizeof(VkPhysicalDeviceFeatures) / sizeof(VkBool32);
		VkBool32 supportedDeviceFeaturesData[deviceFeaturesArraySize];
		VkBool32 appDeviceFeaturesData[deviceFeaturesArraySize];
		memcpy(supportedDeviceFeaturesData, &supportedDeviceFeatures, sizeof(VkPhysicalDeviceFeatures));
		memcpy(appDeviceFeaturesData, &supportedDeviceFeatures, sizeof(VkPhysicalDeviceFeatures));
		for (size_t i = 0; i < deviceFeaturesArraySize; ++i) {
			if (appDeviceFeaturesData[i] && !supportedDeviceFeaturesData[i]) {
				appDeviceFeaturesData[i] = VK_FALSE;
			}
		}
		memcpy(&deviceFeatures, appDeviceFeaturesData, sizeof(VkPhysicalDeviceFeatures));
		
		// Prepare enabled extensions
		deviceExtensions.clear();
		for (auto& ext : requiredDeviceExtensions) {
			deviceExtensions.push_back(ext);
			enabledDeviceExtensions[ext] = true;
			LOG("Enabling Device Extension: " << ext)
		}
		for (auto& ext : optionalDeviceExtensions) {
			if (renderingPhysicalDevice->SupportsExtension(ext)) {
				deviceExtensions.push_back(ext);
				enabledDeviceExtensions[ext] = true;
				LOG("Enabling Device Extension: " << ext)
			} else {
				enabledDeviceExtensions[ext] = false;
			}
		}
		
		// Create Logical Device
		renderingDevice = new Device(
			renderingPhysicalDevice,
			deviceFeatures,
			deviceExtensions,
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
	}

	virtual void DestroyDevices() {
		delete renderingDevice;
	}

	virtual void CreateSyncObjects() {
		// each of the events in the Render method are set in motion using a single function call, but they are executed asynchronously. 
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

	virtual void DestroySyncObjects() {
		for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
			renderingDevice->DestroySemaphore(renderFinishedSemaphores[i], nullptr);
			renderingDevice->DestroySemaphore(imageAvailableSemaphores[i], nullptr);
			renderingDevice->DestroyFence(inFlightFences[i], nullptr);
		}
	}
	
	virtual void CreateCommandPools() {
		renderingDevice->CreateCommandPool(graphicsQueue.index, 0, &commandPool);
	}
	
	virtual void DestroyCommandPools() {
		renderingDevice->DestroyCommandPool(commandPool);
	}
	
	virtual void CreateDescriptorSets() {
		for (auto* set : descriptorSets) {
			set->CreateDescriptorSetLayout(renderingDevice);
		}
		
		// Descriptor sets / pool
		std::map<VkDescriptorType, uint> descriptorTypes {};
		for (auto* set : descriptorSets) {
			for (auto&[binding, descriptor] : set->GetBindings()) {
				if (descriptorTypes.find(descriptor.descriptorType) == descriptorTypes.end()) {
					descriptorTypes[descriptor.descriptorType] = 1;
				} else {
					descriptorTypes[descriptor.descriptorType]++;
				}
			}
		}
		renderingDevice->CreateDescriptorPool(
			descriptorTypes,
			descriptorPool,
			VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT
		);
		
		// Allocate descriptor sets
		std::vector<VkDescriptorSetLayout> setLayouts {};
		vkDescriptorSets.resize(descriptorSets.size());
		setLayouts.reserve(descriptorSets.size());
		for (auto* set : descriptorSets) {
			setLayouts.push_back(set->GetDescriptorSetLayout());
		}
		VkDescriptorSetAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.descriptorPool = descriptorPool;
		allocInfo.descriptorSetCount = (uint)setLayouts.size();
		allocInfo.pSetLayouts = setLayouts.data();
		if (renderingDevice->AllocateDescriptorSets(&allocInfo, vkDescriptorSets.data()) != VK_SUCCESS) {
			throw std::runtime_error("Failed to allocate descriptor sets");
		}
		for (int i = 0; i < descriptorSets.size(); ++i) {
			descriptorSets[i]->descriptorSet = vkDescriptorSets[i];
		}
		
		UpdateDescriptorSets();
		
		// Pipeline layouts
		for (auto* layout : pipelineLayouts) {
			layout->Create(renderingDevice);
		}
	}
	
	virtual void DestroyDescriptorSets() {
		// Pipeline layouts
		for (auto* layout : pipelineLayouts) {
			layout->Destroy(renderingDevice);
		}
		
		// Descriptor Sets
		renderingDevice->FreeDescriptorSets(descriptorPool, (uint)vkDescriptorSets.size(), vkDescriptorSets.data());
		for (auto* set : descriptorSets) set->DestroyDescriptorSetLayout(renderingDevice);
		// Descriptor pools
		renderingDevice->DestroyDescriptorPool(descriptorPool, nullptr);
	}

	virtual void UpdateDescriptorSets() {
		std::vector<VkWriteDescriptorSet> descriptorWrites {};
		for (auto* set : descriptorSets) {
			for (auto&[binding, descriptor] : set->GetBindings()) {
				descriptorWrites.push_back(descriptor.GetWriteDescriptorSet(set->descriptorSet));
			}
		}
		renderingDevice->UpdateDescriptorSets((uint)descriptorWrites.size(), descriptorWrites.data(), 0, nullptr);
	}

	virtual void CreateSwapChain() {
		std::lock_guard lock(renderingMutex);
		
		// Put old swapchain in a temporary pointer and delete it after creating new swapchain
		SwapChain* oldSwapChain = swapChain;

		// Create the new swapchain object
		swapChain = new SwapChain(
			renderingDevice,
			surface,
			{ // Preferred Extent (Screen Resolution)
				0, // width
				0 // height
			},
			{ // Preferred Formats
				{VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR},
			},
			preferredPresentModes
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
		swapChain = nullptr;
	}

	virtual void CreateCommandBuffers() {
		std::lock_guard lock(renderingMutex);
		
		// Because one of the drawing commands involves binding the right VkFramebuffer, we'll actually have to record a command buffer for every image in the swap chain once again.
		// Command buffers will be automatically freed when their command pool is destroyed, so we don't need an explicit cleanup
		commandBuffers.resize(swapChain->imageViews.size());
		VkCommandBufferAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.commandPool = commandPool;
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

			//////////////////////////////////////////////////////////
			
			// Commands to submit on each draw
			RenderingCommandBuffer(commandBuffers[i], i);
			
			//////////////////////////////////////////////////////////
			
			if (renderingDevice->EndCommandBuffer(commandBuffers[i]) != VK_SUCCESS) {
				throw std::runtime_error("Failed to record command buffer");
			}
		}
	}

	virtual void DestroyCommandBuffers() {
		renderingDevice->FreeCommandBuffers(commandPool, static_cast<uint32_t>(commandBuffers.size()), commandBuffers.data());
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

		VkFenceCreateInfo fenceInfo {};
		fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fenceInfo.flags = 0;
		VkFence fence;
		if (renderingDevice->CreateFence(&fenceInfo, nullptr, &fence) != VK_SUCCESS)
			throw std::runtime_error("Failed to create fence");

		if (renderingDevice->QueueSubmit(graphicsQueue.handle, 1, &submitInfo, fence) != VK_SUCCESS)
			throw std::runtime_error("Failed to submit queue");

		if (renderingDevice->WaitForFences(1, &fence, VK_TRUE, std::numeric_limits<uint64_t>::max() /* nanoseconds */))
			throw std::runtime_error("Failed to wait for fence to signal");

		renderingDevice->DestroyFence(fence, nullptr);
		
		renderingDevice->FreeCommandBuffers(commandPool, 1, &commandBuffer);
	}


	void AllocateBufferStaged(VkCommandPool commandPool, Buffer& buffer) {
		auto cmdBuffer = BeginSingleTimeCommands(commandPool);
		Buffer stagingBuffer(VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
		stagingBuffer.srcDataPointers = std::ref(buffer.srcDataPointers);
		stagingBuffer.Allocate(renderingDevice, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, true);
		buffer.usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
		buffer.Allocate(renderingDevice, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, false);
		Buffer::Copy(renderingDevice, cmdBuffer, stagingBuffer.buffer, buffer.buffer, buffer.size);
		EndSingleTimeCommands(commandPool, cmdBuffer);
		stagingBuffer.Free(renderingDevice);
	}
	void AllocateBuffersStaged(VkCommandPool commandPool, std::vector<Buffer>& buffers) {
		auto cmdBuffer = BeginSingleTimeCommands(commandPool);
		std::vector<Buffer> stagingBuffers {};
		stagingBuffers.reserve(buffers.size());
		for (auto& buffer : buffers) {
			auto& stagingBuffer = stagingBuffers.emplace_back(VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
			stagingBuffer.srcDataPointers = std::ref(buffer.srcDataPointers);
			stagingBuffer.Allocate(renderingDevice, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, true);
			buffer.usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
			buffer.Allocate(renderingDevice, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, false);
			Buffer::Copy(renderingDevice, cmdBuffer, stagingBuffer.buffer, buffer.buffer, buffer.size);
		}
		EndSingleTimeCommands(commandPool, cmdBuffer);
		for (auto& stagingBuffer : stagingBuffers) {
			stagingBuffer.Free(renderingDevice);
		}
	}
	void AllocateBuffersStaged(VkCommandPool commandPool, std::vector<Buffer*>& buffers) {
		auto cmdBuffer = BeginSingleTimeCommands(commandPool);
		std::vector<Buffer> stagingBuffers {};
		stagingBuffers.reserve(buffers.size());
		for (auto& buffer : buffers) {
			auto& stagingBuffer = stagingBuffers.emplace_back(VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
			stagingBuffer.srcDataPointers = std::ref(buffer->srcDataPointers);
			stagingBuffer.Allocate(renderingDevice, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, true);
			buffer->usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
			buffer->Allocate(renderingDevice, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, false);
			Buffer::Copy(renderingDevice, cmdBuffer, stagingBuffer.buffer, buffer->buffer, buffer->size);
		}
		EndSingleTimeCommands(commandPool, cmdBuffer);
		for (auto& stagingBuffer : stagingBuffers) {
			stagingBuffer.Free(renderingDevice);
		}
	}


	void TransitionImageLayout(VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t mipLevels) {
		auto commandBuffer = BeginSingleTimeCommands(commandPool);
		TransitionImageLayout(commandBuffer, image, oldLayout, newLayout, mipLevels);
		EndSingleTimeCommands(commandPool, commandBuffer);
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
		barrier.srcAccessMask = 0;
		barrier.dstAccessMask = 0;
		//
		if (newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
			barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
		} else {
			barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		}
		//
		VkPipelineStageFlags srcStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, dstStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
		
		// Source layouts (old)
		// Source access mask controls actions that have to be finished on the old layout
		// before it will be transitioned to the new layout
		switch (oldLayout) {
			case VK_IMAGE_LAYOUT_UNDEFINED:
				// Image layout is undefined (or does not matter)
				// Only valid as initial layout
				// No flags required, listed only for completeness
				barrier.srcAccessMask = 0;
				break;

			case VK_IMAGE_LAYOUT_PREINITIALIZED:
				// Image is preinitialized
				// Only valid as initial layout for linear images, preserves memory contents
				// Make sure host writes have been finished
				barrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
				break;

			case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
				// Image is a color attachment
				// Make sure any writes to the color buffer have been finished
				barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
				break;

			case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
				// Image is a depth/stencil attachment
				// Make sure any writes to the depth/stencil buffer have been finished
				barrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
				break;

			case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
				// Image is a transfer source 
				// Make sure any reads from the image have been finished
				barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
				break;

			case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
				// Image is a transfer destination
				// Make sure any writes to the image have been finished
				barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
				break;

			case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
				// Image is read by a shader
				// Make sure any shader reads from the image have been finished
				barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
				break;
			default:
				// Other source layouts aren't handled (yet)
				break;
		}

		// Target layouts (new)
		// Destination access mask controls the dependency for the new image layout
		switch (newLayout) {
			case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
				// Image will be used as a transfer destination
				// Make sure any writes to the image have been finished
				barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
				break;

			case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
				// Image will be used as a transfer source
				// Make sure any reads from the image have been finished
				barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
				break;

			case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
				// Image will be used as a color attachment
				// Make sure any writes to the color buffer have been finished
				barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
				break;

			case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
				// Image layout will be used as a depth/stencil attachment
				// Make sure any writes to depth/stencil buffer have been finished
				barrier.dstAccessMask = barrier.dstAccessMask | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
				break;

			case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
				// Image will be read in a shader (sampler, input attachment)
				// Make sure any writes to the image have been finished
				if (barrier.srcAccessMask == 0)
				{
					barrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
				}
				barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
				break;
			default:
				// Other source layouts aren't handled (yet)
				break;
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

	void CopyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height) {
		auto commandBuffer = BeginSingleTimeCommands(commandPool);
		CopyBufferToImage(commandBuffer, buffer, image, width, height);
		EndSingleTimeCommands(commandPool, commandBuffer);
	}

	void CopyBufferToImage(VkCommandBuffer commandBuffer, VkBuffer buffer, VkImage image, uint32_t width, uint32_t height) {
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
	}

	// void GenerateMipmaps(Texture2D* texture) {
	// 	GenerateMipmaps(texture->GetImage(), texture->GetFormat(), texture->GetWidth(), texture->GetHeight(), texture->GetMipLevels());
	// }

	void GenerateMipmaps(VkImage image, VkFormat imageFormat, int32_t width, int32_t height, uint32_t mipLevels) {
		VkFormatProperties formatProperties;
		renderingPhysicalDevice->GetPhysicalDeviceFormatProperties(imageFormat, &formatProperties);
		if (!(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT)) {
			throw std::runtime_error("Texture image format does not support linear blitting");
		}

		auto commandBuffer = BeginSingleTimeCommands(commandPool);

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

		EndSingleTimeCommands(commandPool, commandBuffer);
	}

protected: // Init/Reset Methods
	virtual void RecreateSwapChains() {
		std::lock_guard lock(renderingMutex);
		
		swapChainDirty = false;

		DeleteGraphicsFromDevice();
		
		// Re-Create the SwapChain
		CreateSwapChain();
		
		SendGraphicsToDevice();
	}
	
public: // Init/Load/Reset Methods
	virtual void LoadRenderer() {
		std::lock_guard lock(renderingMutex);
		
		Init();
		
		CreateDevices();
		CreateSyncObjects();
		CreateSwapChain();
		
		Info();
		
		LOG_SUCCESS("Vulkan Renderer is Ready !");
	}
	
	virtual void UnloadRenderer() {
		std::lock_guard lock(renderingMutex);
		DestroySwapChain();
		DestroySyncObjects();
		DestroyDevices();
	}
	
	virtual void ReloadRenderer() {
		std::lock_guard lock(renderingMutex);
		LOG_WARN("Reloading renderer...")
		
		DeleteGraphicsFromDevice();
		
		DestroySwapChain();
		DestroySyncObjects();
		DestroyDevices();
		
		CreateDevices();
		CreateSyncObjects();
		CreateSwapChain();
		
		Info();
		
		SendGraphicsToDevice();
		
		LOG_SUCCESS("Vulkan Renderer is Ready !")
	}
	
	virtual void SendGraphicsToDevice() {
		CreateCommandPools();
		CreateResources();
		CreateSceneGraphics();
		CreateDescriptorSets();
		CreateGraphicsPipelines(); // shaders are assigned here
		CreateCommandBuffers(); // objects are rendered here
	}
	
	virtual void DeleteGraphicsFromDevice() {
		// Wait for renderingDevice to be idle before destroying everything
		renderingDevice->DeviceWaitIdle(); // We can also wait for operations in a specific command queue to be finished with vkQueueWaitIdle. These functions can be used as a very rudimentary way to perform synchronization. 

		DestroyCommandBuffers();
		DestroyGraphicsPipelines();
		DestroyDescriptorSets();
		DestroySceneGraphics();
		DestroyResources();
		DestroyCommandPools();
	}
	
public: // Constructor & Destructor
	VulkanRenderer(Loader* loader, const char* applicationName, uint applicationVersion, Window* window)
	 : Instance(loader, applicationName, applicationVersion) {
		surface = window->CreateVulkanSurface(handle);
	}

	virtual ~VulkanRenderer() override {
		DestroySurfaceKHR(surface, nullptr);
	}

public: // Public Methods

	virtual void Render() {
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
		if (result == VK_ERROR_OUT_OF_DATE_KHR || swapChainDirty) {
			// SwapChain is out of date, for instance if the window was resized, stop here and ReCreate the swapchain.
			RecreateSwapChains();
			// Render();
			return;
		} else if (result == VK_SUBOPTIMAL_KHR) {
			// LOG_VERBOSE("Swapchain is suboptimal...")
		} else if (result != VK_SUCCESS) {
			throw std::runtime_error("Failed to acquire swap chain images");
		}

		// Update data every frame
		LockUBO();
		FrameUpdate(imageIndex);
		UnlockUBO();

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
		if ((result = renderingDevice->QueueSubmit(graphicsQueue.handle, 1/*count, for use of the next param*/, &submitInfo/*array, can have multiple!*/, inFlightFences[currentFrameInFlight]/*optional fence to be signaled*/)) != VK_SUCCESS) {
			LOG_ERROR((int)result)
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
		if (result == VK_ERROR_OUT_OF_DATE_KHR || swapChainDirty) {
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
	
	inline void LockUBO() {
		uboMutex.lock();
	}

	inline void UnlockUBO() {
		uboMutex.unlock();
	}

};
