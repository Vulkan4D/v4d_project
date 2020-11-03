#define _V4D_MODULE
#include <v4d.h>
#include "actions.hh"
#include "common.hh"
#include "../V4D_flycam/common.hh"

using namespace v4d::scene;
using namespace networking::actions;

v4d::graphics::Window* window = nullptr;

V4D_Game* game = nullptr;
BuildInterface* buildInterface = nullptr;
V4D_Client* clientModule = nullptr;
PlayerView* player = nullptr;

bool shiftPressed = false;

V4D_MODULE_CLASS(V4D_Input) {
	
	V4D_MODULE_FUNC(std::string, CallbackName) {return THIS_MODULE;}
	
	V4D_MODULE_FUNC(void, ModuleLoad) {
		clientModule = V4D_Client::LoadModule(THIS_MODULE);
		player = (PlayerView*)V4D_Game::LoadModule("V4D_flycam")->ModuleGetCustomPtr(0);
	}
	
	V4D_MODULE_FUNC(void, Init, v4d::graphics::Window* w, v4d::graphics::Renderer*, v4d::scene::Scene*) {
		window = w;
		game = V4D_Game::GetModule(THIS_MODULE);
		buildInterface = (BuildInterface*)game->ModuleGetCustomPtr(0);
	}
	
	V4D_MODULE_FUNC(void, KeyCallback, int key, int scancode, int action, int mods) {
		if (action != GLFW_RELEASE
			#ifdef _ENABLE_IMGUI
				&& (!ImGui::IsAnyWindowFocused() || key == GLFW_KEY_ESCAPE)
			#endif
		) {
			// LOG(scancode) //TODO build platform-specific mapping for scancode when key == -1
			switch (key) {
				
				case GLFW_KEY_TAB:
					buildInterface->selectedEditValue++;
					if (buildInterface->selectedEditValue > 3) buildInterface->selectedEditValue = 0;
					break;
					
				case GLFW_KEY_0:
					buildInterface->selectedBlockType = -1;
					break;
				case GLFW_KEY_1:
					buildInterface->selectedBlockType = 0;
					break;
				case GLFW_KEY_2:
					buildInterface->selectedBlockType = 1;
					break;
				case GLFW_KEY_3:
					buildInterface->selectedBlockType = 2;
					break;
				case GLFW_KEY_4:
					buildInterface->selectedBlockType = 3;
					break;
				case GLFW_KEY_5:
					buildInterface->selectedBlockType = 4;
					break;
					
			}
			buildInterface->RemakeTmpBlock();
		}
		
		if (key == GLFW_KEY_LEFT_SHIFT) {
			if (action == GLFW_PRESS) shiftPressed = true;
			else shiftPressed = false;
		}
		
		player->canChangeVelocity = buildInterface->selectedBlockType == -1;
	}
	
	V4D_MODULE_FUNC(void, MouseButtonCallback, int button, int action, int mods) {
		if (action == GLFW_RELEASE
			#ifdef _ENABLE_IMGUI
				&& !ImGui::IsAnyWindowFocused()
			#endif
		) {
			switch (button) {
				case GLFW_MOUSE_BUTTON_1:
					// Left Click
					if (buildInterface->selectedBlockType != -1) {
						buildInterface->RemakeTmpBlock();
						if (buildInterface->tmpBlock && buildInterface->tmpBlock->block) {
							buildInterface->UpdateTmpBlock();
							NetworkGameObjectTransform transform {};
							transform.SetFromTransformAndVelocity(buildInterface->tmpBlock->GetWorldTransform(), {0,0,0});
							
							v4d::data::WriteOnlyStream stream(256);
								stream << CREATE_NEW_BUILD;
								// Network data 
								stream << transform;
								stream << *buildInterface->tmpBlock->block;
							clientModule->EnqueueAction(stream);
							
							// // deselect build tool
							// buildInterface->selectedBlockType = -1;
							// buildInterface->RemakeTmpBlock();
						}
					}
				break;
				case GLFW_MOUSE_BUTTON_2:
					// Right Click
				break;
				case GLFW_MOUSE_BUTTON_3:
					// Middle Click
				break;
			}
		}
		
		player->canChangeVelocity = buildInterface->selectedBlockType == -1;
	}
	
	V4D_MODULE_FUNC(void, ScrollCallback, double x, double y) {
		if (buildInterface->selectedBlockType != -1) {
			if (y != 0) {
				if (buildInterface->selectedEditValue < 3) {
					// Resize
					float& val = buildInterface->blockSize[buildInterface->selectedBlockType][buildInterface->selectedEditValue];
					val += shiftPressed? y : (y/10.0f);
					if (val < 0.1f) val = 0.1f;
					if (val > 102.4f) val = 102.4f;
				} else {
					// Rotate
					if (y > 0) buildInterface->NextBlockRotation();
					else buildInterface->PreviousBlockRotation();
				}
			} else if (x != 0) {
				buildInterface->selectedEditValue += x;
				if (buildInterface->selectedEditValue > 3) buildInterface->selectedEditValue = 0;
				if (buildInterface->selectedEditValue < 0) buildInterface->selectedEditValue = 3;
			}
		}
		buildInterface->RemakeTmpBlock();
	}
	
};
