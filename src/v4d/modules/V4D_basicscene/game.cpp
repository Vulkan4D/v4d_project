#define _V4D_MODULE
#include <v4d.h>
#include "common.hh"

using namespace v4d::graphics;
using namespace v4d::scene;

ClientObjects* clientObjects = nullptr;
Scene* scene = nullptr;
PlayerView player{};

V4D_MODULE_CLASS(V4D_Game) {
	
	V4D_MODULE_FUNC(void, ModuleLoad) {
		// Load Dependencies
		clientObjects = (ClientObjects*)V4D_Client::LoadModule(THIS_MODULE)->ModuleGetCustomPtr(0);
	}
	
	V4D_MODULE_FUNC(void*, ModuleGetCustomPtr, int) {
		return &player;
	}
	
	V4D_MODULE_FUNC(int, OrderIndex) {return -1000;}
	
	V4D_MODULE_FUNC(void, Init, Scene* _s) {
		scene = _s;
		v4d::scene::Geometry::globalBuffers.objectBuffer.Extend(1000000);
		v4d::scene::Geometry::globalBuffers.geometryBuffer.Extend(1000000);
		v4d::scene::Geometry::globalBuffers.lightBuffer.Extend(4096);
	}
	
	V4D_MODULE_FUNC(void, LoadScene) {
		scene->Lock();
		
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
		scene->AddObjectInstance()->Configure([](ObjectInstance* obj){
			obj->rigidbodyType = ObjectInstance::RigidBodyType::STATIC;
			obj->mass = 10;
			obj->SetSphereLightSource("light", 200, 1000000000);
		}, {10,-500,1000});
		scene->AddObjectInstance()->Configure([](ObjectInstance* obj){
			obj->rigidbodyType = ObjectInstance::RigidBodyType::STATIC;
			obj->mass = 10;
			obj->SetSphereLightSource("light", 2, 10000);
		}, {10,270,10});
		scene->AddObjectInstance()->Configure([](ObjectInstance* obj){
			obj->rigidbodyType = ObjectInstance::RigidBodyType::STATIC;
			obj->mass = 10;
			obj->SetSphereLightSource("light", 1, 10000);
		}, {210,270,-20});
		scene->AddObjectInstance()->Configure([](ObjectInstance* obj){
			obj->rigidbodyType = ObjectInstance::RigidBodyType::STATIC;
			obj->mass = 10;
			obj->SetSphereLightSource("light", 1, 10000);
		}, {-190,270,-15});
		
		// Red Sphere (static)
		scene->AddObjectInstance()->Configure([](ObjectInstance* obj){
			obj->rigidbodyType = ObjectInstance::RigidBodyType::STATIC;
			obj->mass = 10;
			obj->SetSphereGeometry("sphere", 50, {1,0,0, 1});
		}, {60,300,500});
		
		scene->Unlock();
	}
	
	V4D_MODULE_FUNC(void, UnloadScene) {
		clientObjects->clear();
		for (auto obj : scene->objectInstances) {
			scene->RemoveObjectInstance(obj);
		}
		scene->objectInstances.clear();
		scene->ClenupObjectInstancesGeometries();
	}
	
	V4D_MODULE_FUNC(void, RendererFrameUpdate) {
		std::lock_guard lock(player.mu);
		scene->camera.MakeViewMatrix(player.worldPosition, player.viewForward, player.viewUp);
		if (scene->cameraParent) {
			scene->cameraParent->SetWorldTransform(glm::inverse(scene->camera.viewMatrix));
		}
	}
	
	V4D_MODULE_FUNC(void, RendererRunUiDebug) {
		ImGui::Text("%d objects", clientObjects->size());
	}
	
	V4D_MODULE_FUNC(void, RendererRunUi) {
		#ifdef _ENABLE_IMGUI
			std::lock_guard lock(player.mu);
			
			// ImGui::SetNextWindowSizeConstraints({380,90},{380,90});
			// ImGui::Begin("Inputs");
			ImGui::Text("Player Position %.2f, %.2f, %.2f", player.worldPosition.x, player.worldPosition.y, player.worldPosition.z);
			float speed = (float)glm::length(player.velocity);
			if (speed < 1.0) {
				ImGui::Text("Movement speed: %d mm/s", (int)std::ceil(speed*1000.0));
			} else if (speed < 1000.0) {
				ImGui::Text("Movement speed: %d m/s (%d kph, %d mph)", (int)std::ceil(speed), (int)std::round(speed*3.6), (int)std::round(speed*2.23694));
			} else if (speed < 5000.0) {
				ImGui::Text("Movement speed: %.1f km/s (%d kph, %d mph)", speed/1000.0, (int)std::round(speed*3.6), (int)std::round(speed*2.23694));
			} else {
				ImGui::Text("Movement speed: %d km/s", (int)std::round(speed/1000.0));
			}
			ImGui::Text("Mouse look");
			ImGui::SliderFloat("Smoothness", &player.flyCamSmoothness, 0.0f, 100.0f);
			// ImGui::SetNextWindowPos({ImGui::GetWindowPos().x + ImGui::GetWindowSize().x + 5, 0});
			// ImGui::End();
		#endif
	}
	
};
