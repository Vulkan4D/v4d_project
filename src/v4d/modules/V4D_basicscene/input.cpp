#define _V4D_MODULE
#include <v4d.h>

// v4d::graphics::Window* window = nullptr;
// v4d::graphics::Renderer* renderer = nullptr;
v4d::graphics::Scene* scene = nullptr;
// PlayerView player{};

extern "C" {
	
	std::string CallbackName() {return THIS_MODULE;}
	
	void Init(v4d::graphics::Window* w, v4d::graphics::Renderer* r, v4d::graphics::Scene* s) {
		// window = w;
		// renderer = r;
		scene = s;
	}
	
	void CharCallback(unsigned int c) {
		
	}
	
	void KeyCallback(int key, int scancode, int action, int mods) {
		if (action != GLFW_RELEASE
			#ifdef _ENABLE_IMGUI
				&& (!ImGui::IsAnyWindowFocused() || key == GLFW_KEY_ESCAPE)
			#endif
		) {
			// std::lock_guard lock(player.mu);
			
			// // LOG(scancode) //TODO build platform-specific mapping for scancode when key == -1
			// switch (key) {
				
			// 	// Quit
			// 	case GLFW_KEY_ESCAPE:
			// 		glfwSetWindowShouldClose(window->GetHandle(), 1);
			// 		break;
					
			// }
		}
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
