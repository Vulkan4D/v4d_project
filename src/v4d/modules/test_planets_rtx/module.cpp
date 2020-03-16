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
