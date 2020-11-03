#define _V4D_MODULE
#include <v4d.h>
#include "actions.hh"
#include "common.hh"
#include "../V4D_flycam/common.hh"

using namespace v4d::scene;

V4D_Client* clientModule = nullptr;
PlayerView* player = nullptr;

V4D_MODULE_CLASS(V4D_Input) {
	
	V4D_MODULE_FUNC(std::string, CallbackName) {return THIS_MODULE;}
	
	V4D_MODULE_FUNC(void, ModuleLoad) {
		// Load Dependencies
		clientModule = V4D_Client::LoadModule(THIS_MODULE);
		player = (PlayerView*)V4D_Game::LoadModule("V4D_flycam")->ModuleGetCustomPtr(0);
	}
	
	V4D_MODULE_FUNC(void, KeyCallback, int key, int scancode, int action, int mods) {
		if (action != GLFW_RELEASE
			#ifdef _ENABLE_IMGUI
				&& (!ImGui::IsAnyWindowFocused() || key == GLFW_KEY_ESCAPE)
			#endif
		) {
			std::lock_guard lock(player->mu);
			
			switch (key) {
				// Throw stuff
				case GLFW_KEY_B:{
					v4d::data::WriteOnlyStream stream(32);
						stream << networking::action::CUSTOM;
						stream << std::string("ball");
						stream << DVector3{player->viewForward.x, player->viewForward.y, player->viewForward.z};
					clientModule->EnqueueAction(stream);
				}break;
				case GLFW_KEY_N:{
					v4d::data::WriteOnlyStream stream(32);
						stream << networking::action::CUSTOM;
						stream << std::string("balls");
						stream << DVector3{player->viewForward.x, player->viewForward.y, player->viewForward.z};
					clientModule->EnqueueAction(stream);
				}break;
				case GLFW_KEY_L:{
					v4d::data::WriteOnlyStream stream(32);
						stream << networking::action::CUSTOM;
						stream << std::string("light");
						stream << DVector3{player->viewForward.x, player->viewForward.y, player->viewForward.z};
					clientModule->EnqueueAction(stream);
				}break;
				case GLFW_KEY_C:{
					v4d::data::WriteOnlyStream stream(8);
						stream << networking::action::CUSTOM;
						stream << std::string("clear");
					clientModule->EnqueueAction(stream);
				}break;
			}
		}
	}
	
};
