// Metadata
#define _V4D_MODULE
#define THIS_MODULE_VENDOR_ID 16
#define THIS_MODULE_ID 1002
#define THIS_MODULE_REV 1
#define THIS_MODULE_NAME "test_planets_rtx"
#define THIS_MODULE_DESCRIPTION "test_planets_rtx"

// V4D Core Header
#include <v4d.h>

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

DescriptorSet *mapsGenDescriptorSet, *mapsSamplerDescriptorSet;

#define MAX_PLANETS 1

Image mantleMaps[MAX_PLANETS];
Image tectonicsMaps[MAX_PLANETS];
Image heightMaps[MAX_PLANETS];
Image volcanoesMaps[MAX_PLANETS];
Image liquidsMaps[MAX_PLANETS];

#pragma endregion

struct Planet {
	double solidRadius = 8000000;
	double atmosphereRadius = 0;
	
	#pragma region cache
	
	bool mapsGenerated = false;
	
	#pragma endregion
	
	#pragma region Maps
	
	CubeMapImage mantleMap { VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT , {VK_FORMAT_R32G32B32A32_SFLOAT}}; // platesDir, mantleHeightFactor, surfaceHeightFactor, hotSpots
	CubeMapImage tectonicsMap { VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT , {VK_FORMAT_R32G32B32A32_SFLOAT}}; // divergent, convergent, transform, density
	CubeMapImage heightMap { VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT , {VK_FORMAT_R16_SFLOAT}}; // variation from surface radius, in meters, rounded (range -32k, +32k)
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
		heightMap.Create(device, mapSize/2); // max 6283 px = 1807 MB
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
	
		mantleMapGen.SetGroupCounts(mantleMap.width, mantleMap.width, mantleMap.arrayLayers);
		mantleMapGen.Execute(device, commandBuffer, 1, &mapGenPushConstant);
		
		tectonicsMapGen.SetGroupCounts(tectonicsMap.width, tectonicsMap.width, tectonicsMap.arrayLayers);
		tectonicsMapGen.Execute(device, commandBuffer, 1, &mapGenPushConstant);
		
		heightMapGen.SetGroupCounts(heightMap.width, heightMap.width, heightMap.arrayLayers);
		heightMapGen.Execute(device, commandBuffer, 1, &mapGenPushConstant);
		
		volcanoesMapGen.SetGroupCounts(volcanoesMap.width, volcanoesMap.width, volcanoesMap.arrayLayers);
		volcanoesMapGen.Execute(device, commandBuffer, 1, &mapGenPushConstant);
		
		liquidsMapGen.SetGroupCounts(liquidsMap.width, liquidsMap.width, liquidsMap.arrayLayers);
		liquidsMapGen.Execute(device, commandBuffer, 1, &mapGenPushConstant);
		
		// Need pipeline barrier before other passes
		
		
		mapsGenerated = true;
	}
	
	#pragma endregion
	
} planet;

class Rendering : public v4d::modules::Rendering {
public:
	Rendering() {}
	int OrderIndex() const override {return 0;}
	
	// Init/Shaders
	void Init() override {
		
	}
	void InitLayouts(std::vector<DescriptorSet*>& descriptorSets, std::unordered_map<std::string, Image*>&, PipelineLayout* rayTracingPipelineLayout) override {
		
		mapsGenDescriptorSet = descriptorSets.emplace_back(new DescriptorSet(1));
		mapsSamplerDescriptorSet = descriptorSets.emplace_back(new DescriptorSet(2));
		planetsMapGen.AddDescriptorSet(descriptorSets[0]);
		planetsMapGen.AddDescriptorSet(mapsGenDescriptorSet);
		planetsMapGen.AddPushConstant<MapGenPushConstant>(VK_SHADER_STAGE_COMPUTE_BIT);
		rayTracingPipelineLayout->AddDescriptorSet(mapsSamplerDescriptorSet);
		
		mapsGenDescriptorSet->AddBinding_imageView_array(0, mantleMaps, MAX_PLANETS, VK_SHADER_STAGE_COMPUTE_BIT);
		mapsGenDescriptorSet->AddBinding_imageView_array(1, tectonicsMaps, MAX_PLANETS, VK_SHADER_STAGE_COMPUTE_BIT);
		mapsGenDescriptorSet->AddBinding_imageView_array(2, heightMaps, MAX_PLANETS, VK_SHADER_STAGE_COMPUTE_BIT);
		mapsGenDescriptorSet->AddBinding_imageView_array(3, volcanoesMaps, MAX_PLANETS, VK_SHADER_STAGE_COMPUTE_BIT);
		mapsGenDescriptorSet->AddBinding_imageView_array(4, liquidsMaps, MAX_PLANETS, VK_SHADER_STAGE_COMPUTE_BIT);
		
		mapsSamplerDescriptorSet->AddBinding_combinedImageSampler_array(0, mantleMaps, MAX_PLANETS, VK_SHADER_STAGE_INTERSECTION_BIT_NV | VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV);
		mapsSamplerDescriptorSet->AddBinding_combinedImageSampler_array(1, tectonicsMaps, MAX_PLANETS, VK_SHADER_STAGE_INTERSECTION_BIT_NV | VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV);
		mapsSamplerDescriptorSet->AddBinding_combinedImageSampler_array(2, heightMaps, MAX_PLANETS, VK_SHADER_STAGE_INTERSECTION_BIT_NV | VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV);
		mapsSamplerDescriptorSet->AddBinding_combinedImageSampler_array(3, volcanoesMaps, MAX_PLANETS, VK_SHADER_STAGE_INTERSECTION_BIT_NV | VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV);
		mapsSamplerDescriptorSet->AddBinding_combinedImageSampler_array(4, liquidsMaps, MAX_PLANETS, VK_SHADER_STAGE_INTERSECTION_BIT_NV | VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV);
		
	}
	void ConfigureShaders(std::unordered_map<std::string, std::vector<RasterShaderPipeline*>>& shaders, ShaderBindingTable* shaderBindingTable) override {
		Geometry::rayTracingShaderOffsets["planet"] = shaderBindingTable->AddHitShader("modules/test_planets_rtx/assets/shaders/planets.rchit", "", "modules/test_planets_rtx/assets/shaders/planets.rint");
		
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
		
		renderer->TransitionImageLayout(planet.mantleMap.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, 1, planet.mantleMap.arrayLayers);
		renderer->TransitionImageLayout(planet.tectonicsMap.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, 1, planet.tectonicsMap.arrayLayers);
		renderer->TransitionImageLayout(planet.heightMap.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, 1, planet.heightMap.arrayLayers);
		renderer->TransitionImageLayout(planet.volcanoesMap.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, 1, planet.volcanoesMap.arrayLayers);
		renderer->TransitionImageLayout(planet.liquidsMap.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, 1, planet.liquidsMap.arrayLayers);
		
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
		
	}
	void FreeBuffers() override {
		
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
	void LoadScene(Scene& scene) {
		
		// Light source
		scene.objectInstances.emplace_back(new ObjectInstance("light"))->Configure([](ObjectInstance* obj){
			obj->SetSphereLightSource(20, 100000000);
		}, {10,-1000,300});
		
		// Planet
		scene.objectInstances.emplace_back(new ObjectInstance("planet"))->Configure([](ObjectInstance* obj){
			obj->SetSphereGeometry(200, {1,0,0, 1}, 0/*planet index*/);
		}, {0,400,0});
		
	}

	// Frame Update
	void FrameUpdate(Scene&) override {
		
	}
	void LowPriorityFrameUpdate() override {
		
	}
	
	void RunDynamicLowPriorityCompute(VkCommandBuffer commandBuffer) override {
		if (!planet.mapsGenerated) {
			// renderer->UpdateDescriptorSet(mapsGenDescriptorSet, {0,1,2,3,4});
			// renderer->UpdateDescriptorSet(mapsSamplerDescriptorSet, {0,1,2,3,4});
			planet.GenerateMaps(renderingDevice, commandBuffer);
		}
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
