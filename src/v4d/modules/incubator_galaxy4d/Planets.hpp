#pragma once
#include <v4d.h>

using namespace v4d::graphics;
using namespace v4d::graphics::vulkan;

class Planets : public v4d::modules::Rendering {
	
public:

	// Executed when calling InitRenderer() on the main Renderer
	void Init() override {}
	void InitLayouts(std::vector<DescriptorSet*>&) override {}
	void ConfigureShaders(std::unordered_map<std::string, std::vector<RasterShaderPipeline*>>& shaders) override {}
	
	// Executed when calling their respective methods on the main Renderer
	void ReadShaders() override {}
	void LoadScene() override {}
	void UnloadScene() override {}
	
	// Executed when calling LoadRenderer()
	void ScorePhysicalDeviceSelection(int& score, PhysicalDevice*) override {}
	// after selecting rendering device and queues
	void Info() override {}
	void CreateResources() {} // here we have the swapChain available
	void DestroyResources() override {}
	void AllocateBuffers() override {}
	void FreeBuffers() override {}
	void CreatePipelines() override {}
	void DestroyPipelines() override {}
	
	// Rendering methods potentially executed on each frame
	void RunDynamicGraphicsTop(VkCommandBuffer) override {}
	void RunDynamicGraphicsBottom(VkCommandBuffer) override {}
	void RunDynamicLowPriorityCompute(VkCommandBuffer) override {}
	void RunDynamicLowPriorityGraphics(VkCommandBuffer) override {}
	
	// Executed before each frame
	void FrameUpdate(uint imageIndex, glm::dmat4& projection, glm::dmat4& view) override {}
	void LowPriorityFrameUpdate() override {}
	
};
