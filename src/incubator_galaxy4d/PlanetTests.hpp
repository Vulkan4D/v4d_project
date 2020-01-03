#pragma once

#include <v4d.h>
#include "../incubator_rendering/V4DRenderingPipeline.hh"
#include "../incubator_rendering/Camera.hpp"

using namespace v4d::graphics;
using namespace v4d::graphics::vulkan;

class Planet {
	
	PipelineLayout planetPipelineLayout;
	RasterShaderPipeline planetShader {planetPipelineLayout, {
		"incubator_galaxy4d/assets/shaders/planet.vert",
		"incubator_galaxy4d/assets/shaders/planet.frag",
	}};
	
	struct PlanetInfo {
		double radius;
	};
	
	struct PlanetChunk { // max 128 bytes
		glm::dvec3 planetPosition; // 24
		int planetIndex; // 4
		int chunkIndex; // 4
		glm::dvec3 chunkPosOnPlanet; // 24
		float chunkSize; // 4
		// bytes remaining: 68
	} planetChunkPushConstant;
	
	std::array<PlanetInfo, 255> planets {};
	Buffer planetsBuffer { VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, sizeof(PlanetInfo) * 255 };
	
public:
	
	void Init(Renderer* renderer) {
		planetsBuffer.AddSrcDataPtr<PlanetInfo, 255>(&planets);
	}
	
	void Info(Renderer* renderer, Device* renderingDevice) {
		
	}
	
	void InitLayouts(Renderer* renderer, std::vector<DescriptorSet*>& descriptorSets, DescriptorSet* baseDescriptorSet_0) {
		auto* planetDescriptorSet_1 = descriptorSets.emplace_back(new DescriptorSet(1));
		planetDescriptorSet_1->AddBinding_uniformBuffer(0, &planetsBuffer, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
		
		planetPipelineLayout.AddDescriptorSet(baseDescriptorSet_0);
		planetPipelineLayout.AddDescriptorSet(planetDescriptorSet_1);
		planetPipelineLayout.AddPushConstant<PlanetChunk>(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
	}
	
	void ConfigureShaders() {
		planetShader.inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
		planetShader.depthStencilState.depthWriteEnable = VK_FALSE;
		planetShader.depthStencilState.depthTestEnable = VK_FALSE;
		planetShader.SetData(14);
	}
	
	void LoadScene() {
		planets[0].radius = 100;
	}
	
	void ReadShaders() {
		planetShader.ReadShaders();
	}
	
	void CreateResources(Renderer* renderer, Device* renderingDevice, float screenWidth, float screenHeight) {
		
	}
	
	void DestroyResources(Device* renderingDevice) {
		
	}
	
	void AllocateBuffers(Renderer* renderer, Device* renderingDevice, Queue& transferQueue) {
		renderer->AllocateBufferStaged(transferQueue, planetsBuffer);
	}
	
	void FreeBuffers(Renderer* renderer, Device* renderingDevice) {
		planetsBuffer.Free(renderingDevice);
	}
	
	void CreatePipelines(Renderer* renderer, Device* renderingDevice, std::vector<RasterShaderPipeline*>& opaqueLightingShaders) {
		planetPipelineLayout.Create(renderingDevice);
		opaqueLightingShaders.push_back(&planetShader);
	}
	
	void DestroyPipelines(Renderer* renderer, Device* renderingDevice) {
		planetShader.DestroyPipeline(renderingDevice);
		planetPipelineLayout.Destroy(renderingDevice);
	}
	
	void RecordGraphicsCommandBuffer(VkCommandBuffer commandBuffer, int imageIndex) {
		
	}
	
	void RunInOpaqueLightingPass(Device* renderingDevice, VkCommandBuffer commandBuffer) {
		// For each planet
		planetChunkPushConstant.planetIndex = 0;
		planetChunkPushConstant.planetPosition = glm::dvec3(0, 100, -50);
		
		// For each chunk
		planetChunkPushConstant.chunkIndex = 0;
		planetChunkPushConstant.chunkSize = 200;
		planetChunkPushConstant.chunkPosOnPlanet = glm::dvec3(0);
		planetShader.Execute(renderingDevice, commandBuffer, &planetChunkPushConstant);
	}
	
	void RunLowPriorityGraphics(Device* renderingDevice, VkCommandBuffer commandBuffer) {
		
	}
	
	void FrameUpdate(Renderer* renderer, Device* renderingDevice, Camera& mainCamera) {
		
	}
	
	void LowPriorityFrameUpdate(Renderer* renderer, Device* renderingDevice, Camera& mainCamera, Queue& lowPriorityGraphicsQueue) {
		
	}
	
	
	
	
};
