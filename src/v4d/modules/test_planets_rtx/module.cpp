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
	
	#ifdef _ENABLE_IMGUI
		void RunImGui() override {
			
		}
	#endif
	
	virtual void ConfigureShaders(std::unordered_map<std::string, std::vector<RasterShaderPipeline*>>& shaders, ShaderBindingTable* shaderBindingTable) override {
		// Geometry::rayTracingShaderOffsets["planet"] = shaderBindingTable->AddHitShader("planet.rchit", "", "planet.rint");
		
	}
	
	void LoadScene(Scene& scene) {
		
		// scene.objectInstances.emplace_back(new ObjectInstance())->Configure(CreateTestBox, {0,250,-30}, 180.0);
		// scene.objectInstances.emplace_back(new ObjectInstance())->Configure(CreateTestBox, {200,250,-30}, 120.0);
		// scene.objectInstances.emplace_back(new ObjectInstance())->Configure(CreateTestBox, {-200,250,-30}, -120.0);
		
		// scene.objectInstances.emplace_back(new ObjectInstance("light"))->Configure([](ObjectInstance* obj){
		// 	obj->SetSphereLightSource(20, 10000000);
		// }, {10,-2000,10});
		// scene.objectInstances.emplace_back(new ObjectInstance("light"))->Configure([](ObjectInstance* obj){
		// 	obj->SetSphereLightSource(200, 100000000);
		// }, {10,-500,1000});
		// scene.objectInstances.emplace_back(new ObjectInstance("light"))->Configure([](ObjectInstance* obj){
		// 	obj->SetSphereLightSource(2, 10000);
		// }, {10,270,10});
		// scene.objectInstances.emplace_back(new ObjectInstance("light"))->Configure([](ObjectInstance* obj){
		// 	obj->SetSphereLightSource(1, 10000);
		// }, {210,270,-20});
		// scene.objectInstances.emplace_back(new ObjectInstance("light"))->Configure([](ObjectInstance* obj){
		// 	obj->SetSphereLightSource(1, 10000);
		// }, {-190,270,-15});
		
		// scene.objectInstances.emplace_back(new ObjectInstance("sphere"))->Configure([](ObjectInstance* obj){
		// 	obj->SetSphereGeometry(50, {1,0,0, 1});
		// }, {60,300,500});
		
	}
	
};

V4DMODULE void V4D_ModuleCreate() {
	V4D_LOAD_SUBMODULE(Rendering)
}

V4DMODULE void V4D_ModuleDestroy() {
	V4D_UNLOAD_SUBMODULES(Rendering)
}
