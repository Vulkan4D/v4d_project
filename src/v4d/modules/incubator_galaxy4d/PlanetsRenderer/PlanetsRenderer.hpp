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
		// "modules/incubator_galaxy4d/assets/shaders/planetaryTerrain.wireframe.geom",
		"modules/incubator_galaxy4d/assets/shaders/planetaryTerrain.surface.frag",
	}};
	#pragma endregion
	
public:

	int OrderIndex() const override {return 10;}
	
	std::vector<PlanetaryTerrain*> planetaryTerrains {};

	// // Executed when calling InitRenderer() on the main Renderer
	// void Init() override {}
	
	void InitLayouts(std::vector<DescriptorSet*>& descriptorSets) override {
		auto* planetDescriptorSet_1 = descriptorSets.emplace_back(new DescriptorSet(1));
		planetPipelineLayout.AddDescriptorSet(descriptorSets[0]);
		planetPipelineLayout.AddDescriptorSet(planetDescriptorSet_1);
		planetPipelineLayout.AddPushConstant<PlanetShaderPipeline::PlanetChunkPushConstant>(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_GEOMETRY_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
		
		if (sizeof(PlanetShaderPipeline::PlanetChunkPushConstant) > 128) {
			INVALIDCODE("PlanetChunkPushConstant should be no more than 128 bytes. Current size is " << sizeof(PlanetShaderPipeline::PlanetChunkPushConstant))
		}
	}
	
	void ConfigureShaders(std::unordered_map<std::string, std::vector<RasterShaderPipeline*>>& shaders) override {
		shaders["opaqueRasterization"].push_back(&planetShader);
		
		planetShader.planets = &planetaryTerrains;
		planetShader.AddVertexInputBinding(sizeof(PlanetaryTerrain::Vertex), VK_VERTEX_INPUT_RATE_VERTEX, PlanetaryTerrain::Vertex::GetInputAttributes());
		
		// planetShader.rasterizer.polygonMode = VK_POLYGON_MODE_LINE;
		// planetShader.rasterizer.lineWidth = 1;
		
		// Mesh Topology
		#ifdef PLANETARY_TERRAIN_MESH_USE_TRIANGLE_STRIPS
			planetShader.inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
			planetShader.inputAssembly.primitiveRestartEnable = VK_TRUE;
		#else
			planetShader.inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		#endif
	}
	
	// // Executed when calling their respective methods on the main Renderer
	// void ReadShaders() override {}
	
	LightSource sun = {
		POINT_LIGHT,
		{0, -50000000, -20000000},
		{1,1,1}, // color
		100
	};
	
	void LoadScene(Scene& scene) override {
		// Sun(s)
		scene.lightSources["sun"] = &sun;
		
		// Planets
		for (auto* planetaryTerrain : planetaryTerrains) {
			// if (planetaryTerrain->lightIntensity > 0) {
			// 	planetaryTerrain->lightSource = {
			// 		POINT_LIGHT,
			// 		planetaryTerrain->absolutePosition,
			// 		{1,1,1}, // color
			// 		planetaryTerrain->lightIntensity
			// 	};
			// 	// Sun(s)
			// 	scene.lightSources["sun"] = &planetaryTerrain->lightSource;
			// }
			planetaryTerrain->cameraPos = glm::dvec3(scene.camera.worldPosition) - planetaryTerrain->absolutePosition;
			for (auto* chunk : planetaryTerrain->chunks) {
				chunk->BeforeRender(renderingDevice, transferQueue);
			}
		}
	}
	
	void UnloadScene(Scene& scene) override {
		// Planets
		for (auto* planetaryTerrain : planetaryTerrains) {
			if (planetaryTerrain->lightIntensity > 0) {
				// Sun(s)
				scene.lightSources["sun"] = nullptr;
				scene.lightSources.erase("sun");
			}
		}
		
		// Chunks Generator
		if (PlanetaryTerrain::chunkGenerator) {
			delete PlanetaryTerrain::chunkGenerator;
			PlanetaryTerrain::chunkGenerator = nullptr;
		}
	}
	
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
	// void RunDynamicGraphicsTop(VkCommandBuffer, std::unordered_map<std::string, Image*>&) override {}
	// void RunDynamicGraphicsBottom(VkCommandBuffer, std::unordered_map<std::string, Image*>&) override {}
	// void RunDynamicLowPriorityCompute(VkCommandBuffer) override {}
	// void RunDynamicLowPriorityGraphics(VkCommandBuffer) override {}
	
	// Executed before each frame
	void FrameUpdate(Scene& scene) override {
		planetShader.viewMatrix = scene.camera.viewMatrix;
		
		// Planets
		for (auto* planetaryTerrain : planetaryTerrains) {
			std::lock_guard lock(planetaryTerrain->chunksMutex);
			planetaryTerrain->cameraPos = glm::dvec3(scene.camera.worldPosition) - planetaryTerrain->absolutePosition;
			planetaryTerrain->cameraAltitudeAboveTerrain = glm::length(planetaryTerrain->cameraPos) - planetaryTerrain->GetHeightMap(glm::normalize(planetaryTerrain->cameraPos), 0.5);
			for (auto* chunk : planetaryTerrain->chunks) {
				chunk->BeforeRender(renderingDevice, transferQueue);
			}
		}
	}
	
	void LowPriorityFrameUpdate() override {
		for (auto* planetaryTerrain : planetaryTerrains) {
			std::lock_guard lock(planetaryTerrain->chunksMutex);
			for (auto* chunk : planetaryTerrain->chunks) {
				chunk->Process(renderingDevice, transferQueue);
			}
			planetaryTerrain->Optimize();
		}
		PlanetaryTerrain::CollectGarbage(renderingDevice);
	}
	
};
