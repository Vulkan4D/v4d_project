#pragma once

#include <v4d.h>

namespace v4d::graphics {
	class Camera {
	protected:
		
		// Configuration
		double fov = 70;
		double near = 0.01;
		double far = 1.5e17; // 1cm - 1 000 000 UA  (WTF!!! seems to be working great..... 32bit z-buffer is enough???)
		
		// Resolution scaling
		float rasterizationResolutionScale = 1.0;
		float thumbnailScale = 1.0/16;
		float rayTracingScale = 1.0/4;
	
		// Render Target
		Image* targetImage = nullptr;
		SwapChain* targetSwapChain = nullptr;
		VkOffset2D offset {0,0};
		VkExtent2D extent {0,0};
		
		// Dynamic variables
		glm::dvec3 viewDirection {};
		glm::dvec3 worldPosition {};
		glm::dvec3 velocity {};
		glm::dmat4 view {};
		glm::dmat4 projection {};
		
		// Images
		Image tmpImage { VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT };
		Image thumbnailImage { VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT };
		Image rayTracingImage { VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT };
		DepthStencilImage depthStencilImage {};
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
		std::array<Image, 8> gBuffers {
			/* ALBEDO */	Image{ VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT ,1,1, { VK_FORMAT_R32G32B32_SFLOAT, VK_FORMAT_R32G32B32A32_SFLOAT }},
			/* NORMAL */	Image{ VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT ,1,1, { VK_FORMAT_R8G8B8_SNORM, VK_FORMAT_R8G8B8A8_SNORM }},
			/* ROUGHNESS */	Image{ VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT ,1,1, { VK_FORMAT_R8_UNORM }},
			/* METALLIC */	Image{ VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT ,1,1, { VK_FORMAT_R8_UNORM }},
			/* SCATTER */	Image{ VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT ,1,1, { VK_FORMAT_R8_UNORM }},
			/* OCCLUSION */	Image{ VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT ,1,1, { VK_FORMAT_R8_UNORM }},
			/* EMISSION */	Image{ VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT ,1,1, { VK_FORMAT_R32G32B32_SFLOAT, VK_FORMAT_R32G32B32A32_SFLOAT }},
			/* POSITION */	Image{ VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT ,1,1, { VK_FORMAT_R32G32B32_SFLOAT, VK_FORMAT_R32G32B32A32_SFLOAT }},
		};
		
	public:
	
		#pragma region Setters/Getters
		
		void SetFov(double angle) {
			fov = angle;
		}
		void SetNearField(double nearField) {
			near = nearField;
		}
		void SetFarField(double farField) {
			far = farField;
		}
		
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
		
		void SetViewDirection(double x, double y, double z) {
			viewDirection.x = x;
			viewDirection.y = y;
			viewDirection.z = z;
		}
		void SetViewDirection(glm::dvec3 dir) {
			viewDirection = dir;
		}
		glm::dvec3 GetViewDirection() const {
			return viewDirection;
		}
		
		void SetWorldPosition(double x, double y, double z) {
			worldPosition.x = x;
			worldPosition.y = y;
			worldPosition.z = z;
		}
		void SetWorldPosition(glm::dvec3 pos) {
			worldPosition = pos;
		}
		glm::dvec3 GetWorldPosition() const {
			return worldPosition;
		}
		
		void SetVelocity(double x, double y, double z) {
			velocity.x = x;
			velocity.y = y;
			velocity.z = z;
		}
		void SetVelocity(glm::dvec3 v) {
			velocity = v;
		}
		glm::dvec3 GetVelocity() const {
			return velocity;
		}
		
		Image* GetTmpImage() {
			return &tmpImage;
		}
		
		glm::dmat4& GetProjectionMatrix() {
			return projection;
		}
		
		glm::dmat4 GetViewMatrix() {
			return view;
		}
		
		#pragma endregion
		
		void RefreshViewMatrix() {
			view = glm::lookAt(worldPosition, worldPosition + viewDirection, glm::dvec3(0,0,1));
		}
		
		void RefreshProjectionMatrix() {
			projection = glm::perspective(glm::radians(fov), (double) extent.width / extent.height, near, far);
			projection[1][1] *= -1;
		}
		
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
			depthStencilImage.Create(device, rasterWidth, rasterHeight);
			
			for (auto& image : gBuffers) {
				image.Create(device, rasterWidth, rasterHeight);
			}
		}
		
		void DestroyResources(Device* device) {
			tmpImage.Destroy(device);
			thumbnailImage.Destroy(device);
			rayTracingImage.Destroy(device);
			depthStencilImage.Destroy(device);
			
			for (auto& image : gBuffers) {
				image.Destroy(device);
			}
		}
		
	};
}
