#pragma once
#include <v4d.h>

using namespace v4d::graphics;

class MinimalRenderer : public v4d::graphics::Renderer {
	using v4d::graphics::Renderer::Renderer;
	
private: // Init
	void ScorePhysicalDeviceSelection(int& score, PhysicalDevice* physicalDevice) override {}
	void Init() override {}
	void Info() override {}
	void InitLayouts() override {}
	void ConfigureShaders() override {}

private: // Resources
	void CreateResources() override {}
	void DestroyResources() override {}
	void AllocateBuffers() override {}
	void FreeBuffers() override {}

private: // Graphics configuration
	void CreatePipelines() override {}
	void DestroyPipelines() override {}
	
private: // Commands
	void RecordGraphicsCommandBuffer(VkCommandBuffer commandBuffer, int imageIndex) override {
		// Current SwapChain Image must be in the correct layout before presenting, this is the minimum requirement for the renderer to work
		TransitionImageLayout(commandBuffer, swapChain->images[imageIndex], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
	}
	void RunDynamicGraphics(VkCommandBuffer) override {}
	void RunDynamicLowPriorityCompute(VkCommandBuffer) override {}
	void RunDynamicLowPriorityGraphics(VkCommandBuffer) override {}
	
public: // Scene configuration
	void LoadScene() override {}
	void UnloadScene() override {}
	void ReadShaders() override {}
	
public: // Update
	void FrameUpdate(uint imageIndex) override {}
	void LowPriorityFrameUpdate() override {}
};
