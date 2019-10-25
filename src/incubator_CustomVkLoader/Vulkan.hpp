#pragma once

// This file will be automatically generated via tools/generateVulkanInterface
/*
	TODO: Generate this file from vulkan/vulkan_core.h

	Lines to match : 
		typedef\s*(void|VkResult)\s*\(VKAPI_PTR\s*\*PFN_vk(.*)\)\((.*)\);

			1 = return type
			2 = function name
			3 = args


	Instance functions : 
		args == ^(VkInstance|VkPhysicalDevice)

	Device functions : 
		args == ^(VkQueue|VkDevice|VkCommandBuffer)

	Remaining are global function pointers
*/

#include "VulkanLoader.hpp"

namespace Vulkan {
	
	class Loader : public VulkanLoader::BaseLoader {
	public:
	
		DEFINE_VULKAN_INTERFACE_FUNCTION( vkCreateInstance )
		DEFINE_VULKAN_INTERFACE_FUNCTION( vkEnumerateInstanceExtensionProperties )
		
		Loader() : BaseLoader() {
			LOAD_VULKAN_GLOBAL_FUNCTION( vkCreateInstance )
			LOAD_VULKAN_GLOBAL_FUNCTION( vkEnumerateInstanceExtensionProperties )
		}
	};
	
	class Instance : public VulkanLoader::BaseInstance {
	public:
	
		
		DEFINE_VULKAN_INTERFACE_FUNCTION( vkEnumeratePhysicalDevices )
		DEFINE_VULKAN_INTERFACE_FUNCTION( vkGetPhysicalDeviceProperties )
		DEFINE_VULKAN_INTERFACE_FUNCTION( vkGetPhysicalDeviceFeatures )
		DEFINE_VULKAN_INTERFACE_FUNCTION( vkGetPhysicalDeviceQueueFamilyProperties )
		DEFINE_VULKAN_INTERFACE_FUNCTION( vkCreateDevice )
		DEFINE_VULKAN_INTERFACE_FUNCTION( vkEnumerateDeviceExtensionProperties )
		DEFINE_VULKAN_INTERFACE_FUNCTION( vkGetPhysicalDeviceMemoryProperties )
		DEFINE_VULKAN_INTERFACE_FUNCTION( vkDestroyInstance )
		DEFINE_VULKAN_INTERFACE_FUNCTION( vkDestroySurfaceKHR )
		
		Instance(VulkanLoader::BaseLoader* loader, VkInstance& instance) : BaseInstance(loader, instance) {
			LOAD_VULKAN_INSTANCE_FUNCTION( vkEnumeratePhysicalDevices )
			LOAD_VULKAN_INSTANCE_FUNCTION( vkGetPhysicalDeviceProperties )
			LOAD_VULKAN_INSTANCE_FUNCTION( vkGetPhysicalDeviceFeatures )
			LOAD_VULKAN_INSTANCE_FUNCTION( vkGetPhysicalDeviceQueueFamilyProperties )
			LOAD_VULKAN_INSTANCE_FUNCTION( vkCreateDevice )
			LOAD_VULKAN_INSTANCE_FUNCTION( vkEnumerateDeviceExtensionProperties )
			LOAD_VULKAN_INSTANCE_FUNCTION( vkGetPhysicalDeviceMemoryProperties )
			LOAD_VULKAN_INSTANCE_FUNCTION( vkDestroyInstance )
			LOAD_VULKAN_INSTANCE_FUNCTION( vkDestroySurfaceKHR )
		}
	};
	
	class Device : public VulkanLoader::BaseDevice {
	public:
		
		DEFINE_VULKAN_INTERFACE_FUNCTION( vkGetDeviceQueue )
		DEFINE_VULKAN_INTERFACE_FUNCTION( vkDeviceWaitIdle )
		DEFINE_VULKAN_INTERFACE_FUNCTION( vkDestroyDevice )
		DEFINE_VULKAN_INTERFACE_FUNCTION( vkCreateSemaphore )
		DEFINE_VULKAN_INTERFACE_FUNCTION( vkCreateCommandPool )
		DEFINE_VULKAN_INTERFACE_FUNCTION( vkAllocateCommandBuffers )
		DEFINE_VULKAN_INTERFACE_FUNCTION( vkBeginCommandBuffer )
		DEFINE_VULKAN_INTERFACE_FUNCTION( vkCmdPipelineBarrier )
		DEFINE_VULKAN_INTERFACE_FUNCTION( vkCmdClearColorImage )
		DEFINE_VULKAN_INTERFACE_FUNCTION( vkEndCommandBuffer )
		DEFINE_VULKAN_INTERFACE_FUNCTION( vkQueueSubmit )
		DEFINE_VULKAN_INTERFACE_FUNCTION( vkFreeCommandBuffers )
		DEFINE_VULKAN_INTERFACE_FUNCTION( vkDestroyCommandPool )
		DEFINE_VULKAN_INTERFACE_FUNCTION( vkDestroySemaphore )
		DEFINE_VULKAN_INTERFACE_FUNCTION( vkCreateImageView )
		DEFINE_VULKAN_INTERFACE_FUNCTION( vkCreateRenderPass )
		DEFINE_VULKAN_INTERFACE_FUNCTION( vkCreateFramebuffer )
		DEFINE_VULKAN_INTERFACE_FUNCTION( vkCreateShaderModule )
		DEFINE_VULKAN_INTERFACE_FUNCTION( vkCreatePipelineLayout )
		DEFINE_VULKAN_INTERFACE_FUNCTION( vkCreateGraphicsPipelines )
		DEFINE_VULKAN_INTERFACE_FUNCTION( vkCmdBeginRenderPass )
		DEFINE_VULKAN_INTERFACE_FUNCTION( vkCmdBindPipeline )
		DEFINE_VULKAN_INTERFACE_FUNCTION( vkCmdDraw )
		DEFINE_VULKAN_INTERFACE_FUNCTION( vkCmdEndRenderPass )
		DEFINE_VULKAN_INTERFACE_FUNCTION( vkDestroyShaderModule )
		DEFINE_VULKAN_INTERFACE_FUNCTION( vkDestroyPipelineLayout )
		DEFINE_VULKAN_INTERFACE_FUNCTION( vkDestroyPipeline )
		DEFINE_VULKAN_INTERFACE_FUNCTION( vkDestroyRenderPass )
		DEFINE_VULKAN_INTERFACE_FUNCTION( vkDestroyFramebuffer )
		DEFINE_VULKAN_INTERFACE_FUNCTION( vkDestroyImageView )
		DEFINE_VULKAN_INTERFACE_FUNCTION( vkCreateFence )
		DEFINE_VULKAN_INTERFACE_FUNCTION( vkCreateBuffer )
		DEFINE_VULKAN_INTERFACE_FUNCTION( vkGetBufferMemoryRequirements )
		DEFINE_VULKAN_INTERFACE_FUNCTION( vkAllocateMemory )
		DEFINE_VULKAN_INTERFACE_FUNCTION( vkBindBufferMemory )
		DEFINE_VULKAN_INTERFACE_FUNCTION( vkMapMemory )
		DEFINE_VULKAN_INTERFACE_FUNCTION( vkFlushMappedMemoryRanges )
		DEFINE_VULKAN_INTERFACE_FUNCTION( vkUnmapMemory )
		DEFINE_VULKAN_INTERFACE_FUNCTION( vkCmdSetViewport )
		DEFINE_VULKAN_INTERFACE_FUNCTION( vkCmdSetScissor )
		DEFINE_VULKAN_INTERFACE_FUNCTION( vkCmdBindVertexBuffers )
		DEFINE_VULKAN_INTERFACE_FUNCTION( vkWaitForFences )
		DEFINE_VULKAN_INTERFACE_FUNCTION( vkResetFences )
		DEFINE_VULKAN_INTERFACE_FUNCTION( vkFreeMemory )
		DEFINE_VULKAN_INTERFACE_FUNCTION( vkDestroyBuffer )
		DEFINE_VULKAN_INTERFACE_FUNCTION( vkDestroyFence )
		DEFINE_VULKAN_INTERFACE_FUNCTION( vkCmdCopyBuffer )
		DEFINE_VULKAN_INTERFACE_FUNCTION( vkCreateImage )
		DEFINE_VULKAN_INTERFACE_FUNCTION( vkGetImageMemoryRequirements )
		DEFINE_VULKAN_INTERFACE_FUNCTION( vkBindImageMemory )
		DEFINE_VULKAN_INTERFACE_FUNCTION( vkCreateSampler )
		DEFINE_VULKAN_INTERFACE_FUNCTION( vkCmdCopyBufferToImage )
		DEFINE_VULKAN_INTERFACE_FUNCTION( vkCreateDescriptorSetLayout )
		DEFINE_VULKAN_INTERFACE_FUNCTION( vkCreateDescriptorPool )
		DEFINE_VULKAN_INTERFACE_FUNCTION( vkAllocateDescriptorSets )
		DEFINE_VULKAN_INTERFACE_FUNCTION( vkUpdateDescriptorSets )
		DEFINE_VULKAN_INTERFACE_FUNCTION( vkCmdBindDescriptorSets )
		DEFINE_VULKAN_INTERFACE_FUNCTION( vkDestroyDescriptorPool )
		DEFINE_VULKAN_INTERFACE_FUNCTION( vkDestroyDescriptorSetLayout )
		DEFINE_VULKAN_INTERFACE_FUNCTION( vkDestroySampler )
		DEFINE_VULKAN_INTERFACE_FUNCTION( vkDestroyImage )
		
		Device(VulkanLoader::BaseInstance* instance, VkDevice& device) : BaseDevice(instance, device) {
			LOAD_VULKAN_DEVICE_FUNCTION( vkGetDeviceQueue )
			LOAD_VULKAN_DEVICE_FUNCTION( vkDeviceWaitIdle )
			LOAD_VULKAN_DEVICE_FUNCTION( vkDestroyDevice )
			LOAD_VULKAN_DEVICE_FUNCTION( vkCreateSemaphore )
			LOAD_VULKAN_DEVICE_FUNCTION( vkCreateCommandPool )
			LOAD_VULKAN_DEVICE_FUNCTION( vkAllocateCommandBuffers )
			LOAD_VULKAN_DEVICE_FUNCTION( vkBeginCommandBuffer )
			LOAD_VULKAN_DEVICE_FUNCTION( vkCmdPipelineBarrier )
			LOAD_VULKAN_DEVICE_FUNCTION( vkCmdClearColorImage )
			LOAD_VULKAN_DEVICE_FUNCTION( vkEndCommandBuffer )
			LOAD_VULKAN_DEVICE_FUNCTION( vkQueueSubmit )
			LOAD_VULKAN_DEVICE_FUNCTION( vkFreeCommandBuffers )
			LOAD_VULKAN_DEVICE_FUNCTION( vkDestroyCommandPool )
			LOAD_VULKAN_DEVICE_FUNCTION( vkDestroySemaphore )
			LOAD_VULKAN_DEVICE_FUNCTION( vkCreateImageView )
			LOAD_VULKAN_DEVICE_FUNCTION( vkCreateRenderPass )
			LOAD_VULKAN_DEVICE_FUNCTION( vkCreateFramebuffer )
			LOAD_VULKAN_DEVICE_FUNCTION( vkCreateShaderModule )
			LOAD_VULKAN_DEVICE_FUNCTION( vkCreatePipelineLayout )
			LOAD_VULKAN_DEVICE_FUNCTION( vkCreateGraphicsPipelines )
			LOAD_VULKAN_DEVICE_FUNCTION( vkCmdBeginRenderPass )
			LOAD_VULKAN_DEVICE_FUNCTION( vkCmdBindPipeline )
			LOAD_VULKAN_DEVICE_FUNCTION( vkCmdDraw )
			LOAD_VULKAN_DEVICE_FUNCTION( vkCmdEndRenderPass )
			LOAD_VULKAN_DEVICE_FUNCTION( vkDestroyShaderModule )
			LOAD_VULKAN_DEVICE_FUNCTION( vkDestroyPipelineLayout )
			LOAD_VULKAN_DEVICE_FUNCTION( vkDestroyPipeline )
			LOAD_VULKAN_DEVICE_FUNCTION( vkDestroyRenderPass )
			LOAD_VULKAN_DEVICE_FUNCTION( vkDestroyFramebuffer )
			LOAD_VULKAN_DEVICE_FUNCTION( vkDestroyImageView )
			LOAD_VULKAN_DEVICE_FUNCTION( vkCreateFence )
			LOAD_VULKAN_DEVICE_FUNCTION( vkCreateBuffer )
			LOAD_VULKAN_DEVICE_FUNCTION( vkGetBufferMemoryRequirements )
			LOAD_VULKAN_DEVICE_FUNCTION( vkAllocateMemory )
			LOAD_VULKAN_DEVICE_FUNCTION( vkBindBufferMemory )
			LOAD_VULKAN_DEVICE_FUNCTION( vkMapMemory )
			LOAD_VULKAN_DEVICE_FUNCTION( vkFlushMappedMemoryRanges )
			LOAD_VULKAN_DEVICE_FUNCTION( vkUnmapMemory )
			LOAD_VULKAN_DEVICE_FUNCTION( vkCmdSetViewport )
			LOAD_VULKAN_DEVICE_FUNCTION( vkCmdSetScissor )
			LOAD_VULKAN_DEVICE_FUNCTION( vkCmdBindVertexBuffers )
			LOAD_VULKAN_DEVICE_FUNCTION( vkWaitForFences )
			LOAD_VULKAN_DEVICE_FUNCTION( vkResetFences )
			LOAD_VULKAN_DEVICE_FUNCTION( vkFreeMemory )
			LOAD_VULKAN_DEVICE_FUNCTION( vkDestroyBuffer )
			LOAD_VULKAN_DEVICE_FUNCTION( vkDestroyFence )
			LOAD_VULKAN_DEVICE_FUNCTION( vkCmdCopyBuffer )
			LOAD_VULKAN_DEVICE_FUNCTION( vkCreateImage )
			LOAD_VULKAN_DEVICE_FUNCTION( vkGetImageMemoryRequirements )
			LOAD_VULKAN_DEVICE_FUNCTION( vkBindImageMemory )
			LOAD_VULKAN_DEVICE_FUNCTION( vkCreateSampler )
			LOAD_VULKAN_DEVICE_FUNCTION( vkCmdCopyBufferToImage )
			LOAD_VULKAN_DEVICE_FUNCTION( vkCreateDescriptorSetLayout )
			LOAD_VULKAN_DEVICE_FUNCTION( vkCreateDescriptorPool )
			LOAD_VULKAN_DEVICE_FUNCTION( vkAllocateDescriptorSets )
			LOAD_VULKAN_DEVICE_FUNCTION( vkUpdateDescriptorSets )
			LOAD_VULKAN_DEVICE_FUNCTION( vkCmdBindDescriptorSets )
			LOAD_VULKAN_DEVICE_FUNCTION( vkDestroyDescriptorPool )
			LOAD_VULKAN_DEVICE_FUNCTION( vkDestroyDescriptorSetLayout )
			LOAD_VULKAN_DEVICE_FUNCTION( vkDestroySampler )
			LOAD_VULKAN_DEVICE_FUNCTION( vkDestroyImage )
		}
	};
	
}
