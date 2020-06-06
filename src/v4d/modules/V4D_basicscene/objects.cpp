#define _V4D_MODULE
#include <v4d.h>
#include "common.hh"

using namespace v4d::scene;

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

V4D_MODULE_CLASS(V4D_Objects) {
	V4D_MODULE_FUNC(void, BuildObject, v4d::scene::NetworkGameObjectPtr obj, v4d::scene::Scene* scene) {
		switch (obj->type) {
			case OBJECT_TYPE::Player:{
				(obj->objectInstance = scene->AddObjectInstance())->Configure([](ObjectInstance* obj){
					// obj->rigidbodyType = ObjectInstance::RigidBodyType::DYNAMIC;
					obj->mass = 10;
					auto cube = obj->AddProceduralGeometry("aabb", 1);
						cube->SetProceduralVertex(0, glm::vec3(-0.5, -0.5, -1.0), glm::vec3(0.5, 0.5, 1.0), glm::vec4(0,1,0.5,1), 100000);
						cube->rayTracingMask = GEOMETRY_ATTR_PRIMARY_VISIBLE | GEOMETRY_ATTR_COLLIDER | GEOMETRY_ATTR_REFLECTION_VISIBLE;
					obj->AddLightSource({0,1.0,0}, 100000, {0,1,0.5}, 1, 0);
				});
			}break;
			case OBJECT_TYPE::CornellBox:{
				(obj->objectInstance = scene->AddObjectInstance())->Configure(CreateCornellBox);
			}break;
			case OBJECT_TYPE::Ball:{
				(obj->objectInstance = scene->AddObjectInstance())->Configure([](ObjectInstance* obj){
					// obj->rigidbodyType = ObjectInstance::RigidBodyType::DYNAMIC;
					obj->mass = 1;
					obj->SetSphereGeometry("sphere", 1, {0.5,0.5,0.5, 1});
				});
			}break;
			case OBJECT_TYPE::Light:{
				(obj->objectInstance = scene->AddObjectInstance())->Configure([](ObjectInstance* obj){
					// obj->rigidbodyType = ObjectInstance::RigidBodyType::DYNAMIC;
					obj->mass = 1;
					obj->SetSphereLightSource("light", 2, 100000);
				});
			}break;
		}
	}
};
