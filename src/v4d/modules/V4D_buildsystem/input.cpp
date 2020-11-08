#define _V4D_MODULE
#include <v4d.h>
#include "actions.hh"
#include "common.hh"
#include "../V4D_flycam/common.hh"

using namespace v4d::scene;
using namespace networking::actions;

v4d::graphics::Window* window = nullptr;

v4d::scene::Scene* scene = nullptr;
V4D_Game* game = nullptr;
BuildInterface* buildInterface = nullptr;
V4D_Client* clientModule = nullptr;
PlayerView* player = nullptr;

V4D_MODULE_CLASS(V4D_Input) {
	
	V4D_MODULE_FUNC(std::string, CallbackName) {return THIS_MODULE;}
	
	V4D_MODULE_FUNC(void, ModuleLoad) {
		clientModule = V4D_Client::LoadModule(THIS_MODULE);
		player = (PlayerView*)V4D_Game::LoadModule("V4D_flycam")->ModuleGetCustomPtr(0);
	}
	
	V4D_MODULE_FUNC(void, Init, v4d::graphics::Window* w, v4d::graphics::Renderer*, v4d::scene::Scene* s) {
		window = w;
		scene = s;
		game = V4D_Game::GetModule(THIS_MODULE);
		buildInterface = (BuildInterface*)game->ModuleGetCustomPtr(0);
	}
	
	V4D_MODULE_FUNC(void, KeyCallback, int key, int scancode, int action, int mods) {
		std::lock_guard lock(buildInterface->mu);
		
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
					
				case GLFW_KEY_X: // delete block
					if (buildInterface->cachedHitBlock.hasHit) {
						auto hitBuild = buildInterface->cachedHitBlock.build.lock();
						if (hitBuild) {
							PackedBlockCustomData customData;
							customData.packed = buildInterface->cachedHitBlock.customData0;
							auto parentBlock = hitBuild->GetBlock(customData.blockIndex);
							if (parentBlock.has_value()) {
								v4d::data::WriteOnlyStream stream(32);
									stream << REMOVE_BLOCK_FROM_BUILD;
									// Network data 
									stream << hitBuild->networkId;
									stream << parentBlock->GetIndex();
								clientModule->EnqueueAction(stream);
							}
						}
					}
					break;
			}
			buildInterface->RemakeTmpBlock();
		}
		
		// Shift key for higher precision grid
		if (key == GLFW_KEY_LEFT_SHIFT) {
			if (action == GLFW_PRESS) buildInterface->highPrecisionGrid = true;
			else buildInterface->highPrecisionGrid = false;
		}
		
		player->canChangeVelocity = buildInterface->selectedBlockType == -1;
	}
	
	V4D_MODULE_FUNC(void, MouseButtonCallback, int button, int action, int mods) {
		std::lock_guard lock(buildInterface->mu);
		
		if (action == GLFW_RELEASE
			#ifdef _ENABLE_IMGUI
				&& !ImGui::IsAnyWindowFocused()
			#endif
		) {
			switch (button) {
				case GLFW_MOUSE_BUTTON_1:
					// Left Click
					if (buildInterface->selectedBlockType != -1 && buildInterface->isValid) {
						scene->Lock();
						if (buildInterface->tmpBlock && buildInterface->tmpBlock->block) {
							NetworkGameObjectTransform transform {};
							transform.SetFromTransformAndVelocity(buildInterface->tmpBlock->GetWorldTransform(), {0,0,0});
							auto block = *buildInterface->tmpBlock->block;
							auto parent = buildInterface->tmpBuildParent;
							scene->Unlock();
							
							block.SetColor(BLOCK_COLOR_GREY);
							
							if (parent) {
								v4d::data::WriteOnlyStream stream(256);
									stream << ADD_BLOCK_TO_BUILD;
									// Network data 
									stream << parent->networkId;
									stream << block;
								clientModule->EnqueueAction(stream);
							} else {
								v4d::data::WriteOnlyStream stream(256);
									stream << CREATE_NEW_BUILD;
									// Network data 
									stream << transform;
									stream << block;
								clientModule->EnqueueAction(stream);
							}
							
							// // deselect build tool
							// buildInterface->selectedBlockType = -1;
							// buildInterface->RemakeTmpBlock();
						} else {
							scene->Unlock();
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
		std::lock_guard lock(buildInterface->mu);
		
		if (buildInterface->selectedBlockType != -1) {
			if (y != 0) {
				if (buildInterface->selectedEditValue < 3) {
					
					// Resize
					float& val = buildInterface->blockSize[buildInterface->selectedBlockType][buildInterface->selectedEditValue];
					if (buildInterface->highPrecisionGrid) {
						val += y / 10.0f;
					} else {
						val = glm::round(val) + glm::round(y);
					}
					
					// Minimum size
					if (val < 0.1f) val = buildInterface->highPrecisionGrid? 0.1f : 1.0f;
					
					// Maximum size
					// if (val > 102.4f) val = 102.4f; // actual limit
					if (val > 100.0f) val = 100.0f; // nicer limit
					
				} else {
					// Rotate
					buildInterface->NextBlockRotation(y);
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
