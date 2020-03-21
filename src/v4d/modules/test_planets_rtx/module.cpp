// Metadata
#define _V4D_MODULE
#define THIS_MODULE_VENDOR_ID 16
#define THIS_MODULE_ID 1002
#define THIS_MODULE_REV 1
#define THIS_MODULE_NAME "test_planets_rtx"
#define THIS_MODULE_DESCRIPTION "test_planets_rtx"

// V4D Core Header
#include <v4d.h>

#include "PlanetsRenderer/PlanetTerrain.hpp"

using namespace v4d::graphics;
using namespace v4d::graphics::vulkan::rtx;

#pragma region Graphics

PipelineLayout planetsMapGen;

ComputeShaderPipeline 
	mantleMapGen{planetsMapGen, "modules/test_planets_rtx/assets/shaders/planets.mantle.map.comp"},
	tectonicsMapGen{planetsMapGen, "modules/test_planets_rtx/assets/shaders/planets.tectonics.map.comp"},
	heightMapGen{planetsMapGen, "modules/test_planets_rtx/assets/shaders/planets.height.map.comp"},
	volcanoesMapGen{planetsMapGen, "modules/test_planets_rtx/assets/shaders/planets.volcanoes.map.comp"},
	liquidsMapGen{planetsMapGen, "modules/test_planets_rtx/assets/shaders/planets.liquids.map.comp"}
;

struct MapGenPushConstant {
	int planetIndex;
} mapGenPushConstant;

DescriptorSet *mapsGenDescriptorSet, *mapsSamplerDescriptorSet, *planetsDescriptorSet;

#define MAX_PLANETS 1

Image mantleMaps[MAX_PLANETS];
Image tectonicsMaps[MAX_PLANETS];
Image heightMaps[MAX_PLANETS];
Image volcanoesMaps[MAX_PLANETS];
Image liquidsMaps[MAX_PLANETS];

struct PlanetBuffer {
	glm::dmat4 viewToPlanetPosMatrix {1};
};
StagedBuffer planetsBuffer {VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, sizeof(PlanetBuffer)*MAX_PLANETS};

#pragma endregion

struct Planet {
	double solidRadius = 8000000;
	double atmosphereRadius = 8400000;
	double heightVariation = 10000;
	
	#pragma region cache
	
	bool mapsGenerated = false;
	
	#pragma endregion
	
	#pragma region Maps
	
	CubeMapImage mantleMap { VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT , {VK_FORMAT_R32G32B32A32_SFLOAT}}; // platesDir, mantleHeightFactor, surfaceHeightFactor, hotSpots
	CubeMapImage tectonicsMap { VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT , {VK_FORMAT_R32G32B32A32_SFLOAT}}; // divergent, convergent, transform, density
	CubeMapImage heightMap { VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT , {VK_FORMAT_R32_SFLOAT}}; // variation from surface radius, in meters, rounded (range -32k, +32k)
	CubeMapImage volcanoesMap { VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT , {VK_FORMAT_R8_UNORM}}; // volcanoesMap
	CubeMapImage liquidsMap { VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT , {VK_FORMAT_R8_UNORM}}; // liquidMap
	
	// temperature k, radiation rad, moisture norm, wind m/s
	CubeMapImage weatherMapAvg { VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT , {VK_FORMAT_R32G32B32A32_SFLOAT}};
	CubeMapImage weatherMapMinimum { VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT , {VK_FORMAT_R32G32B32A32_SFLOAT}};
	CubeMapImage weatherMapMaximum { VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT , {VK_FORMAT_R32G32B32A32_SFLOAT}};
	CubeMapImage weatherMapCurrent { VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT , {VK_FORMAT_R32G32B32A32_SFLOAT}};
	
	void CreateMaps(Device* device, double scale = 1.0) {
		int mapSize = std::max(64, int(scale * std::min(8000000.0, solidRadius) / 2000.0 * 3.141592654)); // 1 km / pixel (considering maximum radius of 8000km)
		// max image width : 12566
		mantleMap.Create(device, mapSize/16); // max 785 px = 57 MB
		tectonicsMap.Create(device, mapSize/8); // max 1570 px = 226 MB
		heightMap.Create(device, mapSize/2); // max 6283 px = 3614 MB
		volcanoesMap.Create(device, mapSize/4); // max 3141 px = 57 MB
		liquidsMap.Create(device, mapSize/4); // max 3141 px = 57 MB
		int weatherMapSize = std::max(8, int(mapSize / 100)); // max 125 px = 1.5 MB x4
		weatherMapAvg.Create(device, weatherMapSize);
		weatherMapMinimum.Create(device, weatherMapSize);
		weatherMapMaximum.Create(device, weatherMapSize);
		weatherMapCurrent.Create(device, weatherMapSize);
	}
	
	void DestroyMaps(Device* device) {
		mantleMap.Destroy(device);
		tectonicsMap.Destroy(device);
		heightMap.Destroy(device);
		volcanoesMap.Destroy(device);
		liquidsMap.Destroy(device);
		weatherMapAvg.Destroy(device);
		weatherMapMinimum.Destroy(device);
		weatherMapMaximum.Destroy(device);
		weatherMapCurrent.Destroy(device);
		
		mapsGenerated = false;
	}
	
	void GenerateMaps(Device* device, VkCommandBuffer commandBuffer) {
		mapGenPushConstant.planetIndex = 0;
		
		/*First Pass*/
	
		mantleMapGen.SetGroupCounts(mantleMap.width, mantleMap.height, mantleMap.arrayLayers);
		mantleMapGen.Execute(device, commandBuffer, 1, &mapGenPushConstant);
		
		tectonicsMapGen.SetGroupCounts(tectonicsMap.width, tectonicsMap.height, tectonicsMap.arrayLayers);
		tectonicsMapGen.Execute(device, commandBuffer, 1, &mapGenPushConstant);
		
		heightMapGen.SetGroupCounts(heightMap.width, heightMap.height, heightMap.arrayLayers);
		heightMapGen.Execute(device, commandBuffer, 1, &mapGenPushConstant);
		
		volcanoesMapGen.SetGroupCounts(volcanoesMap.width, volcanoesMap.height, volcanoesMap.arrayLayers);
		volcanoesMapGen.Execute(device, commandBuffer, 1, &mapGenPushConstant);
		
		liquidsMapGen.SetGroupCounts(liquidsMap.width, liquidsMap.height, liquidsMap.arrayLayers);
		liquidsMapGen.Execute(device, commandBuffer, 1, &mapGenPushConstant);
		
		// Need pipeline barrier before other passes
		
		
		mapsGenerated = true;
	}
	
	#pragma endregion
	
} planet;

PlanetTerrain terrain {planet.atmosphereRadius, planet.solidRadius, planet.heightVariation, {0,planet.solidRadius*2,0}};

class Rendering : public v4d::modules::Rendering {
public:
	Rendering() {}
	int OrderIndex() const override {return 0;}
	
	// Init/Shaders
	void Init() override {
		
	}
	void InitLayouts(std::vector<DescriptorSet*>& descriptorSets, std::unordered_map<std::string, Image*>&, PipelineLayout* rayTracingPipelineLayout) override {
		
		mapsGenDescriptorSet = descriptorSets.emplace_back(new DescriptorSet(1));
		planetsDescriptorSet = descriptorSets.emplace_back(new DescriptorSet(2));
		mapsSamplerDescriptorSet = descriptorSets.emplace_back(new DescriptorSet(3));
		planetsMapGen.AddDescriptorSet(descriptorSets[0]);
		planetsMapGen.AddDescriptorSet(mapsGenDescriptorSet);
		planetsMapGen.AddPushConstant<MapGenPushConstant>(VK_SHADER_STAGE_COMPUTE_BIT);
		rayTracingPipelineLayout->AddDescriptorSet(mapsSamplerDescriptorSet);
		rayTracingPipelineLayout->AddDescriptorSet(planetsDescriptorSet);
		
		mapsGenDescriptorSet->AddBinding_imageView_array(0, mantleMaps, MAX_PLANETS, VK_SHADER_STAGE_COMPUTE_BIT);
		mapsGenDescriptorSet->AddBinding_imageView_array(1, tectonicsMaps, MAX_PLANETS, VK_SHADER_STAGE_COMPUTE_BIT);
		mapsGenDescriptorSet->AddBinding_imageView_array(2, heightMaps, MAX_PLANETS, VK_SHADER_STAGE_COMPUTE_BIT);
		mapsGenDescriptorSet->AddBinding_imageView_array(3, volcanoesMaps, MAX_PLANETS, VK_SHADER_STAGE_COMPUTE_BIT);
		mapsGenDescriptorSet->AddBinding_imageView_array(4, liquidsMaps, MAX_PLANETS, VK_SHADER_STAGE_COMPUTE_BIT);
		
		mapsSamplerDescriptorSet->AddBinding_combinedImageSampler_array(0, mantleMaps, MAX_PLANETS, VK_SHADER_STAGE_INTERSECTION_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR);
		mapsSamplerDescriptorSet->AddBinding_combinedImageSampler_array(1, tectonicsMaps, MAX_PLANETS, VK_SHADER_STAGE_INTERSECTION_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR);
		mapsSamplerDescriptorSet->AddBinding_combinedImageSampler_array(2, heightMaps, MAX_PLANETS, VK_SHADER_STAGE_INTERSECTION_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR);
		mapsSamplerDescriptorSet->AddBinding_combinedImageSampler_array(3, volcanoesMaps, MAX_PLANETS, VK_SHADER_STAGE_INTERSECTION_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR);
		mapsSamplerDescriptorSet->AddBinding_combinedImageSampler_array(4, liquidsMaps, MAX_PLANETS, VK_SHADER_STAGE_INTERSECTION_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR);
		
		planetsDescriptorSet->AddBinding_storageBuffer(0, &planetsBuffer.deviceLocalBuffer, VK_SHADER_STAGE_INTERSECTION_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR);
	}
	void ConfigureShaders(std::unordered_map<std::string, std::vector<RasterShaderPipeline*>>& shaders, ShaderBindingTable* shaderBindingTable) override {
		Geometry::rayTracingShaderOffsets["planet_raymarching"] = shaderBindingTable->AddHitShader("modules/test_planets_rtx/assets/shaders/planets.raymarching.rchit", "", "modules/test_planets_rtx/assets/shaders/planets.raymarching.rint");
		Geometry::rayTracingShaderOffsets["planet_terrain"] = shaderBindingTable->AddHitShader("modules/test_planets_rtx/assets/shaders/planets.terrain.rchit");
		
	}
	void ReadShaders() override {
		mantleMapGen.ReadShaders();
		tectonicsMapGen.ReadShaders();
		heightMapGen.ReadShaders();
		volcanoesMapGen.ReadShaders();
		liquidsMapGen.ReadShaders();
	}
	
	// Images / Buffers / Pipelines
	void CreateResources() override {
		
		planet.CreateMaps(renderingDevice);
		
		renderer->TransitionImageLayout(planet.mantleMap, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
		renderer->TransitionImageLayout(planet.tectonicsMap, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
		renderer->TransitionImageLayout(planet.heightMap, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
		renderer->TransitionImageLayout(planet.volcanoesMap, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
		renderer->TransitionImageLayout(planet.liquidsMap, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
		
		// Set Maps
		mantleMaps[0] = planet.mantleMap;
		tectonicsMaps[0] = planet.tectonicsMap;
		heightMaps[0] = planet.heightMap;
		volcanoesMaps[0] = planet.volcanoesMap;
		liquidsMaps[0] = planet.liquidsMap;
		
	}
	void DestroyResources() override {
		planet.DestroyMaps(renderingDevice);
	}
	void AllocateBuffers() override {
		planetsBuffer.Allocate(renderingDevice);
	}
	void FreeBuffers() override {
		planetsBuffer.Free(renderingDevice);
	}
	void CreatePipelines(std::unordered_map<std::string, Image*>&) override {
		planetsMapGen.Create(renderingDevice);
		
		mantleMapGen.CreatePipeline(renderingDevice);
		tectonicsMapGen.CreatePipeline(renderingDevice);
		heightMapGen.CreatePipeline(renderingDevice);
		volcanoesMapGen.CreatePipeline(renderingDevice);
		liquidsMapGen.CreatePipeline(renderingDevice);
	}
	void DestroyPipelines() override {
		mantleMapGen.DestroyPipeline(renderingDevice);
		tectonicsMapGen.DestroyPipeline(renderingDevice);
		heightMapGen.DestroyPipeline(renderingDevice);
		volcanoesMapGen.DestroyPipeline(renderingDevice);
		liquidsMapGen.DestroyPipeline(renderingDevice);

		planetsMapGen.Destroy(renderingDevice);
	}
	
	// Scene
	void LoadScene(Scene& scene) override {
		
		// Light source
		scene.AddObjectInstance("light")->Configure([](ObjectInstance* obj){
			obj->SetSphereLightSource(700000000, 1e24f);
		}, {10,-1.496e+11,300});
				
						// // Planet
						// scene.objectInstances.emplace_back(new ObjectInstance("planet_raymarching"))->Configure([](ObjectInstance* obj){
						// 	obj->SetSphereGeometry((float)planet.solidRadius, {1,0,0, 1}, 0/*planet index*/);
						// }, {0,planet.solidRadius*2,0});
				
		terrain.scene = &scene;
		
	}
	
	void UnloadScene(Scene&) override {
		terrain.scene = nullptr;
	}

	// Frame Update
	void FrameUpdate(Scene& scene) override {
		
						// // for each planet
						// 	int planetIndex = 0;
						// 	auto* planetBuffer = &((PlanetBuffer*)(planetsBuffer.stagingBuffer.data))[planetIndex];
						// 	planetBuffer->viewToPlanetPosMatrix = glm::inverse(scene.camera.viewMatrix * scene.objectInstances[1]->GetWorldTransform());
						// //
						// auto cmdBuffer = renderer->BeginSingleTimeCommands(*graphicsQueue);
						// planetsBuffer.Update(renderingDevice, cmdBuffer);
						// renderer->EndSingleTimeCommands(*graphicsQueue, cmdBuffer);
				
		
		
		// // Planets
		// for (auto* terrain : planetTerrains) {
			std::lock_guard lock(terrain.chunksMutex);
			
			// //TODO Planet rotation
			// static v4d::Timer time(true);
			// // terrain.rotationAngle = time.GetElapsedSeconds()/1000000000;
			// // terrain.rotationAngle = time.GetElapsedSeconds()/30;
			// terrain.rotationAngle += 0.0001;
			
			terrain.RefreshMatrix();
			
			// Camera position relative to planet
			terrain.cameraPos = glm::inverse(terrain.matrix) * glm::dvec4(scene.camera.worldPosition, 1);
			terrain.cameraAltitudeAboveTerrain = glm::length(terrain.cameraPos) - terrain.GetHeightMap(glm::normalize(terrain.cameraPos), 0.5);
			
			for (auto* chunk : terrain.chunks) {
				chunk->Process();
				chunk->BeforeRender();
			}
			
		// }
	}
	void LowPriorityFrameUpdate() override {
		// // for (auto* terrain : planetTerrains) {
			// std::lock_guard lock(terrain.chunksMutex);
			// for (auto* chunk : terrain.chunks) {
			// 	chunk->Process();
			// }
		// 	terrain.Optimize();
		// // }
		// PlanetTerrain::CollectGarbage(renderingDevice);
	}
	
	// Render frame
	void RunDynamicGraphicsTop(VkCommandBuffer commandBuffer, std::unordered_map<std::string, Image*>&) {
		
	}
	void RunDynamicLowPriorityCompute(VkCommandBuffer commandBuffer) override {
		// if (!planet.mapsGenerated) {
		// 	// renderer->UpdateDescriptorSet(mapsGenDescriptorSet, {0,1,2,3,4});
		// 	// renderer->UpdateDescriptorSet(mapsSamplerDescriptorSet, {0,1,2,3,4});
		// 	planet.GenerateMaps(renderingDevice, commandBuffer);
		// }
	}
	void RunDynamicLowPriorityGraphics(VkCommandBuffer) override {
		
	}

	#ifdef _ENABLE_IMGUI
		void RunImGui() override {
			
		}
	#endif
};

V4DMODULE void V4D_ModuleCreate() {
	V4D_LOAD_SUBMODULE(Rendering)
}

V4DMODULE void V4D_ModuleDestroy() {
	V4D_UNLOAD_SUBMODULES(Rendering)
}
