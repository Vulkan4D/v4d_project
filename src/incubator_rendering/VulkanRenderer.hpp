#pragma once
#include <v4d.h>

using namespace v4d::graphics::vulkan;

class VulkanRenderer : public Instance {
protected: // class members

	// Main Render Surface
	VkSurfaceKHR surface;

	// Main Graphics Card
	PhysicalDevice* renderingPhysicalDevice = nullptr;
	Device* renderingDevice = nullptr;
	
	// Queues
	Queue graphicsQueue, lowPriorityGraphicsQueue, 
		computeQueue, lowPriorityComputeQueue, 
		presentQueue, transferQueue;

	// Command buffers
	std::vector<VkCommandBuffer> graphicsCommandBuffers, computeCommandBuffers, 
								graphicsDynamicCommandBuffers, computeDynamicCommandBuffers;
	VkCommandBuffer lowPriorityGraphicsCommandBuffer, lowPriorityComputeCommandBuffer;

	// Swap Chains
	SwapChain* swapChain = nullptr;

	// Sync objects
	std::vector<VkSemaphore> imageAvailableSemaphores;
	std::vector<VkSemaphore> renderFinishedSemaphores, dynamicRenderFinishedSemaphores;
	std::vector<VkSemaphore> computeFinishedSemaphores, dynamicComputeFinishedSemaphores;
	std::vector<VkFence> graphicsFences, computeFences;
	size_t currentFrameInFlight = 0;
	const int MAX_FRAMES_IN_FLIGHT = 2;
	
	// States
	std::recursive_mutex renderingMutex, lowPriorityRenderingMutex;
	
	// Descriptor sets
	VkDescriptorPool descriptorPool;
	std::vector<DescriptorSet*> descriptorSets {};
	std::vector<VkDescriptorSet> vkDescriptorSets {};
	std::vector<PipelineLayout*> pipelineLayouts {};

	// Preferences
	std::vector<VkPresentModeKHR> preferredPresentModes {
		// VK_PRESENT_MODE_MAILBOX_KHR,	// TripleBuffering (No Tearing, low latency)
		// VK_PRESENT_MODE_FIFO_KHR,	// VSync ON (No Tearing, more latency)
		VK_PRESENT_MODE_IMMEDIATE_KHR,	// VSync OFF (With Tearing, no latency)
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

protected: // Virtual methods
	// Init
	virtual void ScorePhysicalDeviceSelection(int& score, PhysicalDevice* physicalDevice) {}
	virtual void Init() {}
	virtual void Info() {}
	
	// Resources
	virtual void CreateResources() {}
	virtual void DestroyResources() {}
	virtual void AllocateBuffers() {}
	virtual void FreeBuffers() {}
	
	// Scene
	virtual void LoadScene() {}
	virtual void UnloadScene() {}
	
	// Pipelines
	virtual void CreatePipelines() {}
	virtual void DestroyPipelines() {}
	
	// Update
	virtual void FrameUpdate(uint imageIndex) {}
	virtual void LowPriorityFrameUpdate() {}
	
	// Commands
	virtual void RecordComputeCommandBuffer(VkCommandBuffer, int imageIndex) {}
	virtual void RecordGraphicsCommandBuffer(VkCommandBuffer, int imageIndex) {}
	virtual void RecordLowPriorityComputeCommandBuffer(VkCommandBuffer) {}
	virtual void RecordLowPriorityGraphicsCommandBuffer(VkCommandBuffer) {}
	virtual void RunDynamicCompute(VkCommandBuffer) {}
	virtual void RunDynamicGraphics(VkCommandBuffer) {}
	virtual void RunDynamicLowPriorityCompute(VkCommandBuffer) {}
	virtual void RunDynamicLowPriorityGraphics(VkCommandBuffer) {}

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
					VK_QUEUE_GRAPHICS_BIT,
					2, // Count
					{1.0f, 0.1f}, // Priorities (one per queue count)
					surface // Putting a surface here forces the need for a presentation feature on that specific queue family
				},
				{
					"compute",
					VK_QUEUE_COMPUTE_BIT,
					2, // Count
					{1.0f, 0.1f}, // Priorities (one per queue count)
				},
				{
					"transfer",
					VK_QUEUE_TRANSFER_BIT,
				},
				// {
				// 	"present",
				// 	VK_QUEUE_GRAPHICS_BIT,
				// 	1, // Count
				// 	{1.0f}, // Priorities
				// 	surface
				// },
			}
		);

		// Get Queues
		graphicsQueue = renderingDevice->GetQueue("graphics", 0);
		lowPriorityGraphicsQueue = renderingDevice->GetQueue("graphics", 1);
		computeQueue = renderingDevice->GetQueue("compute", 0);
		lowPriorityComputeQueue = renderingDevice->GetQueue("compute", 1);
		transferQueue = renderingDevice->GetQueue("transfer");
		
		presentQueue = graphicsQueue; // Performance is better when using the same queue index as the main graphics queue that renders to swap chains
		// presentQueue = renderingDevice->GetQueue("present");
		
		if (presentQueue.handle == nullptr) {
			throw std::runtime_error("Failed to get Presentation Queue for surface");
		}
	}

	virtual void DestroyDevices() {
		delete renderingDevice;
	}

	virtual void CreateSyncObjects() {
		imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
		renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
		computeFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
		dynamicRenderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
		dynamicComputeFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
		graphicsFences.resize(MAX_FRAMES_IN_FLIGHT);
		computeFences.resize(MAX_FRAMES_IN_FLIGHT);

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
			if (renderingDevice->CreateSemaphore(&semaphoreInfo, nullptr, &computeFinishedSemaphores[i]) != VK_SUCCESS) {
				throw std::runtime_error("Failed to create semaphore for RenderFinished");
			}
			if (renderingDevice->CreateSemaphore(&semaphoreInfo, nullptr, &dynamicRenderFinishedSemaphores[i]) != VK_SUCCESS) {
				throw std::runtime_error("Failed to create semaphore for RenderFinished");
			}
			if (renderingDevice->CreateSemaphore(&semaphoreInfo, nullptr, &dynamicComputeFinishedSemaphores[i]) != VK_SUCCESS) {
				throw std::runtime_error("Failed to create semaphore for RenderFinished");
			}
			if (renderingDevice->CreateFence(&fenceInfo, nullptr, &graphicsFences[i]) != VK_SUCCESS) {
				throw std::runtime_error("Failed to create graphicsFences");
			}
			if (renderingDevice->CreateFence(&fenceInfo, nullptr, &computeFences[i]) != VK_SUCCESS) {
				throw std::runtime_error("Failed to create computeFences");
			}
		}
	}

	virtual void DestroySyncObjects() {
		for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
			renderingDevice->DestroySemaphore(imageAvailableSemaphores[i], nullptr);
			renderingDevice->DestroySemaphore(renderFinishedSemaphores[i], nullptr);
			renderingDevice->DestroySemaphore(computeFinishedSemaphores[i], nullptr);
			renderingDevice->DestroySemaphore(dynamicRenderFinishedSemaphores[i], nullptr);
			renderingDevice->DestroySemaphore(dynamicComputeFinishedSemaphores[i], nullptr);
			renderingDevice->DestroyFence(graphicsFences[i], nullptr);
			renderingDevice->DestroyFence(computeFences[i], nullptr);
		}
	}
	
	virtual void CreateCommandPools() {
		renderingDevice->CreateCommandPool(graphicsQueue.familyIndex, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, &graphicsQueue.commandPool);
		renderingDevice->CreateCommandPool(computeQueue.familyIndex, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, &computeQueue.commandPool);
		renderingDevice->CreateCommandPool(lowPriorityGraphicsQueue.familyIndex, 0, &lowPriorityGraphicsQueue.commandPool);
		renderingDevice->CreateCommandPool(lowPriorityComputeQueue.familyIndex, 0, &lowPriorityComputeQueue.commandPool);
		renderingDevice->CreateCommandPool(transferQueue.familyIndex, 0, &transferQueue.commandPool);
	}
	
	virtual void DestroyCommandPools() {
		renderingDevice->DestroyCommandPool(graphicsQueue.commandPool);
		renderingDevice->DestroyCommandPool(computeQueue.commandPool);
		renderingDevice->DestroyCommandPool(lowPriorityGraphicsQueue.commandPool);
		renderingDevice->DestroyCommandPool(lowPriorityComputeQueue.commandPool);
		renderingDevice->DestroyCommandPool(transferQueue.commandPool);
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
		
		if (descriptorSets.size() > 0) {
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
		if (descriptorSets.size() > 0) {
			renderingDevice->FreeDescriptorSets(descriptorPool, (uint)vkDescriptorSets.size(), vkDescriptorSets.data());
			for (auto* set : descriptorSets) set->DestroyDescriptorSetLayout(renderingDevice);
			// Descriptor pools
			renderingDevice->DestroyDescriptorPool(descriptorPool, nullptr);
		}
	}

	virtual void UpdateDescriptorSets() {
		if (descriptorSets.size() > 0) {
			std::vector<VkWriteDescriptorSet> descriptorWrites {};
			for (auto* set : descriptorSets) {
				for (auto&[binding, descriptor] : set->GetBindings()) {
					descriptorWrites.push_back(descriptor.GetWriteDescriptorSet(set->descriptorSet));
				}
			}
			renderingDevice->UpdateDescriptorSets((uint)descriptorWrites.size(), descriptorWrites.data(), 0, nullptr);
		}
	}

	virtual void CreateSwapChain() {
		std::scoped_lock lock(renderingMutex, lowPriorityRenderingMutex);
		
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
				{VK_FORMAT_R32G32B32A32_SFLOAT, VK_COLOR_SPACE_HDR10_HLG_EXT},
				{VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR},
			},
			preferredPresentModes
		);
		// Assign queues
		swapChain->AssignQueues({presentQueue.familyIndex, graphicsQueue.familyIndex});
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
		std::scoped_lock lock(renderingMutex, lowPriorityRenderingMutex);
		
		VkCommandBufferAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
						/*	VK_COMMAND_BUFFER_LEVEL_PRIMARY = Can be submitted to a queue for execution, but cannot be called from other command buffers
							VK_COMMAND_BUFFER_LEVEL_SECONDARY = Cannot be submitted directly, but can be called from primary command buffers
						*/
		
		{// Compute Commands Recorder
			// Because one of the drawing commands involves binding the right VkFramebuffer, we'll actually have to record a command buffer for every image in the swap chain once again.
			// Command buffers will be automatically freed when their command pool is destroyed, so we don't need an explicit cleanup
			computeCommandBuffers.resize(swapChain->imageViews.size());
			
			allocInfo.commandPool = computeQueue.commandPool;
			allocInfo.commandBufferCount = (uint) computeCommandBuffers.size();
			if (renderingDevice->AllocateCommandBuffers(&allocInfo, computeCommandBuffers.data()) != VK_SUCCESS) {
				throw std::runtime_error("Failed to allocate command buffers");
			}

			// Starting command buffer recording...
			for (size_t i = 0; i < computeCommandBuffers.size(); i++) {
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
				if (renderingDevice->BeginCommandBuffer(computeCommandBuffers[i], &beginInfo) != VK_SUCCESS) {
					throw std::runtime_error("Faild to begin recording command buffer");
				}
				
				// Record commands
				RecordComputeCommandBuffer(computeCommandBuffers[i], i);
				
				if (renderingDevice->EndCommandBuffer(computeCommandBuffers[i]) != VK_SUCCESS) {
					throw std::runtime_error("Failed to record command buffer");
				}
			}
		}
		computeDynamicCommandBuffers.resize(computeCommandBuffers.size());
		if (renderingDevice->AllocateCommandBuffers(&allocInfo, computeDynamicCommandBuffers.data()) != VK_SUCCESS) {
			throw std::runtime_error("Failed to allocate command buffers");
		}
		
		{// Graphics Commands Recorder
			// Because one of the drawing commands involves binding the right VkFramebuffer, we'll actually have to record a command buffer for every image in the swap chain once again.
			// Command buffers will be automatically freed when their command pool is destroyed, so we don't need an explicit cleanup
			graphicsCommandBuffers.resize(swapChain->imageViews.size());
			
			allocInfo.commandPool = graphicsQueue.commandPool;
			allocInfo.commandBufferCount = (uint) graphicsCommandBuffers.size();
			if (renderingDevice->AllocateCommandBuffers(&allocInfo, graphicsCommandBuffers.data()) != VK_SUCCESS) {
				throw std::runtime_error("Failed to allocate command buffers");
			}

			// Starting command buffer recording...
			for (size_t i = 0; i < graphicsCommandBuffers.size(); i++) {
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
				if (renderingDevice->BeginCommandBuffer(graphicsCommandBuffers[i], &beginInfo) != VK_SUCCESS) {
					throw std::runtime_error("Faild to begin recording command buffer");
				}
				
				// Record commands
				RecordGraphicsCommandBuffer(graphicsCommandBuffers[i], i);
				
				if (renderingDevice->EndCommandBuffer(graphicsCommandBuffers[i]) != VK_SUCCESS) {
					throw std::runtime_error("Failed to record command buffer");
				}
			}
		}
		graphicsDynamicCommandBuffers.resize(graphicsCommandBuffers.size());
		if (renderingDevice->AllocateCommandBuffers(&allocInfo, graphicsDynamicCommandBuffers.data()) != VK_SUCCESS) {
			throw std::runtime_error("Failed to allocate command buffers");
		}
		
		{// Low Priority Compute recorder
			allocInfo.commandPool = lowPriorityComputeQueue.commandPool;
			allocInfo.commandBufferCount = 1;
			if (renderingDevice->AllocateCommandBuffers(&allocInfo, &lowPriorityComputeCommandBuffer) != VK_SUCCESS) {
				throw std::runtime_error("Failed to allocate command buffers");
			}
			// Starting command buffer recording...
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
			if (renderingDevice->BeginCommandBuffer(lowPriorityComputeCommandBuffer, &beginInfo) != VK_SUCCESS) {
				throw std::runtime_error("Faild to begin recording command buffer");
			}

			// Record commands
			RecordLowPriorityComputeCommandBuffer(lowPriorityComputeCommandBuffer);
			
			if (renderingDevice->EndCommandBuffer(lowPriorityComputeCommandBuffer) != VK_SUCCESS) {
				throw std::runtime_error("Failed to record command buffer");
			}
		}
		
		{// Low Priority Graphics recorder
			allocInfo.commandPool = lowPriorityGraphicsQueue.commandPool;
			allocInfo.commandBufferCount = 1;
			if (renderingDevice->AllocateCommandBuffers(&allocInfo, &lowPriorityGraphicsCommandBuffer) != VK_SUCCESS) {
				throw std::runtime_error("Failed to allocate command buffers");
			}
			// Starting command buffer recording...
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
			if (renderingDevice->BeginCommandBuffer(lowPriorityGraphicsCommandBuffer, &beginInfo) != VK_SUCCESS) {
				throw std::runtime_error("Faild to begin recording command buffer");
			}

			// Record commands
			RecordLowPriorityGraphicsCommandBuffer(lowPriorityGraphicsCommandBuffer);
			
			if (renderingDevice->EndCommandBuffer(lowPriorityGraphicsCommandBuffer) != VK_SUCCESS) {
				throw std::runtime_error("Failed to record command buffer");
			}
		}
		
	}

	virtual void DestroyCommandBuffers() {
		renderingDevice->FreeCommandBuffers(computeQueue.commandPool, static_cast<uint32_t>(computeCommandBuffers.size()), computeCommandBuffers.data());
		renderingDevice->FreeCommandBuffers(computeQueue.commandPool, static_cast<uint32_t>(computeDynamicCommandBuffers.size()), computeDynamicCommandBuffers.data());
		renderingDevice->FreeCommandBuffers(graphicsQueue.commandPool, static_cast<uint32_t>(graphicsCommandBuffers.size()), graphicsCommandBuffers.data());
		renderingDevice->FreeCommandBuffers(graphicsQueue.commandPool, static_cast<uint32_t>(graphicsDynamicCommandBuffers.size()), graphicsDynamicCommandBuffers.data());
		renderingDevice->FreeCommandBuffers(lowPriorityComputeQueue.commandPool, 1, &lowPriorityComputeCommandBuffer);
		renderingDevice->FreeCommandBuffers(lowPriorityGraphicsQueue.commandPool, 1, &lowPriorityGraphicsCommandBuffer);
	}

protected: // Helper methods

	VkCommandBuffer BeginSingleTimeCommands(Queue queue) {
		VkCommandBufferAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandPool = queue.commandPool;
		allocInfo.commandBufferCount = 1;

		VkCommandBuffer commandBuffer;
		renderingDevice->AllocateCommandBuffers(&allocInfo, &commandBuffer);

		VkCommandBufferBeginInfo beginInfo = {};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

		renderingDevice->BeginCommandBuffer(commandBuffer, &beginInfo);

		return commandBuffer;
	}

	void EndSingleTimeCommands(Queue queue, VkCommandBuffer commandBuffer) {
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

		if (renderingDevice->QueueSubmit(queue.handle, 1, &submitInfo, fence) != VK_SUCCESS)
			throw std::runtime_error("Failed to submit queue");

		if (renderingDevice->WaitForFences(1, &fence, VK_TRUE, std::numeric_limits<uint64_t>::max() /* nanoseconds */))
			throw std::runtime_error("Failed to wait for fence to signal");

		renderingDevice->DestroyFence(fence, nullptr);
		
		renderingDevice->FreeCommandBuffers(queue.commandPool, 1, &commandBuffer);
	}

	void AllocateBufferStaged(Queue queue, Buffer& buffer) {
		auto cmdBuffer = BeginSingleTimeCommands(queue);
		Buffer stagingBuffer(VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
		stagingBuffer.srcDataPointers = std::ref(buffer.srcDataPointers);
		stagingBuffer.Allocate(renderingDevice, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, true);
		buffer.usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
		buffer.Allocate(renderingDevice, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, false);
		Buffer::Copy(renderingDevice, cmdBuffer, stagingBuffer.buffer, buffer.buffer, buffer.size);
		EndSingleTimeCommands(queue, cmdBuffer);
		stagingBuffer.Free(renderingDevice);
	}
	void AllocateBuffersStaged(Queue queue, std::vector<Buffer>& buffers) {
		auto cmdBuffer = BeginSingleTimeCommands(queue);
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
		EndSingleTimeCommands(queue, cmdBuffer);
		for (auto& stagingBuffer : stagingBuffers) {
			stagingBuffer.Free(renderingDevice);
		}
	}
	void AllocateBuffersStaged(Queue queue, std::vector<Buffer*>& buffers) {
		auto cmdBuffer = BeginSingleTimeCommands(queue);
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
		EndSingleTimeCommands(queue, cmdBuffer);
		for (auto& stagingBuffer : stagingBuffers) {
			stagingBuffer.Free(renderingDevice);
		}
	}


	void TransitionImageLayout(VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t mipLevels = 1, uint32_t layerCount = 1) {
		auto commandBuffer = BeginSingleTimeCommands(graphicsQueue);
		TransitionImageLayout(commandBuffer, image, oldLayout, newLayout, mipLevels, layerCount);
		EndSingleTimeCommands(graphicsQueue, commandBuffer);
	}
	
	void TransitionImageLayout(VkCommandBuffer commandBuffer, VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t mipLevels = 1, uint32_t layerCount = 1) {
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
		barrier.subresourceRange.layerCount = layerCount;
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
		auto commandBuffer = BeginSingleTimeCommands(graphicsQueue);
		CopyBufferToImage(commandBuffer, buffer, image, width, height);
		EndSingleTimeCommands(graphicsQueue, commandBuffer);
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

		auto commandBuffer = BeginSingleTimeCommands(graphicsQueue);

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

		EndSingleTimeCommands(graphicsQueue, commandBuffer);
	}

protected: // Init/Reset Methods
	virtual void RecreateSwapChains() {
		std::scoped_lock lock(renderingMutex, lowPriorityRenderingMutex);
		
		DeleteGraphicsFromDevice();
		
		// Re-Create the SwapChain
		CreateSwapChain();
		
		SendGraphicsToDevice();
	}
	
public: // Init/Load/Reset Methods
	virtual void LoadRenderer() {
		std::scoped_lock lock(renderingMutex, lowPriorityRenderingMutex);
		
		Init();
		
		CreateDevices();
		CreateSyncObjects();
		CreateSwapChain();
		
		Info();
		
		LOG_SUCCESS("Vulkan Renderer is Ready !");
	}
	
	virtual void UnloadRenderer() {
		std::scoped_lock lock(renderingMutex, lowPriorityRenderingMutex);
		DestroySwapChain();
		DestroySyncObjects();
		DestroyDevices();
	}
	
	virtual void ReloadRenderer() {
		std::scoped_lock lock(renderingMutex, lowPriorityRenderingMutex);
		
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
		AllocateBuffers();
		CreateDescriptorSets();
		CreatePipelines(); // shaders are assigned here
		CreateCommandBuffers(); // objects are rendered here
	}
	
	virtual void DeleteGraphicsFromDevice() {
		// Wait for renderingDevice to be idle before destroying everything
		renderingDevice->DeviceWaitIdle(); // We can also wait for operations in a specific command queue to be finished with vkQueueWaitIdle. These functions can be used as a very rudimentary way to perform synchronization. 

		DestroyCommandBuffers();
		DestroyPipelines();
		DestroyDescriptorSets();
		FreeBuffers();
		DestroyResources();
		DestroyCommandPools();
	}
	
public: // Constructor & Destructor
	VulkanRenderer(Loader* loader, const char* applicationName, uint applicationVersion, Window* window)
	 : Instance(loader, applicationName, applicationVersion, true) {
		surface = window->CreateVulkanSurface(handle);
	}

	virtual ~VulkanRenderer() override {
		DestroySurfaceKHR(surface, nullptr);
	}

public: // Public Methods

	virtual void Render() {
		std::lock_guard lock(renderingMutex);
		
		uint64_t timeout = 1000 * 1000 * 1000; // one second
		
		// Wait for previous frame to be finished
		renderingDevice->WaitForFences(1/*fencesCount*/, &graphicsFences[currentFrameInFlight]/*fences array*/, VK_TRUE/*wait for all fences in this array*/, timeout/*timeout*/);
		renderingDevice->ResetFences(1, &graphicsFences[currentFrameInFlight]); // Unlike the semaphores, we manually need to restore the fence to the unsignaled state
		renderingDevice->WaitForFences(1/*fencesCount*/, &computeFences[currentFrameInFlight]/*fences array*/, VK_TRUE/*wait for all fences in this array*/, timeout/*timeout*/);
		renderingDevice->ResetFences(1, &computeFences[currentFrameInFlight]); // Unlike the semaphores, we manually need to restore the fence to the unsignaled state

		// Get an image from the swapchain
		uint imageIndex;
		VkResult result = renderingDevice->AcquireNextImageKHR(
			swapChain->GetHandle(), // swapChain
			timeout, // timeout in nanoseconds (using max disables the timeout)
			imageAvailableSemaphores[currentFrameInFlight], // semaphore
			VK_NULL_HANDLE, // fence
			&imageIndex // output the index of the swapchain image in there
		);

		// Check for errors
		if (result == VK_ERROR_OUT_OF_DATE_KHR) {
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
		
		std::array<VkSubmitInfo, 2> computeSubmitInfo {};
			computeSubmitInfo[0].sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
			computeSubmitInfo[1].sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		std::array<VkSubmitInfo, 2> graphicsSubmitInfo {};
			graphicsSubmitInfo[0].sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
			graphicsSubmitInfo[1].sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		
		std::array<VkSemaphore, 1> graphicsWaitSemaphores {
			imageAvailableSemaphores[currentFrameInFlight],
		};
		VkPipelineStageFlags graphicsWaitStages[] {
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		};
		
		{// Configure Compute
			VkCommandBufferBeginInfo beginInfo = {};
			beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
			beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
			
			renderingDevice->ResetCommandBuffer(computeDynamicCommandBuffers[imageIndex], 0);
			if (renderingDevice->BeginCommandBuffer(computeDynamicCommandBuffers[imageIndex], &beginInfo) != VK_SUCCESS) {
				throw std::runtime_error("Faild to begin recording command buffer");
			}
			RunDynamicCompute(computeDynamicCommandBuffers[imageIndex]);
			if (renderingDevice->EndCommandBuffer(computeDynamicCommandBuffers[imageIndex]) != VK_SUCCESS) {
				throw std::runtime_error("Failed to record command buffer");
			}
			// static commands
			computeSubmitInfo[0].waitSemaphoreCount = 0;
			computeSubmitInfo[0].pWaitSemaphores = nullptr;
			computeSubmitInfo[0].pWaitDstStageMask = nullptr;
			computeSubmitInfo[0].commandBufferCount = 1;
			computeSubmitInfo[0].pCommandBuffers = &computeCommandBuffers[imageIndex];
			computeSubmitInfo[0].signalSemaphoreCount = 1;
			computeSubmitInfo[0].pSignalSemaphores = &computeFinishedSemaphores[currentFrameInFlight];
			// dynamic commands
			computeSubmitInfo[1].waitSemaphoreCount = 0;
			computeSubmitInfo[1].pWaitSemaphores = nullptr;
			computeSubmitInfo[1].pWaitDstStageMask = nullptr;
			computeSubmitInfo[1].commandBufferCount = 1;
			computeSubmitInfo[1].pCommandBuffers = &computeDynamicCommandBuffers[imageIndex];
			computeSubmitInfo[1].signalSemaphoreCount = 1;
			computeSubmitInfo[1].pSignalSemaphores = &dynamicComputeFinishedSemaphores[currentFrameInFlight];
		}
		
		// Submit Compute
		result = renderingDevice->QueueSubmit(computeQueue.handle, computeSubmitInfo.size(), computeSubmitInfo.data(), computeFences[currentFrameInFlight]/*optional fence to be signaled*/);
		if (result != VK_SUCCESS) {
			if (result == VK_ERROR_DEVICE_LOST) {
				LOG_WARN("Render() Failed to submit compute command buffer : VK_ERROR_DEVICE_LOST. Reloading renderer...")
				SLEEP(500ms)
				ReloadRenderer();
				return;
			}
			LOG_ERROR((int)result)
			throw std::runtime_error("Render() Failed to submit compute command buffer");
		}

		{// Configure Graphics
			VkCommandBufferBeginInfo beginInfo = {};
			beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
			beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
			renderingDevice->ResetCommandBuffer(graphicsDynamicCommandBuffers[imageIndex], 0);
			if (renderingDevice->BeginCommandBuffer(graphicsDynamicCommandBuffers[imageIndex], &beginInfo) != VK_SUCCESS) {
				throw std::runtime_error("Faild to begin recording command buffer");
			}
			RunDynamicGraphics(graphicsDynamicCommandBuffers[imageIndex]);
			if (renderingDevice->EndCommandBuffer(graphicsDynamicCommandBuffers[imageIndex]) != VK_SUCCESS) {
				throw std::runtime_error("Failed to record command buffer");
			}
			// static commands
			graphicsSubmitInfo[0].waitSemaphoreCount = graphicsWaitSemaphores.size();
			graphicsSubmitInfo[0].pWaitSemaphores = graphicsWaitSemaphores.data();
			graphicsSubmitInfo[0].pWaitDstStageMask = graphicsWaitStages;
			graphicsSubmitInfo[0].commandBufferCount = 1;
			graphicsSubmitInfo[0].pCommandBuffers = &graphicsCommandBuffers[imageIndex];
			graphicsSubmitInfo[0].signalSemaphoreCount = 1;
			graphicsSubmitInfo[0].pSignalSemaphores = &renderFinishedSemaphores[currentFrameInFlight];
			// dynamic commands
			graphicsSubmitInfo[1].waitSemaphoreCount = 0;
			graphicsSubmitInfo[1].pWaitSemaphores = nullptr;
			graphicsSubmitInfo[1].pWaitDstStageMask = nullptr;
			graphicsSubmitInfo[1].commandBufferCount = 1;
			graphicsSubmitInfo[1].pCommandBuffers = &graphicsDynamicCommandBuffers[imageIndex];
			graphicsSubmitInfo[1].signalSemaphoreCount = 1;
			graphicsSubmitInfo[1].pSignalSemaphores = &dynamicRenderFinishedSemaphores[currentFrameInFlight];
		}
		
		// Submit Graphics
		result = renderingDevice->QueueSubmit(graphicsQueue.handle, graphicsSubmitInfo.size(), graphicsSubmitInfo.data(), graphicsFences[currentFrameInFlight]/*optional fence to be signaled*/);
		if (result != VK_SUCCESS) {
			if (result == VK_ERROR_DEVICE_LOST) {
				LOG_WARN("Render() Failed to submit graphics command buffer : VK_ERROR_DEVICE_LOST. Reloading renderer...")
				SLEEP(500ms)
				ReloadRenderer();
				return;
			}
			LOG_ERROR((int)result)
			throw std::runtime_error("Render() Failed to submit graphics command buffer");
		}

		// Present
		VkPresentInfoKHR presentInfo = {};
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		// Specify which semaphore to wait on before presentation can happen
		presentInfo.waitSemaphoreCount = 4;
		VkSemaphore presentWaitSemaphores[] = {
			renderFinishedSemaphores[currentFrameInFlight],
			dynamicRenderFinishedSemaphores[currentFrameInFlight],
			computeFinishedSemaphores[currentFrameInFlight],
			dynamicComputeFinishedSemaphores[currentFrameInFlight],
		};
		presentInfo.pWaitSemaphores = presentWaitSemaphores;
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
		result = renderingDevice->QueuePresentKHR(presentQueue.handle, &presentInfo);

		// Check for errors
		if (result == VK_ERROR_OUT_OF_DATE_KHR) {
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
	
	virtual void RenderLowPriority() {
		std::lock_guard lock(lowPriorityRenderingMutex);
	
		LowPriorityFrameUpdate();
		
		{// Static Compute
			VkSubmitInfo submitInfo = {};
			// first 3 params specify which semaphores to wait on before execution begins and in which stage(s) of the pipeline to wait.
			// We want to wait with writing colors to the image until it's available, so we're specifying the stage of the graphics pipeline that writes to the color attachment.
			// That means that theoretically the implementation can already start executing our vertex shader and such while the image is not yet available.
			// Each entry in the waitStages array corresponds to the semaphore with the same index in pWaitSemaphores.
			submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
			submitInfo.waitSemaphoreCount = 0;
			submitInfo.pWaitSemaphores = nullptr;
			submitInfo.pWaitDstStageMask = nullptr;
			// specify which command buffers to actually submit for execution
			submitInfo.commandBufferCount = 1;
			submitInfo.pCommandBuffers = &lowPriorityComputeCommandBuffer;
			// specify which semaphore to signal once the command buffer(s) have finished execution.
			submitInfo.signalSemaphoreCount = 0;
			submitInfo.pSignalSemaphores = nullptr;
			
			VkResult result = renderingDevice->QueueSubmit(lowPriorityComputeQueue.handle, 1/*count, for use of the next param*/, &submitInfo/*array, can have multiple!*/, VK_NULL_HANDLE/*optional fence to be signaled*/);
		
			if (result != VK_SUCCESS) {
				if (result == VK_ERROR_DEVICE_LOST) {
					LOG_WARN("RenderLowPriority() Failed to submit draw command buffer : VK_ERROR_DEVICE_LOST. Reloading renderer...")
					// SLEEP(500ms)
					// ReloadRenderer();
					return;
				}
				LOG_ERROR((int)result)
				throw std::runtime_error("RenderLowPriority() Failed to submit draw command buffer");
			}
		}
		
		// Dynamic compute
		auto computeCmd = BeginSingleTimeCommands(lowPriorityComputeQueue);
		RunDynamicLowPriorityCompute(computeCmd);
		EndSingleTimeCommands(lowPriorityComputeQueue, computeCmd);
		renderingDevice->QueueWaitIdle(lowPriorityComputeQueue.handle);
		
		{// Static Graphics
			VkSubmitInfo submitInfo = {};
			// first 3 params specify which semaphores to wait on before execution begins and in which stage(s) of the pipeline to wait.
			// We want to wait with writing colors to the image until it's available, so we're specifying the stage of the graphics pipeline that writes to the color attachment.
			// That means that theoretically the implementation can already start executing our vertex shader and such while the image is not yet available.
			// Each entry in the waitStages array corresponds to the semaphore with the same index in pWaitSemaphores.
			submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
			submitInfo.waitSemaphoreCount = 0;
			submitInfo.pWaitSemaphores = nullptr;
			submitInfo.pWaitDstStageMask = nullptr;
			// specify which command buffers to actually submit for execution
			submitInfo.commandBufferCount = 1;
			submitInfo.pCommandBuffers = &lowPriorityGraphicsCommandBuffer;
			// specify which semaphore to signal once the command buffer(s) have finished execution.
			submitInfo.signalSemaphoreCount = 0;
			submitInfo.pSignalSemaphores = nullptr;
			
			VkResult result = renderingDevice->QueueSubmit(lowPriorityGraphicsQueue.handle, 1/*count, for use of the next param*/, &submitInfo/*array, can have multiple!*/, VK_NULL_HANDLE/*optional fence to be signaled*/);
		
			if (result != VK_SUCCESS) {
				if (result == VK_ERROR_DEVICE_LOST) {
					LOG_WARN("RenderLowPriority() Failed to submit draw command buffer : VK_ERROR_DEVICE_LOST. Reloading renderer...")
					// SLEEP(500ms)
					// ReloadRenderer();
					return;
				}
				LOG_ERROR((int)result)
				throw std::runtime_error("RenderLowPriority() Failed to submit draw command buffer");
			}
		}
		
		// Dynamic graphics
		renderingDevice->QueueWaitIdle(lowPriorityGraphicsQueue.handle);
		auto graphicsCmd = BeginSingleTimeCommands(lowPriorityComputeQueue);
		RunDynamicLowPriorityGraphics(graphicsCmd);
		EndSingleTimeCommands(lowPriorityComputeQueue, graphicsCmd);
		
	}
	
};
