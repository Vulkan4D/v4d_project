#define _V4D_MODULE
#include <v4d.h>

using namespace v4d::graphics;
using namespace v4d::scene;

void CreateCornellBox(ObjectInstance* obj) {
	obj->rigidbodyType = ObjectInstance::RigidBodyType::KINEMATIC;

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

std::vector<ObjectInstancePtr> objects {};
Scene* scene = nullptr;

std::vector<ObjectInstancePtr> balls {};

extern "C" {
	
	void ModuleLoad() {
		// Load Dependencies
		V4D_Input::LoadModule(THIS_MODULE)->ModuleSetCustomPtr(0, &balls);
		V4D_Input::LoadModule("V4D_sample");
		V4D_Game::LoadModule("V4D_sample");
	}
	
	void Init(Scene* _s) {
		scene = _s;
		v4d::scene::Geometry::globalBuffers.objectBuffer.Extend(1000000);
		v4d::scene::Geometry::globalBuffers.geometryBuffer.Extend(1000000);
		v4d::scene::Geometry::globalBuffers.lightBuffer.Extend(4096);
	}
	
	void LoadScene() {
		
		// Cornell boxes
		objects.emplace_back(scene->AddObjectInstance())->Configure(CreateCornellBox, {0,250,-30}, 180.0);
		for (int i = 0; i < 100; ++i)
			objects.emplace_back(scene->AddObjectInstance())->Configure(CreateCornellBox, {0,500,-30 + (i*90)}, 180.0);
		objects.emplace_back(scene->AddObjectInstance())->Configure(CreateCornellBox, {200,250,-30}, 120.0);
		objects.emplace_back(scene->AddObjectInstance())->Configure(CreateCornellBox, {-200,250,-30}, -120.0);
		
		// Ground
		objects.emplace_back(scene->AddObjectInstance())->Configure([](ObjectInstance* obj){
			obj->rigidbodyType = ObjectInstance::RigidBodyType::STATIC;
			auto plane = obj->AddGeometry(4, 6);
			// plane->colliderType = Geometry::ColliderType::STATIC_PLANE;
			plane->SetVertex(0, /*pos*/{-80000.0,-80000.0, 0.0}, /*normal*/{ 0.0, 0.0, 1.0}, /*uv*/{0.0, 0.0}, /*color*/{1.0,1.0,1.0, 1.0});
			plane->SetVertex(1, /*pos*/{ 80000.0,-80000.0, 0.0}, /*normal*/{ 0.0, 0.0, 1.0}, /*uv*/{0.0, 0.0}, /*color*/{1.0,1.0,1.0, 1.0});
			plane->SetVertex(2, /*pos*/{ 80000.0, 80000.0, 0.0}, /*normal*/{ 0.0, 0.0, 1.0}, /*uv*/{0.0, 0.0}, /*color*/{1.0,1.0,1.0, 1.0});
			plane->SetVertex(3, /*pos*/{-80000.0, 80000.0, 0.0}, /*normal*/{ 0.0, 0.0, 1.0}, /*uv*/{0.0, 0.0}, /*color*/{1.0,1.0,1.0, 1.0});
			plane->SetIndices({
				0, 1, 2, 2, 3, 0,
			});
		}, {0,0,-200});
		
		// Lights
		objects.emplace_back(scene->AddObjectInstance())->Configure([](ObjectInstance* obj){
			obj->rigidbodyType = ObjectInstance::RigidBodyType::DYNAMIC;
			obj->mass = 10;
			obj->SetSphereLightSource("light", 20, 10000000);
		}, {10,-2000,10});
		objects.emplace_back(scene->AddObjectInstance())->Configure([](ObjectInstance* obj){
			obj->rigidbodyType = ObjectInstance::RigidBodyType::DYNAMIC;
			obj->mass = 10;
			obj->SetSphereLightSource("light", 200, 100000000);
		}, {10,-500,1000});
		objects.emplace_back(scene->AddObjectInstance())->Configure([](ObjectInstance* obj){
			obj->rigidbodyType = ObjectInstance::RigidBodyType::DYNAMIC;
			obj->mass = 10;
			obj->SetSphereLightSource("light", 2, 10000);
		}, {10,270,10});
		objects.emplace_back(scene->AddObjectInstance())->Configure([](ObjectInstance* obj){
			obj->rigidbodyType = ObjectInstance::RigidBodyType::DYNAMIC;
			obj->mass = 10;
			obj->SetSphereLightSource("light", 1, 10000);
		}, {210,270,-20});
		objects.emplace_back(scene->AddObjectInstance())->Configure([](ObjectInstance* obj){
			obj->rigidbodyType = ObjectInstance::RigidBodyType::DYNAMIC;
			obj->mass = 10;
			obj->SetSphereLightSource("light", 1, 10000);
		}, {-190,270,-15});
		
		// Red Sphere
		objects.emplace_back(scene->AddObjectInstance())->Configure([](ObjectInstance* obj){
			obj->rigidbodyType = ObjectInstance::RigidBodyType::DYNAMIC;
			obj->mass = 10;
			obj->SetSphereGeometry("sphere", 50, {1,0,0, 1});
		}, {60,300,500});
		
	}
	
	void UnloadScene() {
		for (auto obj : objects) {
			scene->RemoveObjectInstance(obj);
		}
		objects.clear();
		scene->ClenupObjectInstancesGeometries();
	}
	
	void RendererRunUiDebug() {
		ImGui::Text("%d balls", balls.size());
	}
	
}
