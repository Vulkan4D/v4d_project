#pragma once
#include <v4d.h>

using namespace v4d::graphics;

class V4DRenderer : public v4d::graphics::Renderer {
	using v4d::graphics::Renderer::Renderer;
	
	// RenderPass renderPass;
	// PipelineLayout testLayout;
	// RasterShaderPipeline* testShader;

private: // Init
	void ScorePhysicalDeviceSelection(int& score, PhysicalDevice* physicalDevice) override {}
	void Init() override {}
	void Info() override {}

private: // Resources
	void CreateResources() override {}
	void DestroyResources() override {}
	void AllocateBuffers() override {}
	void FreeBuffers() override {}

private: // Graphics configuration
	void CreatePipelines() override {}
	void DestroyPipelines() override {}
	
private: // Commands
	void RecordComputeCommandBuffer(VkCommandBuffer, int imageIndex) override {}
	void RecordGraphicsCommandBuffer(VkCommandBuffer commandBuffer, int imageIndex) override {
		TransitionImageLayout(swapChain->images[imageIndex], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
	}
	void RecordLowPriorityComputeCommandBuffer(VkCommandBuffer) override {}
	void RecordLowPriorityGraphicsCommandBuffer(VkCommandBuffer) override {}
	void RunDynamicCompute(VkCommandBuffer) override {}
	void RunDynamicGraphics(VkCommandBuffer) override {}
	void RunDynamicLowPriorityCompute(VkCommandBuffer) override {}
	void RunDynamicLowPriorityGraphics(VkCommandBuffer) override {}
	
public: // Scene configuration
	void LoadScene() override {}
	void UnloadScene() override {}
	
public: // Update
	void FrameUpdate(uint imageIndex) override {}
	void LowPriorityFrameUpdate() override {}
	
public: // ubo/conditional member variables
	bool continuousGalaxyGen = true;
	
};
