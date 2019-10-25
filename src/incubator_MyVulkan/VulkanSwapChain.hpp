#pragma once

#include "VulkanStructs.hpp"
#include "VulkanDevice.hpp"
#include "VulkanSurface.hpp"

using namespace std;

struct VulkanSwapChain {
private:
	VulkanDevice *device;
	VulkanSurface *surface;

	VkSwapchainKHR handle;
	vector<VkImage> images;

public:
	// Selected configuration
	VkSurfaceFormatKHR format;
	VkPresentModeKHR presentMode;
	VkExtent2D extent;

	VkPipelineViewportStateCreateInfo viewportState = {};
	VkViewport viewport = {};
	VkRect2D scissor = {};
	VkSwapchainCreateInfoKHR createInfo = {};
	VkImageViewCreateInfo imageViewsCreateInfo = {};
	vector<VkImageView> imageViews;

	// Constructor
	VulkanSwapChain(){}
	VulkanSwapChain(VulkanDevice *device, VulkanSurface *surface) : device(device), surface(surface) {
		ResolveCapabilities(device->GetPhysicalDeviceHandle());
		ResolveFormats(device->GetPhysicalDeviceHandle());
		ResolvePresentModes(device->GetPhysicalDeviceHandle());

		// Check Swap Chain Support
		if (formats.empty())
			throw runtime_error("GPU Swap Chain does not support any image format");
		if (presentModes.empty())
			throw runtime_error("GPU Swap Chain does not support any presentation mode");

		createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
		createInfo.surface = surface->GetHandle();
	}
	VulkanSwapChain(VulkanDevice *device, VulkanSurface *surface, VkExtent2D preferredExtent, const vector<VkSurfaceFormatKHR> preferredFormats, const vector<VkPresentModeKHR> preferredPresentModes)
	: VulkanSwapChain(device, surface) {
		SetConfiguration(preferredExtent, preferredFormats, preferredPresentModes);
	}

	~VulkanSwapChain() {
		for_each(imageViews.begin(), imageViews.end(), [this](const VkImageView& imageView) {
			vkDestroyImageView(this->device->GetHandle(), imageView, nullptr);
		});
		vkDestroySwapchainKHR(device->GetHandle(), handle, nullptr);
	}

	void SetConfiguration(VkExtent2D preferredExtent, const vector<VkSurfaceFormatKHR> preferredFormats, const vector<VkPresentModeKHR> preferredPresentModes) {
		// Get Preferred Extent
		extent = GetPreferredExtent(preferredExtent);
		// Get Preferred Format
		format = GetPreferredSurfaceFormat(preferredFormats);
		// Get Preferred Mode and Image Count
		presentMode = GetPreferredPresentMode(preferredPresentModes);

		// Default value for minImageCount
		if (presentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
			createInfo.minImageCount = capabilities.minImageCount + 1;
		} else {
			createInfo.minImageCount = capabilities.minImageCount;
		}

		// Assign part of the SwapChain Info Struct with default params
		createInfo.imageExtent = extent;
		createInfo.imageFormat = format.format;
		createInfo.imageColorSpace = format.colorSpace;
		createInfo.presentMode = presentMode;
		createInfo.imageArrayLayers = 1; // 1 for normal rendering, 2 for VR
		createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT; // VK_IMAGE_USAGE_TRANSFER_DST_BIT to render to a texture before doing post-processing
		createInfo.preTransform = capabilities.currentTransform; // We can specify that a certain transform should be applied to images in the swap chain if it is supported (supportedTransforms in capabilities), like a 90 degree clockwise rotation or horizontal flip. To specify that you do not want any transformation, simply specify the current transformation.
		createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR; // specifies if the alpha channel should be used for blending with other windows in the window system. You'll almost always want to simply ignore the alpha channel
		createInfo.clipped = VK_TRUE; // If the clipped member is set to VK_TRUE then that means that we don't care about the color of pixels that are obscured, for example because another window is in front of them. Unless you really need to be able to read these pixels back and get predictable results, you'll get the best performance by enabling clipping.
		createInfo.oldSwapchain = VK_NULL_HANDLE; // With Vulkan it's possible that your swap chain becomes invalid or unoptimized while your application is running, for example because the window was resized. In that case the swap chain actually needs to be recreated from scratch and a reference to the old one must be specified in this field.
		// Use AssignQueues() to override the block below
		createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		createInfo.queueFamilyIndexCount = 0; // Optional
		createInfo.pQueueFamilyIndices = nullptr; // Optional

		// Global ImageViews Settings
		imageViewsCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		imageViewsCreateInfo.format = format.format;
		imageViewsCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D; // Allows to treat images as 1D textures, 2D textures, 3D textures or cube maps.
		imageViewsCreateInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY; // The components field allows to swizzle the color channels around. For example, you can map all of the channels to the red channel for a monochrome texture. You can also map constant values of 0 and 1 to a channel. In our case we'll stick to the default mapping.
		imageViewsCreateInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		imageViewsCreateInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		imageViewsCreateInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
		imageViewsCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		imageViewsCreateInfo.subresourceRange.baseMipLevel = 0;
		imageViewsCreateInfo.subresourceRange.levelCount = 1;
		imageViewsCreateInfo.subresourceRange.baseArrayLayer = 0;
		imageViewsCreateInfo.subresourceRange.layerCount = 1;
	}

	void AssignQueues(vector<uint32_t> queues) {
		if (queues.size() < 2) throw runtime_error("AssignQueues needs at least two queues");
		if (queues[0] != queues[1]) {
			createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
			createInfo.queueFamilyIndexCount = queues.size();
			createInfo.pQueueFamilyIndices = queues.data();
		}
	}

	void Create(VulkanSwapChain *oldSwapChain = nullptr) {
		// Check for an old swapchain
		if (oldSwapChain != nullptr) createInfo.oldSwapchain = oldSwapChain->GetHandle();

		// Validation
		// Clamp minImageCount
		if (capabilities.maxImageCount > 0 && createInfo.minImageCount > capabilities.maxImageCount) {
			createInfo.minImageCount = capabilities.maxImageCount;
		}
		if (createInfo.minImageCount < capabilities.minImageCount) {
			createInfo.minImageCount = capabilities.minImageCount;
		}

		// Create the Handle
		if (vkCreateSwapchainKHR(device->GetHandle(), &createInfo, nullptr, &handle) != VK_SUCCESS) {
			throw runtime_error("Failed to create Swap Chain");
		}

		// Get Images and generate ImageViews
		uint imageCount;
		vkGetSwapchainImagesKHR(device->GetHandle(), handle, &imageCount, nullptr);
		images.resize(imageCount);
		imageViews.resize(imageCount);
		vkGetSwapchainImagesKHR(device->GetHandle(), handle, &imageCount, images.data());
		for (uint i = 0; i < imageCount; i++) {
			auto createInfo = imageViewsCreateInfo;
			createInfo.image = images[i];
			if (vkCreateImageView(device->GetHandle(), &createInfo, nullptr, &imageViews[i]) != VK_SUCCESS) {
				throw runtime_error("Failed to create image views");
			}
		}

		// Create Viewport State
		viewport.x = 0;
		viewport.y = 0;
		viewport.width = (float) extent.width;
		viewport.height = (float) extent.height;
		viewport.minDepth = 0;
		viewport.maxDepth = 1;
		scissor.offset = {0, 0};
		scissor.extent = extent;
		viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		viewportState.viewportCount = 1;
		viewportState.scissorCount = 1;
		viewportState.pViewports = &viewport;
		viewportState.pScissors = &scissor;
	}

	inline void ResolveCapabilities(VkPhysicalDevice physicalDevice) {
		vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface->GetHandle(), &capabilities);
	}

	inline void ResolveFormats(VkPhysicalDevice physicalDevice) {
		uint formatCount;
		vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface->GetHandle(), &formatCount, nullptr);
		if (formatCount > 0) {
			formats.resize(formatCount);
			vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface->GetHandle(), &formatCount, formats.data());
		}
	}

	inline void ResolvePresentModes(VkPhysicalDevice physicalDevice) {
		uint presentModeCount;
		vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface->GetHandle(), &presentModeCount, nullptr);
		if (presentModeCount > 0) {
			presentModes.resize(presentModeCount);
			vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface->GetHandle(), &presentModeCount, presentModes.data());
		}
	}

	VkExtent2D GetPreferredExtent(VkExtent2D preferredExtent) {
		if (capabilities.currentExtent.width != numeric_limits<uint32_t>::max()) {
			// Use current extent
			return capabilities.currentExtent;
		}
		// Use prefered extent Clamped between min and max supported extent if the window manager gives us flexibility
		preferredExtent.width = max(capabilities.minImageExtent.width, min(capabilities.maxImageExtent.width, preferredExtent.width));
		preferredExtent.height = max(capabilities.minImageExtent.height, min(capabilities.maxImageExtent.height, preferredExtent.height));
		return preferredExtent;
	}

	VkSurfaceFormatKHR GetPreferredSurfaceFormat(const vector<VkSurfaceFormatKHR> preferredFormats) {
		if (formats.size() == 1 && formats[0].format == VK_FORMAT_UNDEFINED) {
			// If the surface does not have a preferred format, use OUR first preferred format.
			return preferredFormats[0];
		} else {
			for (const auto &preferred : preferredFormats) {
				for (const auto &available : formats) {
					if (available.format == preferred.format && available.colorSpace == preferred.colorSpace) {
						return preferred;
					}
				}
			}
		}
		// If our preferred format is not in the list of supported formats, use the first supported format
		return formats[0];
	}

	VkPresentModeKHR GetPreferredPresentMode(const vector<VkPresentModeKHR> preferredPresentModes) {/*
		VK_PRESENT_MODE_IMMEDIATE_KHR: Images submitted by your application are transferred to the screen right away, which may result in tearing.
		VK_PRESENT_MODE_FIFO_KHR: The swap chain is a queue where the display takes an image from the front of the queue when the display is refreshed and the program inserts rendered images at the back of the queue. If the queue is full then the program has to wait. This is most similar to vertical sync as found in modern games. The moment that the display is refreshed is known as "vertical blank".
		VK_PRESENT_MODE_FIFO_RELAXED_KHR: This mode only differs from the previous one if the application is late and the queue was empty at the last vertical blank. Instead of waiting for the next vertical blank, the image is transferred right away when it finally arrives. This may result in visible tearing.
		VK_PRESENT_MODE_MAILBOX_KHR: This is another variation of the second mode. Instead of blocking the application when the queue is full, the images that are already queued are simply replaced with the newer ones. This mode can be used to implement triple buffering, which allows you to avoid tearing with significantly less latency issues than standard vertical sync that uses double buffering.
		*/
		for (const auto &preferred : preferredPresentModes) {
			for (const auto &available : presentModes) {
				if (available == preferred) {
					return preferred;
				}
			}
		}
		return presentModes[0];
	}

	inline VkSwapchainKHR GetHandle() const {
		return handle;
	}

private:

	// Supported configurations
	VkSurfaceCapabilitiesKHR capabilities;
	vector<VkSurfaceFormatKHR> formats;
	vector<VkPresentModeKHR> presentModes;

};
