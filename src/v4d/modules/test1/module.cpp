// Metadata
#define _V4D_MODULE
#define THIS_MODULE_VENDOR_ID 16
#define THIS_MODULE_ID 1001
#define THIS_MODULE_REV 1
#define THIS_MODULE_NAME "Test1"
#define THIS_MODULE_DESCRIPTION "Test1"

// V4D Core Header
#include <v4d.h>

using namespace v4d::graphics;
using namespace v4d::graphics::vulkan::rtx;

void CreateTestBox(ObjectInstance* obj) {

	auto geom1 = obj->AddGeometry(28, 42);
	
	geom1->SetVertex(0,  /*pos*/{-5.0,-5.0, -2.0}, /*normal*/{ 0.0, 0.0, 1.0}, /*uv*/{0.0, 0.0}, /*color*/{1.0, 0.0, 0.0, 1.0});
	geom1->SetVertex(1,  /*pos*/{ 5.0,-5.0, -2.0}, /*normal*/{ 0.0, 0.0, 1.0}, /*uv*/{0.0, 0.0}, /*color*/{0.0, 1.0, 0.0, 1.0});
	geom1->SetVertex(2,  /*pos*/{ 5.0, 5.0, -2.0}, /*normal*/{ 0.0, 0.0, 1.0}, /*uv*/{0.0, 0.0}, /*color*/{0.0, 0.0, 1.0, 1.0});
	geom1->SetVertex(3,  /*pos*/{-5.0, 5.0, -2.0}, /*normal*/{ 0.0, 0.0, 1.0}, /*uv*/{0.0, 0.0}, /*color*/{0.0, 1.0, 1.0, 1.0});
	//
	geom1->SetVertex(4,  /*pos*/{-5.0,-5.0,-8.0}, /*normal*/{ 0.0, 0.0, 1.0}, /*uv*/{0.0, 0.0}, /*color*/{1.0, 0.0, 0.0, 1.0});
	geom1->SetVertex(5,  /*pos*/{ 5.0,-5.0,-8.0}, /*normal*/{ 0.0, 0.0, 1.0}, /*uv*/{0.0, 0.0}, /*color*/{0.0, 1.0, 0.0, 1.0});
	geom1->SetVertex(6,  /*pos*/{ 5.0, 5.0,-8.0}, /*normal*/{ 0.0, 0.0, 1.0}, /*uv*/{0.0, 0.0}, /*color*/{0.0, 0.0, 1.0, 1.0});
	geom1->SetVertex(7,  /*pos*/{-5.0, 5.0,-8.0}, /*normal*/{ 0.0, 0.0, 1.0}, /*uv*/{0.0, 0.0}, /*color*/{0.0, 1.0, 1.0, 1.0});
	
	// bottom white
	geom1->SetVertex(8,  /*pos*/{-80.0,-80.0,-20.0}, /*normal*/{ 0.0, 0.0, 1.0}, /*uv*/{0.0, 0.0}, /*color*/{1.0,1.0,1.0, 1.0});
	geom1->SetVertex(9,  /*pos*/{ 80.0,-80.0,-20.0}, /*normal*/{ 0.0, 0.0, 1.0}, /*uv*/{0.0, 0.0}, /*color*/{1.0,1.0,1.0, 1.0});
	geom1->SetVertex(10, /*pos*/{ 80.0, 80.0,-20.0}, /*normal*/{ 0.0, 0.0, 1.0}, /*uv*/{0.0, 0.0}, /*color*/{1.0,1.0,1.0, 1.0});
	geom1->SetVertex(11, /*pos*/{-80.0, 80.0,-20.0}, /*normal*/{ 0.0, 0.0, 1.0}, /*uv*/{0.0, 0.0}, /*color*/{1.0,1.0,1.0, 1.0});
	
	// top yellow
	geom1->SetVertex(12, /*pos*/{-80.0,-80.0, 40.0}, /*normal*/{ 0.0, 0.0,-1.0}, /*uv*/{0.0, 0.0}, /*color*/{1.0,1.0,0.0, 1.0});
	geom1->SetVertex(13, /*pos*/{ 80.0,-80.0, 40.0}, /*normal*/{ 0.0, 0.0,-1.0}, /*uv*/{0.0, 0.0}, /*color*/{1.0,1.0,0.0, 1.0});
	geom1->SetVertex(14, /*pos*/{ 80.0, 80.0, 40.0}, /*normal*/{ 0.0, 0.0,-1.0}, /*uv*/{0.0, 0.0}, /*color*/{1.0,1.0,0.0, 1.0});
	geom1->SetVertex(15, /*pos*/{-80.0, 80.0, 40.0}, /*normal*/{ 0.0, 0.0,-1.0}, /*uv*/{0.0, 0.0}, /*color*/{1.0,1.0,0.0, 1.0});
	
	// left red
	geom1->SetVertex(16, /*pos*/{ 80.0, 80.0,-20.0}, /*normal*/{-1.0, 0.0, 0.0}, /*uv*/{0.0, 0.0}, /*color*/{1.0,0.0,0.0, 0.5});
	geom1->SetVertex(17, /*pos*/{ 80.0,-80.0,-20.0}, /*normal*/{-1.0, 0.0, 0.0}, /*uv*/{0.0, 0.0}, /*color*/{1.0,0.0,0.0, 0.5});
	geom1->SetVertex(18, /*pos*/{ 80.0, 80.0, 40.0}, /*normal*/{-1.0, 0.0, 0.0}, /*uv*/{0.0, 0.0}, /*color*/{1.0,0.0,0.0, 0.5});
	geom1->SetVertex(19, /*pos*/{ 80.0,-80.0, 40.0}, /*normal*/{-1.0, 0.0, 0.0}, /*uv*/{0.0, 0.0}, /*color*/{1.0,0.0,0.0, 0.5});
	
	// back blue
	geom1->SetVertex(20, /*pos*/{ 80.0,-80.0, 40.0}, /*normal*/{ 0.0, 1.0, 0.0}, /*uv*/{0.0, 0.0}, /*color*/{0.0,0.0,1.0, 1.0});
	geom1->SetVertex(21, /*pos*/{ 80.0,-80.0,-20.0}, /*normal*/{ 0.0, 1.0, 0.0}, /*uv*/{0.0, 0.0}, /*color*/{0.0,0.0,1.0, 1.0});
	geom1->SetVertex(22, /*pos*/{-80.0,-80.0, 40.0}, /*normal*/{ 0.0, 1.0, 0.0}, /*uv*/{0.0, 0.0}, /*color*/{0.0,0.0,1.0, 1.0});
	geom1->SetVertex(23, /*pos*/{-80.0,-80.0,-20.0}, /*normal*/{ 0.0, 1.0, 0.0}, /*uv*/{0.0, 0.0}, /*color*/{0.0,0.0,1.0, 1.0});
	
	// right green
	geom1->SetVertex(24, /*pos*/{-80.0, 80.0,-20.0}, /*normal*/{ 1.0, 0.0, 0.0}, /*uv*/{0.0, 0.0}, /*color*/{0.0,1.0,0.0, 1.0});
	geom1->SetVertex(25, /*pos*/{-80.0,-80.0,-20.0}, /*normal*/{ 1.0, 0.0, 0.0}, /*uv*/{0.0, 0.0}, /*color*/{0.0,1.0,0.0, 1.0});
	geom1->SetVertex(26, /*pos*/{-80.0, 80.0, 40.0}, /*normal*/{ 1.0, 0.0, 0.0}, /*uv*/{0.0, 0.0}, /*color*/{0.0,1.0,0.0, 1.0});
	geom1->SetVertex(27, /*pos*/{-80.0,-80.0, 40.0}, /*normal*/{ 1.0, 0.0, 0.0}, /*uv*/{0.0, 0.0}, /*color*/{0.0,1.0,0.0, 1.0});
	
	geom1->SetIndices({
		0, 1, 2, 2, 3, 0,
		4, 5, 6, 6, 7, 4,
		8, 9, 10, 10, 11, 8,
		//
		13, 12, 14, 14, 12, 15,
		16, 17, 18, 18, 17, 19,
		20, 21, 22, 22, 21, 23,
		25, 24, 26, 26, 27, 25,
	});
	
}

class Rendering : public v4d::modules::Rendering {
public:
	Rendering() {}

	int OrderIndex() const override {return 0;}
	
	#ifdef _ENABLE_IMGUI
		void RunImGui() override {
			
		}
	#endif
	
	virtual void ConfigureShaders(std::unordered_map<std::string, std::vector<RasterShaderPipeline*>>& shaders, ShaderBindingTable* shaderBindingTable) override {
				
	}
	
	void LoadScene(Scene& scene) {
		
		scene.AddObjectInstance()->Configure(CreateTestBox, {0,250,-30}, 180.0);
		scene.AddObjectInstance()->Configure(CreateTestBox, {200,250,-30}, 120.0);
		scene.AddObjectInstance()->Configure(CreateTestBox, {-200,250,-30}, -120.0);
		
		scene.AddObjectInstance()->Configure([](ObjectInstance* obj){
			obj->SetSphereLightSource("light", 20, 10000000);
		}, {10,-2000,10});
		scene.AddObjectInstance()->Configure([](ObjectInstance* obj){
			obj->SetSphereLightSource("light", 200, 100000000);
		}, {10,-500,1000});
		scene.AddObjectInstance()->Configure([](ObjectInstance* obj){
			obj->SetSphereLightSource("light", 2, 10000);
		}, {10,270,10});
		scene.AddObjectInstance()->Configure([](ObjectInstance* obj){
			obj->SetSphereLightSource("light", 1, 10000);
		}, {210,270,-20});
		scene.AddObjectInstance()->Configure([](ObjectInstance* obj){
			obj->SetSphereLightSource("light", 1, 10000);
		}, {-190,270,-15});
		
		scene.AddObjectInstance()->Configure([](ObjectInstance* obj){
			obj->SetSphereGeometry("sphere", 50, {1,0,0, 1});
		}, {60,300,500});
		
	}
	
};

V4DMODULE void V4D_ModuleCreate() {
	V4D_LOAD_SUBMODULE(Rendering)
}

V4DMODULE void V4D_ModuleDestroy() {
	V4D_UNLOAD_SUBMODULES(Rendering)
}
