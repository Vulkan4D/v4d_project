#define _V4D_MODULE
#include <v4d.h>

v4d::graphics::Renderer* renderer = nullptr;

V4D_MODULE_CLASS(V4D_Input) {
	
	V4D_MODULE_FUNC(std::string, CallbackName) {return THIS_MODULE;}
	
	V4D_MODULE_FUNC(void, Init, v4d::graphics::Window*, v4d::graphics::Renderer* r, v4d::scene::Scene*) {
		renderer = r;
	}
	
	V4D_MODULE_FUNC(void, KeyCallback, int key, int scancode, int action, int mods) {
		if (action != GLFW_RELEASE
			#ifdef _ENABLE_IMGUI
				&& !ImGui::IsAnyWindowFocused()
			#endif
		) {
			switch (key) {
				
				// Reload Renderer
				case GLFW_KEY_R:
					renderer->ReloadRenderer();
					break;
				
			}
		}
	}
	
};
