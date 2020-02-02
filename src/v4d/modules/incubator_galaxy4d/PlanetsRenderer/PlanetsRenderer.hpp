#pragma once
#include <v4d.h>

using namespace v4d::graphics;
using namespace v4d::graphics::vulkan;

#include "PlanetTerrain.hpp"
#include "PlanetAtmosphere.hpp"
#include "PlanetTerrainShaderPipeline.hpp"
#include "PlanetAtmosphereShaderPipeline.hpp"

class PlanetsRenderer : public v4d::modules::Rendering {
	
	#pragma region Graphics
	PipelineLayout planetTerrainPipelineLayout;
	PlanetTerrainShaderPipeline planetShader {planetTerrainPipelineLayout, {
		"modules/incubator_galaxy4d/assets/shaders/planetTerrain.vert",
		"modules/incubator_galaxy4d/assets/shaders/planetTerrain.surface.frag",
	}};
	PipelineLayout planetAtmospherePipelineLayout;
	PlanetAtmosphereShaderPipeline planetAtmosphereShader {planetAtmospherePipelineLayout, {
		"modules/incubator_galaxy4d/assets/shaders/planetAtmosphere.vert",
		"modules/incubator_galaxy4d/assets/shaders/planetAtmosphere.surface.transparent.frag",
	}};
	#pragma endregion
	
public:

	int OrderIndex() const override {return 10;}
	
	std::vector<PlanetTerrain*> planetTerrains {};

	// // Executed when calling InitRenderer() on the main Renderer
	// void Init() override {}
	
	void InitLayouts(std::vector<DescriptorSet*>& descriptorSets) override {
		auto* planetDescriptorSet_1 = descriptorSets.emplace_back(new DescriptorSet(1));
		auto* atmosphereDescriptorSet_1 = descriptorSets.emplace_back(new DescriptorSet(1));
		
		planetTerrainPipelineLayout.AddDescriptorSet(descriptorSets[0]);
		planetTerrainPipelineLayout.AddDescriptorSet(planetDescriptorSet_1);
		planetTerrainPipelineLayout.AddPushConstant<PlanetTerrainShaderPipeline::PlanetChunkPushConstant>(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
		
		planetAtmospherePipelineLayout.AddDescriptorSet(descriptorSets[0]);
		planetAtmospherePipelineLayout.AddDescriptorSet(atmosphereDescriptorSet_1);
		planetAtmospherePipelineLayout.AddPushConstant<PlanetAtmosphereShaderPipeline::PlanetAtmospherePushConstant>(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
		
		if (sizeof(PlanetTerrainShaderPipeline::PlanetChunkPushConstant) > 128) {
			INVALIDCODE("PlanetChunkPushConstant should be no more than 128 bytes. Current size is " << sizeof(PlanetTerrainShaderPipeline::PlanetChunkPushConstant))
		}
		if (sizeof(PlanetAtmosphereShaderPipeline::PlanetAtmospherePushConstant) > 128) {
			INVALIDCODE("PlanetAtmospherePushConstant should be no more than 128 bytes. Current size is " << sizeof(PlanetAtmosphereShaderPipeline::PlanetAtmospherePushConstant))
		}
	}
	
	void ConfigureShaders(std::unordered_map<std::string, std::vector<RasterShaderPipeline*>>& shaders) override {
		shaders["opaqueRasterization"].push_back(&planetShader);
		
		planetShader.planets = &planetTerrains;
		planetShader.AddVertexInputBinding(sizeof(PlanetTerrain::Vertex), VK_VERTEX_INPUT_RATE_VERTEX, PlanetTerrain::Vertex::GetInputAttributes());
		
		// planetShader.rasterizer.polygonMode = VK_POLYGON_MODE_LINE;
		// planetShader.rasterizer.lineWidth = 1;
		
		// Mesh Topology
		#ifdef PLANETARY_TERRAIN_MESH_USE_TRIANGLE_STRIPS
			planetShader.inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
			planetShader.inputAssembly.primitiveRestartEnable = VK_TRUE;
		#else
			planetShader.inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		#endif
		
		
		shaders["transparentRasterization"].push_back(&planetAtmosphereShader);
		
		planetAtmosphereShader.planets = &planetTerrains;
		planetAtmosphereShader.AddVertexInputBinding(sizeof(PlanetAtmosphere::Vertex), VK_VERTEX_INPUT_RATE_VERTEX, PlanetAtmosphere::Vertex::GetInputAttributes());
		
		planetAtmosphereShader.rasterizer.polygonMode = VK_POLYGON_MODE_LINE;
		// planetAtmosphereShader.rasterizer.lineWidth = 1;
		
		// Mesh Topology
		#ifdef PLANETARY_ATMOSPHERE_MESH_USE_TRIANGLE_STRIPS
			planetAtmosphereShader.inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
			planetAtmosphereShader.inputAssembly.primitiveRestartEnable = VK_TRUE;
		#else
			planetAtmosphereShader.inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
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
		for (auto* planetaryTerrain : planetTerrains) {
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
		for (auto* planetaryTerrain : planetTerrains) {
			if (planetaryTerrain->lightIntensity > 0) {
				// Sun(s)
				scene.lightSources["sun"] = nullptr;
				scene.lightSources.erase("sun");
			}
		}
		
		// Chunks Generator
		if (PlanetTerrain::chunkGenerator) {
			delete PlanetTerrain::chunkGenerator;
			PlanetTerrain::chunkGenerator = nullptr;
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
		for (auto* planetaryTerrain : planetTerrains) {
			for (auto* chunk : planetaryTerrain->chunks) {
				chunk->Remove();
			}
		}
		PlanetTerrain::vertexBufferPool.FreePool(renderingDevice);
		PlanetTerrain::indexBufferPool.FreePool(renderingDevice);
	}
	
	void CreatePipelines() override {
		planetTerrainPipelineLayout.Create(renderingDevice);
		planetAtmospherePipelineLayout.Create(renderingDevice);
	}
	
	void DestroyPipelines() override {
		planetTerrainPipelineLayout.Destroy(renderingDevice);
		planetAtmospherePipelineLayout.Destroy(renderingDevice);
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
		for (auto* planetaryTerrain : planetTerrains) {
			std::lock_guard lock(planetaryTerrain->chunksMutex);
			planetaryTerrain->cameraPos = glm::dvec3(scene.camera.worldPosition) - planetaryTerrain->absolutePosition;
			planetaryTerrain->cameraAltitudeAboveTerrain = glm::length(planetaryTerrain->cameraPos) - planetaryTerrain->GetHeightMap(glm::normalize(planetaryTerrain->cameraPos), 0.5);
			for (auto* chunk : planetaryTerrain->chunks) {
				chunk->BeforeRender(renderingDevice, transferQueue);
			}
		}
	}
	
	void LowPriorityFrameUpdate() override {
		for (auto* planetaryTerrain : planetTerrains) {
			std::lock_guard lock(planetaryTerrain->chunksMutex);
			for (auto* chunk : planetaryTerrain->chunks) {
				chunk->Process(renderingDevice, transferQueue);
			}
			planetaryTerrain->Optimize();
		}
		PlanetTerrain::CollectGarbage(renderingDevice);
	}
	
};
