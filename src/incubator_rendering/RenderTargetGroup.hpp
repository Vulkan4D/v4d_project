#pragma once

#include <v4d.h>

namespace v4d::graphics {
	
	class RenderTargetGroup {
	public:
		static const int GBUFFER_NB_IMAGES = 8;
		
	protected:
		
		// Resolution scaling
		float rasterizationResolutionScale = 1.0;
		float thumbnailScale = 1.0/16;
		float rayTracingScale = 1.0/4;
	
		// Render Target
		Image* targetImage = nullptr;
		SwapChain* targetSwapChain = nullptr;
		VkOffset2D offset {0,0};
		VkExtent2D extent {0,0};
		
		// Images
		Image tmpImage { VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT };
		Image thumbnailImage { VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT };
		Image rayTracingImage { VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT };
		DepthStencilImage depthImage {};
		enum GBUFFER : int {
			ALBEDO = 0, 	// rgb32_sfloat
			NORMAL = 1, 	// rgb8_snorm
			ROUGHNESS = 2, 	// r8_unorm
			METALLIC = 3, 	// r8_unorm
			SCATTER = 4, 	// r8_unorm
			OCCLUSION = 5, 	// r8_unorm
			EMISSION = 6, 	// rgb32_sfloat
			POSITION = 7 	// rgb32_sfloat
		};
		std::array<Image, GBUFFER_NB_IMAGES> gBuffers {
			/* ALBEDO */	Image{ VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT ,1,1, { VK_FORMAT_R32G32B32A32_SFLOAT }},
			/* NORMAL */	Image{ VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT ,1,1, { VK_FORMAT_R8G8B8_SNORM, VK_FORMAT_R8G8B8A8_SNORM }},
			/* ROUGHNESS */	Image{ VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT ,1,1, { VK_FORMAT_R8_UNORM }},
			/* METALLIC */	Image{ VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT ,1,1, { VK_FORMAT_R8_UNORM }},
			/* SCATTER */	Image{ VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT ,1,1, { VK_FORMAT_R8_UNORM }},
			/* OCCLUSION */	Image{ VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT ,1,1, { VK_FORMAT_R8_UNORM }},
			/* EMISSION */	Image{ VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT ,1,1, { VK_FORMAT_R32G32B32_SFLOAT, VK_FORMAT_R32G32B32A32_SFLOAT }},
			/* POSITION */	Image{ VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT ,1,1, { VK_FORMAT_R32G32B32_SFLOAT, VK_FORMAT_R32G32B32A32_SFLOAT }},
		};
		
	public:
	
		#pragma region Setters/Getters
		
		void SetRenderTarget(SwapChain* target, VkOffset2D offset = {0,0}, VkExtent2D extent = {0,0}) {
			targetSwapChain = target;
			targetImage = nullptr;
			this->offset = offset;
			if (extent.width == 0) extent.width = target->extent.width - offset.x;
			if (extent.height == 0) extent.height = target->extent.height - offset.y;
			this->extent = extent;
		}
		
		void SetRenderTarget(Image* target, VkOffset2D offset = {0,0}, VkExtent2D extent = {0,0}) {
			targetSwapChain = nullptr;
			targetImage = target;
			this->offset = offset;
			if (extent.width == 0) extent.width = target->width - offset.x;
			if (extent.height == 0) extent.height = target->height - offset.y;
			this->extent = extent;
		}
		
		Image& GetTmpImage() {
			return tmpImage;
		}
		
		Image& GetThumbnailImage() {
			return thumbnailImage;
		}
		
		Image& GetDepthImage() {
			return depthImage;
		}
		
		Image& GetGBuffer(int index) {
			return gBuffers[index];
		}
		
		std::array<Image, GBUFFER_NB_IMAGES>& GetGBuffers() {
			return gBuffers;
		}
		
		std::vector<VkClearValue> GetGBuffersClearValues() {
			std::vector<VkClearValue> clearValues(GBUFFER_NB_IMAGES);
			for (int i = 0; i < GBUFFER_NB_IMAGES; ++i)
				clearValues[i] = {.0,.0,.0,.0};
			return clearValues;
		}
		
		#pragma endregion
		
		void CreateResources(Device* device) {
			uint rasterWidth = (uint)((float)extent.width * rasterizationResolutionScale);
			uint rasterHeight = (uint)((float)extent.height * rasterizationResolutionScale);
			uint thumbnailWidth = (uint)((float)extent.width * thumbnailScale);
			uint thumbnailHeight = (uint)((float)extent.height * thumbnailScale);
			uint rayTracingWidth = (uint)((float)extent.width * rayTracingScale);
			uint rayTracingHeight = (uint)((float)extent.height * rayTracingScale);
			
			tmpImage.Create(device, rasterWidth, rasterHeight);
			thumbnailImage.Create(device, thumbnailWidth, thumbnailHeight, {tmpImage.format});
			rayTracingImage.Create(device, rayTracingWidth, rayTracingHeight);
			depthImage.Create(device, rasterWidth, rasterHeight);
			
			for (auto& image : gBuffers) {
				image.Create(device, rasterWidth, rasterHeight);
			}
		}
		
		void DestroyResources(Device* device) {
			tmpImage.Destroy(device);
			thumbnailImage.Destroy(device);
			rayTracingImage.Destroy(device);
			depthImage.Destroy(device);
			
			for (auto& image : gBuffers) {
				image.Destroy(device);
			}
		}
		
	};
}
