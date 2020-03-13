#pragma once

#include <v4d.h>

namespace v4d::graphics {
	
	class RenderTargetGroup {
	public:
		// static const int GBUFFER_NB_IMAGES = 4;
		
	protected:
		
		// Render Target
		Image* targetImage = nullptr;
		SwapChain* targetSwapChain = nullptr;
		VkOffset2D offset {0,0};
		VkExtent2D extent {0,0};
		
		// Images
		Image litImage { VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT ,1,1, { VK_FORMAT_R16G16B16A16_SFLOAT }};
		Image ppImage { VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT ,1,1, { VK_FORMAT_R16G16B16A16_SFLOAT }};
		Image historyImage { VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT ,1,1, { VK_FORMAT_R16G16B16A16_SFLOAT }};
		// DepthStencilImage depthImage { VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT };
		Image depthImage { VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT ,1,1, { VK_FORMAT_R32_SFLOAT } };
		
		// enum GBUFFER : int {
		// 	ALBEDO = 0, 	// rgb16_sfloat, a16_sfloat = alpha
		// 	NORMAL = 1, 	// rgb16_sfloat, a16_sfloat = roughness
		// 	EMISSION = 2, 	// rgb16_sfloat, a16_sfloat = metallic
		// 	POSITION = 3 	// rgb32_sfloat, a32_sfloat = dist (trueDistanceFromCamera)
		// };
		// std::array<Image, GBUFFER_NB_IMAGES> gBuffers {
		// 	/* ALBEDO */	Image{ VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT ,1,1, { VK_FORMAT_R16G16B16A16_SFLOAT }},
		// 	/* NORMAL */	Image{ VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT ,1,1, { VK_FORMAT_R16G16B16A16_SFLOAT }},
		// 	/* EMISSION */	Image{ VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT ,1,1, { VK_FORMAT_R16G16B16A16_SFLOAT }},
		// 	/* POSITION */	Image{ VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT ,1,1, { VK_FORMAT_R32G32B32A32_SFLOAT }},
		// };
		
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
		
		Image& GetLitImage() {
			return litImage;
		}
		
		Image& GetPpImage() {
			return ppImage;
		}
		
		Image& GetHistoryImage() {
			return historyImage;
		}
		
		Image& GetDepthImage() {
			return depthImage;
		}
		
		// Image& GetGBuffer(int index) {
		// 	return gBuffers[index];
		// }
		
		// std::array<Image, GBUFFER_NB_IMAGES>& GetGBuffers() {
		// 	return gBuffers;
		// }
		
		// std::vector<VkClearValue> GetGBuffersClearValues() {
		// 	std::vector<VkClearValue> clearValues(GBUFFER_NB_IMAGES);
		// 	for (int i = 0; i < GBUFFER_NB_IMAGES; ++i)
		// 		clearValues[i] = {.0,.0,.0,.0};
		// 	return clearValues;
		// }
		
		#pragma endregion
		
		void CreateResources(Device* device) {
			uint rasterWidth = (uint)((float)extent.width);
			uint rasterHeight = (uint)((float)extent.height);
			
			litImage.Create(device, rasterWidth, rasterHeight);
			ppImage.Create(device, rasterWidth, rasterHeight);
			historyImage.Create(device, rasterWidth, rasterHeight);
			depthImage.Create(device, rasterWidth, rasterHeight);
			
			// for (auto& image : gBuffers) {
			// 	image.Create(device, rasterWidth, rasterHeight);
			// }
		}
		
		void DestroyResources(Device* device) {
			litImage.Destroy(device);
			ppImage.Destroy(device);
			historyImage.Destroy(device);
			depthImage.Destroy(device);
			
			// for (auto& image : gBuffers) {
			// 	image.Destroy(device);
			// }
		}
		
	};
}
