#pragma once

#include <v4d.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"

using namespace v4d::graphics::vulkan;

/* Image Layouts: 
	VK_IMAGE_LAYOUT_PRESENT_SRC_KHR : Optimal for presentation
	VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL : Optimal as attachment for writing colors from the fragment shader
	VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL : Optimal as source in a transfer operation, like vkCmdCopyImageToBuffer
	VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL : Optimal as destination in a transfer operation, like vkCmdCopyBufferToImage
	VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : Optimal for sampling from a shader
*/

class Texture2D {
private:
	std::string filepath;
	int req_components;
	int width, height, components;

public:
	Texture2D(const std::string& filepath, int req_components = STBI_default)
	: filepath(filepath), req_components(req_components) {
		LoadImagePixelsFromFile();
	}

	Texture2D(int width, int height, int components = STBI_rgb_alpha) 
	: filepath(""), req_components(components), width(width), height(height), components(components) {}

	~Texture2D() {
		if (pixelBufferStandalone) {
			FreeImagePixels();
		}
	}

	void FreeImagePixels() {
		if (pixels) stbi_image_free(pixels);
		// pixels = nullptr;
	}

	void SetImagePixelsPtr(unsigned char* pixels, int width, int height, int components) {
		if (pixels) FreeImagePixels();
		this->width = width;
		this->height = height;
		this->components = components;
		this->req_components = components;
		this->bufferSize = width * height * components;
		this->pixels = pixels;
		pixelBufferStandalone = false;
	}

	void SetImagePixelsCpy(unsigned char* pixels, int width, int height, int components) {
		if (pixels) FreeImagePixels();
		this->width = width;
		this->height = height;
		this->components = components;
		this->req_components = components;
		this->bufferSize = width * height * components;
		memcpy(this->pixels, pixels, bufferSize);
		pixelBufferStandalone = true;
	}

	inline int GetWidth() const { return width; }
	inline int GetHeight() const { return height; }

	size_t GetBufferSize() const { return bufferSize; }
	
	unsigned char* GetImagePixels() const {
		return pixels;
	}
	void GetImagePixels(void* buffer) const {
		memcpy(buffer, pixels, bufferSize);
	}

	inline Image* GetImage() {
		return &image;
	}

	inline Buffer* GetStagingBuffer() {
		return &stagingBuffer;
	}

	inline uint32_t GetMipLevels() const {
		return mipLevels;
	}

	uint32_t SetMipLevels(uint32_t mipLevels = 0) {
		if (mipLevels == 0) {
			mipLevels = (uint32_t)floor(log2(std::max(width, height))) + 1;
		}
		return this->mipLevels = mipLevels;
	}

	VkFormat GetFormatFromComponentCount() const {
		return GetFormatFromComponentCount(components);
	}

	VkFormat GetFormatFromReqComponentCount() const {
		return GetFormatFromComponentCount(req_components);
	}

	VkFormat GetFormat() const {
		return GetFormatFromReqComponentCount();
	}

	VkFormat GetFormatFromComponentCount(int n) const {
		switch (n) {
			case STBI_grey:
				return VK_FORMAT_R8_UNORM;
			case STBI_grey_alpha:
				return VK_FORMAT_R8G8_UNORM;
			case STBI_rgb:
				return VK_FORMAT_R8G8B8_UNORM;
			case STBI_rgb_alpha:
				return VK_FORMAT_R8G8B8A8_UNORM;
		}
		return VK_FORMAT_R8G8B8A8_UNORM;
	}

	VkImageView GetImageView() const {
		return image.view;
	}

	VkSampler GetSampler() const {
		return image.sampler;
	}

	void LoadImagePixelsFromFile() {
		// stbi_set_flip_vertically_on_load(1);
		pixels = stbi_load(filepath.c_str(), &width, &height, &components, req_components);
		bufferSize = width * height * req_components;
		if (!pixels){
			throw std::runtime_error("Failed to load texture '" + filepath + "' : " + stbi_failure_reason());
		}
		pixelBufferStandalone = true;
	}

	void AllocateAndWriteStagingMemory(Device* device) {
		if (!pixels) {
			if (filepath == "") {
				throw std::runtime_error("Pixels are empty and no filepath defined");
			}
			LoadImagePixelsFromFile();
		}
		stagingBuffer.size = bufferSize;
		stagingBuffer.Allocate(device, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, false);
		stagingBuffer.MapMemory(device);
			memcpy(stagingBuffer.data, pixels, bufferSize);
		stagingBuffer.UnmapMemory(device);
	}

	void FreeStagingMemory(Device* device) {
		stagingBuffer.Free(device);
	}
	
	void CopyStagingBufferToImage(Device* device, VkCommandBuffer commandBuffer, uint32_t generateMipLevels = 0) {
		if (generateMipLevels == 0) generateMipLevels = mipLevels;
		
		// Transition image layout for transfer
		VkImageMemoryBarrier barrier = {};
			barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			barrier.image = image.image;
			barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			barrier.subresourceRange.baseArrayLayer = 0;
			barrier.subresourceRange.layerCount = 1;
			barrier.subresourceRange.levelCount = generateMipLevels;
			barrier.subresourceRange.baseMipLevel = 0;
			barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		device->CmdPipelineBarrier(
			commandBuffer,
			VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0,
			0, nullptr,
			0, nullptr,
			1, &barrier
		);
		
		VkBufferImageCopy region = {};
			region.bufferOffset = 0;
			region.bufferRowLength = 0;
			region.bufferImageHeight = 0;
			region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			region.imageSubresource.mipLevel = 0;
			region.imageSubresource.baseArrayLayer = 0;
			region.imageSubresource.layerCount = 1;
			region.imageOffset = {0, 0, 0};
			region.imageExtent = {(uint32_t)width, (uint32_t)height, 1};

		device->CmdCopyBufferToImage(
			commandBuffer,
			stagingBuffer.buffer,
			image.image,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			1,
			&region
		);
		
		if (generateMipLevels > 1) {
			GenerateMipmaps(device, commandBuffer, generateMipLevels);
		}

		// Transition image layout to General
		barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		device->CmdPipelineBarrier(
			commandBuffer,
			VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0,
			0, nullptr,
			0, nullptr,
			1, &barrier
		);
	}
	
	void GenerateMipmaps(Device* device, VkCommandBuffer commandBuffer, uint32_t mipLevels) {
		VkFormatProperties formatProperties;
		device->GetPhysicalDevice()->GetPhysicalDeviceFormatProperties(GetFormatFromReqComponentCount(), &formatProperties);
		if (!(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT)) {
			throw std::runtime_error("Texture image format does not support linear blitting");
		}

		VkImageMemoryBarrier barrier = {};
			barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			barrier.image = image.image;
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
				VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0,
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
				image.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
				image.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				1, &blit,
				VK_FILTER_LINEAR
			);

			if (mipWidth > 1) mipWidth /= 2;
			if (mipHeight > 1) mipHeight /= 2;
		}

		barrier.subresourceRange.levelCount = mipLevels - 1;
		barrier.subresourceRange.baseMipLevel = 0;
		barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		device->CmdPipelineBarrier(
			commandBuffer,
			VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0,
			0, nullptr,
			0, nullptr,
			1, &barrier
		);
	}


	void CreateImage(Device* device, VkImageTiling tiling = VK_IMAGE_TILING_OPTIMAL, VkImageUsageFlags usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VkMemoryPropertyFlags memoryPropertyFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) {
		image.usage = usage;
		image.memoryPropertyFlags = memoryPropertyFlags;
		
		// Image
		VkImageCreateInfo& imageInfo = image.imageInfo;
			imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
			imageInfo.imageType = VK_IMAGE_TYPE_2D;
			imageInfo.extent.width = static_cast<uint32_t>(width);
			imageInfo.extent.height = static_cast<uint32_t>(height);
			imageInfo.extent.depth = 1;
			imageInfo.mipLevels = mipLevels;
			imageInfo.arrayLayers = 1;
			imageInfo.format = GetFormatFromReqComponentCount();
			imageInfo.tiling = tiling;
							/*
								VK_IMAGE_TILING_LINEAR : Texels are laid out in row-major order like our pixels array
								VK_IMAGE_TILING_OPTIMAL : Texels are laid out in an implementation defined order for optimal access
							*/
							// Unlike the layout of an image, the tiling mode cannot be changed at a later time.
							// If we want to be able to directly access texels in the memory of the image, then we must use VK_IMAGE_TILING_LINEAR
			imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
									/*
										VK_IMAGE_LAYOUT_UNDEFINED : Not usable by the PhysicalDevice and the very first transition will discard the texels
										VK_IMAGE_LAYOUT_PREINITIALIZED : Not usable by the PhysicalDevice, but the first transition will preserve the texels
									*/
									// There are few situations where it is necessary for the texels to be preserved during the first transition.
									// One example would be if we wanted to use an image as a staging image in combination with the VK_IMAGE_TILING_LINEAR layout.
									// In that case, we'd want to upload the texel data to it and then transition the image to be a transfer source without losing the data.
									// In our case, we're first going to transition the image to be a transfer destination and then copy texel data to it from a buffer object, so we dont need this property and can safely use VK_IMAGE_LAYOUT_UNDEFINED
			imageInfo.usage = usage;
			imageInfo.samples = VK_SAMPLE_COUNT_1_BIT; // Related to multisampling, only relevant for attachments
			imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
			imageInfo.flags = 0; // optional
		
		
		// Sampler
		VkSamplerCreateInfo& samplerInfo = image.samplerInfo;
			samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
			// VK_FILTER_LINEAR or VK_FILTER_NEAREST
			samplerInfo.magFilter = samplerMagFilter;
			samplerInfo.minFilter = samplerMinFilter;
			// VK_SAMPLER_ADDRESS_MODE_REPEAT, VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER
			samplerInfo.addressModeU = samplerAddressModeU;
			samplerInfo.addressModeV = samplerAddressModeV;
			samplerInfo.addressModeW = samplerAddressModeW;
			samplerInfo.borderColor = samplerBorderColor;
			// Anisotropy
			samplerInfo.anisotropyEnable = samplerAnisotropy>1 ? VK_TRUE : VK_FALSE;
			samplerInfo.maxAnisotropy = samplerAnisotropy;
			//
			samplerInfo.unnormalizedCoordinates = VK_FALSE;
			samplerInfo.compareEnable = VK_FALSE;
			samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
			// Mipmapping
			samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
			samplerInfo.mipLodBias = mipLodBias;
			samplerInfo.minLod = 0;
			samplerInfo.maxLod = mipLevels;
		
		
		// ImageView
		VkImageViewCreateInfo& viewInfo = image.viewInfo;
			viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			viewInfo.image = image.image;
			viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
			viewInfo.format = imageInfo.format;
			viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			viewInfo.subresourceRange.baseMipLevel = 0;
			viewInfo.subresourceRange.levelCount = mipLevels;
			viewInfo.subresourceRange.baseArrayLayer = 0;
			viewInfo.subresourceRange.layerCount = 1;


		image.Create(device, width, height, {imageInfo.format});
	}
	
	void DestroyImage(Device* device) {
		image.Destroy(device);
	}
	
	void SetSamplerAddressMode(VkSamplerAddressMode mode, VkBorderColor borderColor = VK_BORDER_COLOR_INT_TRANSPARENT_BLACK) {
		// VK_SAMPLER_ADDRESS_MODE_REPEAT, VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER
		samplerAddressModeU = mode;
		samplerAddressModeV = mode;
		samplerAddressModeW = mode;
		samplerBorderColor = borderColor;
	}

	void SetSamplerAddressMode(VkSamplerAddressMode u, VkSamplerAddressMode v, VkBorderColor borderColor = VK_BORDER_COLOR_INT_TRANSPARENT_BLACK) {
		samplerAddressModeU = u;
		samplerAddressModeV = v;
		samplerAddressModeW = v;
		samplerBorderColor = borderColor;
	}

	void SetSamplerAddressMode(VkSamplerAddressMode u, VkSamplerAddressMode v, VkSamplerAddressMode w, VkBorderColor borderColor = VK_BORDER_COLOR_INT_TRANSPARENT_BLACK) {
		samplerAddressModeU = u;
		samplerAddressModeV = v;
		samplerAddressModeW = w;
		samplerBorderColor = borderColor;
	}

	void SetSamplerFiltering(VkFilter magFilter,  VkFilter minFilter) {
		// VK_FILTER_LINEAR or VK_FILTER_NEAREST
		samplerMagFilter = magFilter;
		samplerMinFilter = minFilter;
	}

	void SetSamplerAnisotropy(float anisotropy) {
		samplerAnisotropy = anisotropy;
	}

private:

	unsigned char* pixels = nullptr;
	bool pixelBufferStandalone = false;

	size_t bufferSize;
	Buffer stagingBuffer {VK_BUFFER_USAGE_TRANSFER_SRC_BIT};

	Image image {0};
	uint32_t mipLevels = 1;
	float mipLodBias = 0;
	
	VkFilter samplerMagFilter = VK_FILTER_LINEAR;
	VkFilter samplerMinFilter = VK_FILTER_LINEAR;
	VkSamplerAddressMode samplerAddressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	VkSamplerAddressMode samplerAddressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	VkSamplerAddressMode samplerAddressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	VkBorderColor samplerBorderColor = VK_BORDER_COLOR_INT_TRANSPARENT_BLACK;
	float samplerAnisotropy = 1;

};
