#pragma once

#include "VulkanStructs.hpp"

#include "VulkanDevice.hpp"

// https://github.com/nothings/stb
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

using namespace std;

/* Image Layouts: 
	VK_IMAGE_LAYOUT_PRESENT_SRC_KHR : Optimal for presentation
	VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL : Optimal as attachment for writing colors from the fragment shader
	VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL : Optimal as source in a transfer operation, like vkCmdCopyImageToBuffer
	VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL : Optimal as destination in a transfer operation, like vkCmdCopyBufferToImage
	VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : Optimal for sampling from a shader
*/

class Texture2D {
private:
	string filepath;
	int req_components;
	int width, height, components;

public:
	Texture2D(const string& filepath, int req_components = STBI_default)
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
	void GetImagePixels(void *buffer) const {
		memcpy(buffer, pixels, bufferSize);
	}

	inline VkImage GetImage() const {
		return image;
	}

	inline VkBuffer GetStagingBuffer() const {
		return stagingBuffer;
	}

	inline uint32_t GetMipLevels() const {
		return mipLevels;
	}

	uint32_t SetMipLevels(uint32_t mipLevels = 0) {
		if (mipLevels == 0) {
			mipLevels = (uint32_t)floor(log2(max(width, height))) + 1;
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
		return imageView;
	}

	VkSampler GetSampler() const {
		return sampler;
	}

	void LoadImagePixelsFromFile() {
		// stbi_set_flip_vertically_on_load(1);
		pixels = stbi_load(filepath.c_str(), &width, &height, &components, req_components);
		bufferSize = width * height * req_components;
		if (!pixels){
			throw runtime_error("Failed to load texture '" + filepath + "' : " + stbi_failure_reason());
		}
		pixelBufferStandalone = true;
	}

	void AllocateVulkanStagingMemory(VulkanDevice *device) {
		if (!pixels) {
			if (filepath == "") {
				throw runtime_error("Pixels are empty and no filepath defined");
			}
			LoadImagePixelsFromFile();
		}
		VkDeviceSize size = bufferSize;
		device->CreateBuffer(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);
		void *data;
		vkMapMemory(device->GetHandle(), stagingBufferMemory, 0, size, 0, &data);
			memcpy(data, pixels, bufferSize);
		vkUnmapMemory(device->GetHandle(), stagingBufferMemory);
	}

	void FreeVulkanStagingMemory(VulkanDevice *device) {
		vkDestroyBuffer(device->GetHandle(), stagingBuffer, nullptr);
		vkFreeMemory(device->GetHandle(), stagingBufferMemory, nullptr);
	}

	void CreateVulkanImage(VulkanDevice *device, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags memoryPropertyFlags) {
		VkImageCreateInfo imageInfo = {};
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
									VK_IMAGE_LAYOUT_UNDEFINED : Not usable by the GPU and the very first transition will discard the texels
									VK_IMAGE_LAYOUT_PREINITIALIZED : Not usable by the GPU, but the first transition will preserve the texels
								*/
								// There are few situations where it is necessary for the texels to be preserved during the first transition.
								// One example would be if we wanted to use an image as a staging image in combination with the VK_IMAGE_TILING_LINEAR layout.
								// In that case, we'd want to upload the texel data to it and then transition the image to be a transfer source without losing the data.
								// In our case, we're first going to transition the image to be a transfer destination and then copy texel data to it from a buffer object, so we dont need this property and can safely use VK_IMAGE_LAYOUT_UNDEFINED
		imageInfo.usage = usage;
		imageInfo.samples = VK_SAMPLE_COUNT_1_BIT; // Related to multisampling, only relevant for attachments
		imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		imageInfo.flags = 0; // optional

		if (vkCreateImage(device->GetHandle(), &imageInfo, nullptr, &image) != VK_SUCCESS) {
			throw runtime_error("Failed to create image");
		}
		
		VkMemoryRequirements memRequirements;
		vkGetImageMemoryRequirements(device->GetHandle(), image, &memRequirements);

		VkMemoryAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize = memRequirements.size;
		allocInfo.memoryTypeIndex = device->GetGPU()->FindMemoryType(memRequirements.memoryTypeBits, memoryPropertyFlags);
		
		if (vkAllocateMemory(device->GetHandle(), &allocInfo, nullptr, &imageMemory) != VK_SUCCESS) {
			throw runtime_error("Failed to allocate image memory");
		}

		vkBindImageMemory(device->GetHandle(), image, imageMemory, 0);
	}

	void DestroyVulkanImage(VulkanDevice *device) {
		vkDestroyImage(device->GetHandle(), image, nullptr);
		vkFreeMemory(device->GetHandle(), imageMemory, nullptr);
	}

	void CreateVulkanImageView(VulkanDevice *device) {
		VkImageViewCreateInfo viewInfo = {};
		viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewInfo.image = image;
		viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		viewInfo.format = GetFormatFromReqComponentCount();
		viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		viewInfo.subresourceRange.baseMipLevel = 0;
		viewInfo.subresourceRange.levelCount = mipLevels;
		viewInfo.subresourceRange.baseArrayLayer = 0;
		viewInfo.subresourceRange.layerCount = 1;

		if (vkCreateImageView(device->GetHandle(), &viewInfo, nullptr, &imageView) != VK_SUCCESS) {
			throw runtime_error("Failed to create texture image view");
		}
	}

	void DestroyVulkanImageView(VulkanDevice *device) {
		vkDestroyImageView(device->GetHandle(), imageView, nullptr);
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

	void CreateVulkanSampler(VulkanDevice *device, float mipLodBias = 0) {
		VkSamplerCreateInfo samplerInfo = {};
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
		//
		if (vkCreateSampler(device->GetHandle(), &samplerInfo, nullptr, &sampler) != VK_SUCCESS) {
			throw runtime_error("Failed to create texture sampler");
		}
	}

	void DestroyVulkanSampler(VulkanDevice *device) {
		vkDestroySampler(device->GetHandle(), sampler, nullptr);
	}

private:

	unsigned char* pixels = nullptr;
	bool pixelBufferStandalone = false;
	size_t bufferSize;

	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;

	VkImage image;
	VkDeviceMemory imageMemory;
	uint32_t mipLevels = 1;
	
	VkImageView imageView;

	VkSampler sampler;
	VkFilter samplerMagFilter = VK_FILTER_LINEAR;
	VkFilter samplerMinFilter = VK_FILTER_LINEAR;
	VkSamplerAddressMode samplerAddressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	VkSamplerAddressMode samplerAddressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	VkSamplerAddressMode samplerAddressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	VkBorderColor samplerBorderColor = VK_BORDER_COLOR_INT_TRANSPARENT_BLACK;
	float samplerAnisotropy = 1;


};
