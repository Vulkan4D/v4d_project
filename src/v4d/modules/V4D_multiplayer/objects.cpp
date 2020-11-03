#define _V4D_MODULE
#include <v4d.h>
#include "common.hh"

using namespace v4d::scene;

V4D_MODULE_CLASS(V4D_Objects) {
	V4D_MODULE_FUNC(void, AddToScene, v4d::scene::NetworkGameObjectPtr obj, v4d::scene::Scene* scene) {
		switch (obj->type) {
			case OBJECT_TYPE::Player:{
				(obj->objectInstance = scene->AddObjectInstance())->Configure([](ObjectInstance* obj){
					auto cube = obj->AddProceduralGeometry("aabb", 1);
						cube->SetProceduralVertex(0, glm::vec3(-0.5, -0.5, -0.5), glm::vec3(0.5, 0.5, 0.5), glm::vec4(0,1,0.5,1), 0);
						// cube->rayTracingMask = GEOMETRY_ATTR_PRIMARY_VISIBLE | GEOMETRY_ATTR_COLLIDER | GEOMETRY_ATTR_REFLECTION_VISIBLE;
				});
				obj->objectInstance->rigidbodyType = ObjectInstance::RigidBodyType::NONE;
			}break;
			case OBJECT_TYPE::Ball:{
				(obj->objectInstance = scene->AddObjectInstance())->Configure([](ObjectInstance* obj){
					obj->SetSphereGeometry("sphere", 1, {0.5,0.5,0.5, 1});
				});
				obj->objectInstance->rigidbodyType = ObjectInstance::RigidBodyType::DYNAMIC;
				obj->objectInstance->mass = 1;
			}break;
			case OBJECT_TYPE::Light:{
				(obj->objectInstance = scene->AddObjectInstance())->Configure([](ObjectInstance* obj){
					obj->SetSphereLightSource("light", 2, 100000);
				});
				obj->objectInstance->rigidbodyType = ObjectInstance::RigidBodyType::DYNAMIC;
				obj->objectInstance->mass = 1;
			}break;
		}
	}
};
