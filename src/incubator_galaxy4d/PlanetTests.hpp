#pragma once

#include <v4d.h>
#include "../incubator_rendering/V4DRenderingPipeline.hh"
#include "../incubator_rendering/Camera.hpp"

using namespace v4d::graphics;
using namespace v4d::graphics::vulkan;
using namespace v4d::graphics::vulkan::rtx;

class Planet {
public:
	
	void Init(Renderer* renderer) {
		
	}
	
	void Info(Renderer* renderer, Device* renderingDevice) {
		
	}
	
	void InitLayouts(Renderer* renderer, std::vector<DescriptorSet*>& descriptorSets, DescriptorSet* baseDescriptorSet_0) {
		
	}
	
	void ConfigureShaders() {
		
	}
	
	void ReadShaders() {
		
	}
	
	void CreateResources(Renderer* renderer, Device* renderingDevice, float screenWidth, float screenHeight) {
		
	}
	
	void DestroyResources(Device* renderingDevice) {
		
	}
	
	void AllocateBuffers(Device* renderingDevice) {
		
	}
	
	void FreeBuffers(Renderer* renderer, Device* renderingDevice) {
		
	}
	
	void CreatePipelines(Renderer* renderer, Device* renderingDevice, std::vector<RasterShaderPipeline*>& opaqueLightingShaders) {
		
		// opaqueLightingShaders.push_back(&);
		
	}
	
	void DestroyPipelines(Renderer* renderer, Device* renderingDevice) {
		
	}
	
	void RecordGraphicsCommandBuffer(VkCommandBuffer commandBuffer, int imageIndex) {
		
	}
	
	void RunInOpaqueLightingPass(Device* renderingDevice, VkCommandBuffer commandBuffer) {
		
	}
	
	void RunLowPriorityGraphics(Device* renderingDevice, VkCommandBuffer commandBuffer) {
		
	}
	
	void FrameUpdate(Renderer* renderer, Device* renderingDevice, Camera& mainCamera) {
		
	}
	
	void LowPriorityFrameUpdate(Renderer* renderer, Device* renderingDevice, Camera& mainCamera, Queue& lowPriorityGraphicsQueue) {
		
	}
	
	
	
	
};
