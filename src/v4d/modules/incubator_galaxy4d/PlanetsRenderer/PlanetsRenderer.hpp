#pragma once
#include <v4d.h>

using namespace v4d::graphics;
using namespace v4d::graphics::vulkan;

#include "PlanetaryTerrain.hpp"
#include "PlanetShaderPipeline.hpp"

class PlanetsRenderer : public v4d::modules::Rendering {
	
	#pragma region Graphics
	PipelineLayout planetPipelineLayout;
	PlanetShaderPipeline planetShader {planetPipelineLayout, {
		"modules/incubator_galaxy4d/assets/shaders/planetaryTerrain.vert",
		"modules/incubator_galaxy4d/assets/shaders/planetaryTerrain.wireframe.geom",
		"modules/incubator_galaxy4d/assets/shaders/planetaryTerrain.surface.frag",
	}};
	#pragma endregion
	
public:

	std::vector<PlanetaryTerrain*> planetaryTerrains {};

	// // Executed when calling InitRenderer() on the main Renderer
	// void Init() override {}
	
	void InitLayouts(std::vector<DescriptorSet*>& descriptorSets) override {
		auto* planetDescriptorSet_1 = descriptorSets.emplace_back(new DescriptorSet(1));
		planetPipelineLayout.AddDescriptorSet(descriptorSets[0]);
		planetPipelineLayout.AddDescriptorSet(planetDescriptorSet_1);
		planetPipelineLayout.AddPushConstant<PlanetShaderPipeline::PlanetChunkPushConstant>(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
	}
	void ConfigureShaders(std::unordered_map<std::string, std::vector<RasterShaderPipeline*>>& shaders) override {
		shaders["opaqueRasterization"].push_back(&planetShader);
		
		planetShader.planets = &planetaryTerrains;
		
		planetShader.rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
		planetShader.inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		planetShader.depthStencilState.depthWriteEnable = VK_TRUE;
		planetShader.depthStencilState.depthTestEnable = VK_TRUE;
		planetShader.AddVertexInputBinding(sizeof(PlanetaryTerrain::Vertex), VK_VERTEX_INPUT_RATE_VERTEX, {
			{0, offsetof(PlanetaryTerrain::Vertex, PlanetaryTerrain::Vertex::pos), VK_FORMAT_R32G32B32A32_SFLOAT},
			{1, offsetof(PlanetaryTerrain::Vertex, PlanetaryTerrain::Vertex::normal), VK_FORMAT_R32G32B32A32_SFLOAT},
			{2, offsetof(PlanetaryTerrain::Vertex, PlanetaryTerrain::Vertex::uv), VK_FORMAT_R32G32_SFLOAT},
		});
	}
	
	// // Executed when calling their respective methods on the main Renderer
	// void ReadShaders() override {}
	// void LoadScene() override {}
	// void UnloadScene() override {}
	
	// // Executed when calling LoadRenderer()
	// void ScorePhysicalDeviceSelection(int& score, PhysicalDevice*) override {}
	// // after selecting rendering device and queues
	// void Info() override {}
	// void CreateResources() {}
	// void DestroyResources() override {}
	// void AllocateBuffers() override {}
	
	void FreeBuffers() override {
		for (auto* planetaryTerrain : planetaryTerrains) {
			for (auto* chunk : planetaryTerrain->chunks) {
				chunk->Remove();
			}
		}
		PlanetaryTerrain::vertexBufferPool.FreePool(renderingDevice);
		PlanetaryTerrain::indexBufferPool.FreePool(renderingDevice);
	}
	
	void CreatePipelines() override {
		planetPipelineLayout.Create(renderingDevice);
	}
	void DestroyPipelines() override {
		planetPipelineLayout.Destroy(renderingDevice);
	}
	
	// // Rendering methods potentially executed on each frame
	// void RunDynamicGraphicsTop(VkCommandBuffer) override {}
	// void RunDynamicGraphicsBottom(VkCommandBuffer) override {}
	// void RunDynamicLowPriorityCompute(VkCommandBuffer) override {}
	// void RunDynamicLowPriorityGraphics(VkCommandBuffer) override {}
	
	// Executed before each frame
	void FrameUpdate(uint imageIndex, glm::dmat4& projection, glm::dmat4& view) override {
		glm::dvec4 cameraAbsolutePosition = glm::inverse(view)[3];
		planetShader.viewMatrix = view;
		
		// Planets
		for (auto* planetaryTerrain : planetaryTerrains) {
			planetaryTerrain->cameraPos = glm::dvec3(cameraAbsolutePosition)/cameraAbsolutePosition.w - planetaryTerrain->absolutePosition;
			for (auto* chunk : planetaryTerrain->chunks) {
				chunk->BeforeRender(renderingDevice, transferQueue);
			}
		}
	}
	void LowPriorityFrameUpdate() override {
		for (auto* planetaryTerrain : planetaryTerrains) {
			for (auto* chunk : planetaryTerrain->chunks) {
				chunk->Process();
			}
		}
	}
	
};
