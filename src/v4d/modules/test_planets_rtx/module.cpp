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

struct Planet {
	double solidRadius;
	double atmosphereRadius;
	
	#pragma region Maps
	
	CubeMapImage mantleMap { VK_IMAGE_USAGE_SAMPLED_BIT , {VK_FORMAT_R8G8B8A8_SRGB}}; // platesDir, mantleHeightFactor, surfaceHeightFactor, hotSpots
	CubeMapImage tectonicsMap { VK_IMAGE_USAGE_SAMPLED_BIT , {VK_FORMAT_R8G8B8A8_SRGB}}; // divergent, convergent, transform, density
	CubeMapImage heightMap { VK_IMAGE_USAGE_SAMPLED_BIT , {VK_FORMAT_R16_SINT}}; // variation from surface radius, in meters, rounded (range -32k, +32k)
	CubeMapImage stateMap { VK_IMAGE_USAGE_SAMPLED_BIT , {VK_FORMAT_R8G8_UNORM}}; // volcanoesMap, liquidMap
	
	// temperature k, radiation rad, moisture norm, wind m/s
	CubeMapImage weatherMapAvg { VK_IMAGE_USAGE_SAMPLED_BIT , {VK_FORMAT_R32G32B32A32_SFLOAT}};
	CubeMapImage weatherMapMinimum { VK_IMAGE_USAGE_SAMPLED_BIT , {VK_FORMAT_R32G32B32A32_SFLOAT}};
	CubeMapImage weatherMapMaximum { VK_IMAGE_USAGE_SAMPLED_BIT , {VK_FORMAT_R32G32B32A32_SFLOAT}};
	CubeMapImage weatherMapCurrent { VK_IMAGE_USAGE_SAMPLED_BIT , {VK_FORMAT_R32G32B32A32_SFLOAT}};
	
	void CreateMaps(Device* device, double scale = 1.0) {
		int mapSize = std::max(64, int(scale * std::min(8000000.0, solidRadius) / 2000.0 * 3.141592654)); // 1 km / pixel (considering maximum radius of 8000km)
		mantleMap.Create(device, mapSize/16);
		tectonicsMap.Create(device, mapSize/8);
		heightMap.Create(device, mapSize/2);
		stateMap.Create(device, mapSize/4);
		int weatherMapSize = std::max(8, int(mapSize / 100));
		weatherMapAvg.Create(device, weatherMapSize);
		weatherMapMinimum.Create(device, weatherMapSize);
		weatherMapMaximum.Create(device, weatherMapSize);
		weatherMapCurrent.Create(device, weatherMapSize);
	}
	
	void DestroyMaps(Device* device) {
		mantleMap.Destroy(device);
		tectonicsMap.Destroy(device);
		heightMap.Destroy(device);
		stateMap.Destroy(device);
		weatherMapAvg.Destroy(device);
		weatherMapMinimum.Destroy(device);
		weatherMapMaximum.Destroy(device);
		weatherMapCurrent.Destroy(device);
	}
	
	#pragma endregion
	
};

class Rendering : public v4d::modules::Rendering {
public:
	Rendering() {}
	int OrderIndex() const override {return 0;}

	// Init/Shaders
	void Init() override {
		
	}
	void InitLayouts(std::vector<DescriptorSet*>&, std::unordered_map<std::string, Image*>&) override {
		
	}
	void ConfigureShaders(std::unordered_map<std::string, std::vector<RasterShaderPipeline*>>& shaders, ShaderBindingTable* shaderBindingTable) override {
		Geometry::rayTracingShaderOffsets["planet"] = shaderBindingTable->AddHitShader("modules/test_planets_rtx/assets/shaders/planets.rchit", "", "modules/test_planets_rtx/assets/shaders/planets.rint");
		
	}
	void ReadShaders() override {
		
	}
	
	// Images / Buffers
	void CreateResources() override {
		
	}
	void DestroyResources() override {
		
	}
	void AllocateBuffers() override {
		
	}
	void FreeBuffers() override {
		
	}
	
	// Scene
	void LoadScene(Scene& scene) {
		
		// Light source
		scene.objectInstances.emplace_back(new ObjectInstance("light"))->Configure([](ObjectInstance* obj){
			obj->SetSphereLightSource(20, 100000000);
		}, {10,-1000,300});
		
		// Planet
		scene.objectInstances.emplace_back(new ObjectInstance("planet"))->Configure([](ObjectInstance* obj){
			obj->SetSphereGeometry(200, {1,0,0, 1});
		}, {0,400,0});
		
	}

	// Frame Update
	void FrameUpdate(Scene&) override {
		
	}
	void LowPriorityFrameUpdate() override {
		
	}
	
	void RunDynamicLowPriorityCompute(VkCommandBuffer) override {
		
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
