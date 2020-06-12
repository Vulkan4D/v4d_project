#define _V4D_MODULE
#include <v4d.h>
#include "common.hh"

using namespace v4d::graphics;
using namespace v4d::scene;

Scene* scene = nullptr;
PlayerView player{};

V4D_MODULE_CLASS(V4D_Game) {
	
	V4D_MODULE_FUNC(void*, ModuleGetCustomPtr, int) {
		return &player;
	}
	
	V4D_MODULE_FUNC(int, OrderIndex) {return -1000;}
	
	V4D_MODULE_FUNC(void, Init, v4d::scene::Scene* s) {
		scene = s;
	}
	
	V4D_MODULE_FUNC(void, RendererFrameUpdate) {
		{std::lock_guard lock(player.mu);
			scene->camera.MakeViewMatrix(player.worldPosition, player.viewForward, player.viewUp);
		}
		if (scene->cameraParent) {
			scene->cameraParent->Lock();
				scene->cameraParent->SetWorldTransform(glm::inverse(scene->camera.viewMatrix));
			scene->cameraParent->Unlock();
		}
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
