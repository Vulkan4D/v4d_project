#define _V4D_MODULE
#include <v4d.h>

using namespace v4d::graphics;
using namespace v4d::scene;

Scene* scene = nullptr;

void CreateCornellBox(ObjectInstance* obj) {
	obj->rigidbodyType = ObjectInstance::RigidBodyType::STATIC;

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

V4D_MODULE_CLASS(V4D_Game) {
	
	V4D_MODULE_FUNC(void, Init, Scene* _s) {
		scene = _s;
		v4d::scene::Geometry::globalBuffers.objectBuffer.Extend(10000);
		v4d::scene::Geometry::globalBuffers.geometryBuffer.Extend(10000);
		v4d::scene::Geometry::globalBuffers.lightBuffer.Extend(1024);
	}
	
	V4D_MODULE_FUNC(void, LoadScene) {
		scene->Lock();
		
		// // Cornell boxes
		// scene->AddObjectInstance()->Configure(CreateCornellBox, {0,250,-30}, 180.0);
		// scene->AddObjectInstance()->Configure(CreateCornellBox, {200,250,-30}, 120.0);
		// scene->AddObjectInstance()->Configure(CreateCornellBox, {-200,250,-30}, -120.0);
		// for (int i = 0; i < 100; ++i)
		// 	scene->AddObjectInstance()->Configure(CreateCornellBox, {0,500,-30 + (i*90)}, 180.0);
			
		// Ground (static)
		scene->AddObjectInstance()->Configure([](ObjectInstance* obj){
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
		
		// Lights (static)
		scene->AddObjectInstance()->Configure([](ObjectInstance* obj){
			obj->rigidbodyType = ObjectInstance::RigidBodyType::STATIC;
			obj->mass = 10;
			obj->SetSphereLightSource("light", 20, 100000000);
		}, {10,-2000,10});
		// scene->AddObjectInstance()->Configure([](ObjectInstance* obj){
		// 	obj->rigidbodyType = ObjectInstance::RigidBodyType::STATIC;
		// 	obj->mass = 10;
		// 	obj->SetSphereLightSource("light", 200, 1000000000);
		// }, {10,-500,1000});
		// scene->AddObjectInstance()->Configure([](ObjectInstance* obj){
		// 	obj->rigidbodyType = ObjectInstance::RigidBodyType::STATIC;
		// 	obj->mass = 10;
		// 	obj->SetSphereLightSource("light", 2, 10000);
		// }, {10,270,10});
		// scene->AddObjectInstance()->Configure([](ObjectInstance* obj){
		// 	obj->rigidbodyType = ObjectInstance::RigidBodyType::STATIC;
		// 	obj->mass = 10;
		// 	obj->SetSphereLightSource("light", 1, 10000);
		// }, {210,270,-20});
		// scene->AddObjectInstance()->Configure([](ObjectInstance* obj){
		// 	obj->rigidbodyType = ObjectInstance::RigidBodyType::STATIC;
		// 	obj->mass = 10;
		// 	obj->SetSphereLightSource("light", 1, 10000);
		// }, {-190,270,-15});
		
		// // Red Sphere (static)
		// scene->AddObjectInstance()->Configure([](ObjectInstance* obj){
		// 	obj->rigidbodyType = ObjectInstance::RigidBodyType::STATIC;
		// 	obj->mass = 10;
		// 	obj->SetSphereGeometry("sphere", 50, {1,0,0, 1});
		// }, {60,300,500});
		
		scene->Unlock();
	}
	
	V4D_MODULE_FUNC(void, UnloadScene) {
		for (auto obj : scene->objectInstances) {
			scene->RemoveObjectInstance(obj);
		}
		scene->objectInstances.clear();
		scene->ClenupObjectInstancesGeometries();
	}
	
	V4D_MODULE_FUNC(void, RendererRunUiDebug) {
		#ifdef _ENABLE_IMGUI
			ImGui::Text("%d objects", Geometry::globalBuffers.nbAllocatedObjects);
		#endif
	}
	
};
