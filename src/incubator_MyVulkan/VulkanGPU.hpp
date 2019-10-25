#pragma once

#include "VulkanStructs.hpp"

using namespace std;

class VulkanGPU {
private:
	VkPhysicalDevice handle;

	VkPhysicalDeviceProperties deviceProperties;
	VkPhysicalDeviceFeatures deviceFeatures;
	vector<VkQueueFamilyProperties> *queueFamilies;
	vector<VkExtensionProperties> *supportedExtensions;

public:
	VulkanGPU(VkPhysicalDevice handle) : handle(handle) {
		// Properties
		vkGetPhysicalDeviceProperties(handle, &deviceProperties);
		LOG_VERBOSE("DETECTED GPU: " << deviceProperties.deviceName);
		// Features
		vkGetPhysicalDeviceFeatures(handle, &deviceFeatures);
		// Queue Families
		uint queueFamilyCount = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(handle, &queueFamilyCount, nullptr);
		queueFamilies = new vector<VkQueueFamilyProperties>(queueFamilyCount);
		vkGetPhysicalDeviceQueueFamilyProperties(handle, &queueFamilyCount, queueFamilies->data());
		// Supported Extensions
		uint supportedExtensionsCount = 0;
		vkEnumerateDeviceExtensionProperties(handle, nullptr, &supportedExtensionsCount, nullptr);
		supportedExtensions = new vector<VkExtensionProperties>(supportedExtensionsCount);
		vkEnumerateDeviceExtensionProperties(handle, nullptr, &supportedExtensionsCount, supportedExtensions->data());
	}

	~VulkanGPU() {
		delete queueFamilies;
		delete supportedExtensions;
	}

	inline int GetQueueFamilyIndexFromFlags(VkDeviceQueueCreateFlags flags, uint minQueuesCount = 1, VkSurfaceKHR surface = nullptr) {
		int i = 0;
		for (const auto& queueFamily : *queueFamilies) {
			if (queueFamily.queueCount >= minQueuesCount && queueFamily.queueFlags & flags) {
				if (surface == nullptr) return i;
				VkBool32 presentationSupport;
				vkGetPhysicalDeviceSurfaceSupportKHR(handle, i, surface, &presentationSupport);
				if (presentationSupport) return i;
			}
			i++;
		}
		return -1;
	}

	inline bool QueueFamiliesContainsFlags(VkDeviceQueueCreateFlags flags, uint minQueuesCount = 1, VkSurfaceKHR surface = nullptr) {
		for (const auto& queueFamily : *queueFamilies) {
			if (queueFamily.queueCount >= minQueuesCount && queueFamily.queueFlags & flags) {
				if (surface == nullptr) return true;
				VkBool32 presentationSupport;
				vkGetPhysicalDeviceSurfaceSupportKHR(handle, 0, surface, &presentationSupport);
				if (presentationSupport) return true;
			}
		}
		return false;
	}

	inline bool SupportsExtension(string ext) {
		for (const auto& extension : *supportedExtensions) {
			if (extension.extensionName == ext) {
				return true;
			}
		}
		return false;
	}

	inline VkPhysicalDeviceProperties GetProperties() const {
		return deviceProperties;
	}

	inline VkPhysicalDeviceFeatures GetFeatures() const {
		return deviceFeatures;
	}

	inline VkPhysicalDevice GetHandle() const {
		return handle;
	}

	inline string GetDescription() const {
		return GetProperties().deviceName;
	}

	uint FindMemoryType(uint typeFilter, VkMemoryPropertyFlags properties) {
		VkPhysicalDeviceMemoryProperties memProperties;
		vkGetPhysicalDeviceMemoryProperties(handle, &memProperties);
		for (uint i = 0; i < memProperties.memoryTypeCount; i++) {
			if (typeFilter & (1 << i) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
				return i;
			}
		}
		throw runtime_error("Failed to find suitable memory type");
	}

	VkFormat FindSupportedFormat(const vector<VkFormat> &candidates, VkImageTiling tiling, VkFormatFeatureFlags features) {
		for (VkFormat format : candidates) {
			VkFormatProperties props;
			vkGetPhysicalDeviceFormatProperties(handle, format, &props);
			if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) {
				return format;
			} else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) {
				return format;
			}
		}
		throw runtime_error("Failed to find supported format");
	}

	VkSampleCountFlagBits GetMaxUsableSampleCount() {
		VkSampleCountFlags counts = min(GetProperties().limits.framebufferColorSampleCounts, GetProperties().limits.framebufferDepthSampleCounts);
		if (counts & VK_SAMPLE_COUNT_64_BIT) { return VK_SAMPLE_COUNT_64_BIT; }
		if (counts & VK_SAMPLE_COUNT_32_BIT) { return VK_SAMPLE_COUNT_32_BIT; }
		if (counts & VK_SAMPLE_COUNT_16_BIT) { return VK_SAMPLE_COUNT_16_BIT; }
		if (counts & VK_SAMPLE_COUNT_8_BIT) { return VK_SAMPLE_COUNT_8_BIT; }
		if (counts & VK_SAMPLE_COUNT_4_BIT) { return VK_SAMPLE_COUNT_4_BIT; }
		if (counts & VK_SAMPLE_COUNT_2_BIT) { return VK_SAMPLE_COUNT_2_BIT; }
		return VK_SAMPLE_COUNT_1_BIT;
	}

};
