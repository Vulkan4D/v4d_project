#define _V4D_MODULE
#include <v4d.h>

v4d::graphics::Scene* scene = nullptr;

extern "C" {
	
	std::string CallbackName() {return THIS_MODULE;}
	
	void Init(v4d::graphics::Window* w, v4d::graphics::Renderer* r, v4d::graphics::Scene* s) {
		scene = s;
	}
	
	void MouseButtonCallback(int button, int action, int mods) {
		if (action == GLFW_RELEASE
			#ifdef _ENABLE_IMGUI
				&& !ImGui::IsAnyWindowFocused()
			#endif
		) {
			switch (button) {
				case GLFW_MOUSE_BUTTON_1:
					if (v4d::graphics::Scene::RayCastHit hit; scene->RayCastClosest(&hit)) {
						if (hit.obj->GetGeometries().size()) {
							LOG("RayCastClosest Hit = " << hit.obj->GetGeometries()[0].type)
							hit.obj->AddImpulse(scene->camera.lookDirection*100.0);
						}
					}
					break;
				case GLFW_MOUSE_BUTTON_3:
					if (std::vector<v4d::graphics::Scene::RayCastHit> hits{}; scene->RayCastAll(&hits)) {
						LOG("RayCastAll Hits = ")
						for (auto hit : hits) if (hit.obj->GetGeometries().size()) {
							LOG("    " << hit.obj->GetGeometries()[0].type)
						}
					}
					break;
			}
		}
	}
	
}
