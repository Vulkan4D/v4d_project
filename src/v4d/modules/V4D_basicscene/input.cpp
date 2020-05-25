#define _V4D_MODULE
#include <v4d.h>

using namespace v4d::scene;
Scene* scene = nullptr;

extern "C" {
	
	std::string CallbackName() {return THIS_MODULE;}
	
	void Init(v4d::graphics::Window* w, v4d::graphics::Renderer* r, v4d::scene::Scene* s) {
		scene = s;
		v4d::scene::Geometry::globalBuffers.lightBuffer.Extend(2048);
	}
	
	void MouseButtonCallback(int button, int action, int mods) {
		if (action == GLFW_RELEASE
			#ifdef _ENABLE_IMGUI
				&& !ImGui::IsAnyWindowFocused()
			#endif
		) {
			switch (button) {
				case GLFW_MOUSE_BUTTON_1:
					if (v4d::scene::Scene::RayCastHit hit; scene->RayCastClosest(&hit)) {
						if (hit.obj->GetGeometries().size()) {
							LOG("RayCastClosest Hit = " << hit.obj->GetGeometries()[0].type)
							hit.obj->AddImpulse(scene->camera.lookDirection*100.0);
						}
					}
					break;
				case GLFW_MOUSE_BUTTON_3:
					if (std::vector<v4d::scene::Scene::RayCastHit> hits{}; scene->RayCastAll(&hits)) {
						LOG("RayCastAll Hits = ")
						for (auto hit : hits) if (hit.obj->GetGeometries().size()) {
							LOG("    " << hit.obj->GetGeometries()[0].type)
						}
					}
					break;
			}
		}
	}
	
	std::vector<ObjectInstancePtr> balls {};
	
	void KeyCallback(int key, int scancode, int action, int mods) {
		if (action != GLFW_RELEASE
			#ifdef _ENABLE_IMGUI
				&& (!ImGui::IsAnyWindowFocused() || key == GLFW_KEY_ESCAPE)
			#endif
		) {
			// LOG(scancode) //TODO build platform-specific mapping for scancode when key == -1
			switch (key) {
				
				case GLFW_KEY_B:
					{
						// Launch ball
						auto ball = scene->AddObjectInstance();
						ball->Configure([](ObjectInstance* obj){
							obj->rigidbodyType = ObjectInstance::RigidBodyType::DYNAMIC;
							obj->mass = 1;
							obj->SetSphereGeometry("sphere", 1, {0.5,0.5,0.5, 1});
						}, scene->camera.worldPosition + scene->camera.lookDirection * 5.0);
						ball->AddImpulse(scene->camera.lookDirection*40.0);
						balls.push_back(ball);
					}
					break;
				case GLFW_KEY_L:
					{
						// Launch light
						auto ball = scene->AddObjectInstance();
						ball->Configure([](ObjectInstance* obj){
							obj->rigidbodyType = ObjectInstance::RigidBodyType::DYNAMIC;
							obj->mass = 1;
							obj->SetSphereLightSource("light", 2, 100000);
						}, scene->camera.worldPosition + scene->camera.lookDirection * 5.0);
						ball->AddImpulse(scene->camera.lookDirection*40.0);
						balls.push_back(ball);
					}
					break;
				case GLFW_KEY_C:
					for (auto ball : balls) {
						scene->RemoveObjectInstance(ball);
					}
					balls.clear();
					break;
				
			}
		}
	}
	
}
