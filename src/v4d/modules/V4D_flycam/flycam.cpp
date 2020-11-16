#define _V4D_MODULE
#include <v4d.h>
#include "common.hh"

using namespace v4d::graphics;
using namespace v4d::scene;

v4d::graphics::Window* window = nullptr;
Scene* scene = nullptr;
PlayerView player{};

V4D_MODULE_CLASS(V4D_Mod) {
	
	V4D_MODULE_FUNC(void*, ModuleGetCustomPtr, int) {
		return &player;
	}
	
	V4D_MODULE_FUNC(int, OrderIndex) {return -1000;}
	
	V4D_MODULE_FUNC(void, InitWindow, v4d::graphics::Window* w) {
		window = w;
	}
	
	V4D_MODULE_FUNC(void, LoadScene, v4d::scene::Scene* s) {
		scene = s;
	}
	
	V4D_MODULE_FUNC(void, BeginFrameUpdate) {
		{std::lock_guard lock(player.mu);
			scene->camera.MakeViewMatrix(player.worldPosition, player.viewForward, player.viewUp);
		}
		if (scene->cameraParent) {
			scene->cameraParent->Lock();
				scene->cameraParent->SetWorldTransform(glm::inverse(scene->camera.viewMatrix));
			scene->cameraParent->Unlock();
		}
	}
	
	V4D_MODULE_FUNC(void, DrawUi2) {
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
			ImGui::SliderFloat("Sensitivity", &player.mouseSensitivity, 1.0f, 30.0f);
			// ImGui::SetNextWindowPos({ImGui::GetWindowPos().x + ImGui::GetWindowSize().x + 5, 0});
			// ImGui::End();
		#endif
	}
	
	V4D_MODULE_FUNC(std::string, InputCallbackName) {return THIS_MODULE;}
	
	// V4D_MODULE_FUNC(void, MouseButtonCallback, int button, int action, int mods) {
	// 	if (action == GLFW_RELEASE
	// 		#ifdef _ENABLE_IMGUI
	// 			&& !ImGui::IsAnyWindowFocused()
	// 		#endif
	// 	) {
	// 		switch (button) {
	// 			case GLFW_MOUSE_BUTTON_1:
	// 				if (v4d::scene::Scene::RayCastHit hit; scene->PhysicsRayCastClosest(&hit)) {
	// 					if (hit.obj->GetGeometries().size()) {
	// 						LOG("PhysicsRayCastClosest Hit = " << hit.obj->GetGeometries()[0].type)
	// 						hit.obj->AddImpulse(scene->camera.lookDirection*100.0);
	// 					}
	// 				}
	// 				break;
	// 			case GLFW_MOUSE_BUTTON_3:
	// 				if (std::vector<v4d::scene::Scene::RayCastHit> hits{}; scene->PhysicsRayCastAll(&hits)) {
	// 					LOG("PhysicsRayCastAll Hits = ")
	// 					for (auto hit : hits) if (hit.obj->GetGeometries().size()) {
	// 						LOG("    " << hit.obj->GetGeometries()[0].type)
	// 					}
	// 				}
	// 				break;
	// 		}
	// 	}
	// }
	
	V4D_MODULE_FUNC(void, InputKeyCallback, int key, int scancode, int action, int mods) {
		if (action != GLFW_RELEASE
			#ifdef _ENABLE_IMGUI
				&& (!ImGui::IsAnyWindowFocused() || key == GLFW_KEY_ESCAPE)
			#endif
		) {
			std::lock_guard lock(player.mu);
			
			switch (key) {
				
				// Increase speed
				case GLFW_KEY_PAGE_UP:
					player.camSpeed *= 10;
					break;
				
				// Decrease speed
				case GLFW_KEY_PAGE_DOWN:
					player.camSpeed *= 0.1;
					break;
				
			}
		}
	}
	
	V4D_MODULE_FUNC(void, InputScrollCallback, double x, double y) {
		if (true
			#ifdef _ENABLE_IMGUI
				&& !ImGui::IsAnyWindowFocused()
			#endif
			&& player.canChangeVelocity
		) {
			std::lock_guard lock(player.mu);
			player.camSpeed *= (1.0+y/10);
		}
	}
	
	V4D_MODULE_FUNC(void, InputUpdate, double deltaTime) {
		
		// double deltaTime = 0.005f; // No need to calculate it... This seems to already be taken into account in GLFW ???????
		
		// Camera Movements
		double camSpeedMult = glfwGetKey(window->GetHandle(), GLFW_KEY_LEFT_SHIFT)? 4.0 : (glfwGetKey(window->GetHandle(), GLFW_KEY_LEFT_ALT)? 0.25 : 1.0);
		
		std::lock_guard lock(player.mu);
		
		player.velocity = glm::dvec3{0};
		
		if (true
			#ifdef _ENABLE_IMGUI
				&& !ImGui::IsAnyWindowFocused()
			#endif
		) {
			if (glfwGetKey(window->GetHandle(), GLFW_KEY_W)) {
				player.velocity += +player.viewForward * player.camSpeed * camSpeedMult;
			}
			if (glfwGetKey(window->GetHandle(), GLFW_KEY_S)) {
				player.velocity += -player.viewForward * player.camSpeed * camSpeedMult;
			}
			if (glfwGetKey(window->GetHandle(), GLFW_KEY_A)) {
				player.velocity += -player.viewRight * player.camSpeed * camSpeedMult;
			}
			if (glfwGetKey(window->GetHandle(), GLFW_KEY_D)) {
				player.velocity += +player.viewRight * player.camSpeed * camSpeedMult;
			}
			if (glfwGetKey(window->GetHandle(), GLFW_KEY_SPACE)) {
				player.velocity += +player.viewUp * player.camSpeed * camSpeedMult;
			}
			if (glfwGetKey(window->GetHandle(), GLFW_KEY_LEFT_CONTROL)) {
				player.velocity += -player.viewUp * player.camSpeed * camSpeedMult;
			}
		}
		
		player.worldPosition += player.velocity * deltaTime;
		
		if (glfwGetInputMode(window->GetHandle(), GLFW_CURSOR) == GLFW_CURSOR_DISABLED
			#ifdef _ENABLE_IMGUI
				&& !ImGui::IsAnyWindowFocused()
			#endif
		) {
			double x, y;
			glfwGetCursorPos(window->GetHandle(), &x, &y);
			glfwSetCursorPos(window->GetHandle(), 0, 0);
			if (player.useFreeFlyCam) {
				if (x != 0) {
					player.freeFlyCamRotationMatrix = glm::rotate(player.freeFlyCamRotationMatrix, x * player.mouseSensitivity/27.0 * deltaTime, player.viewUpTarget);
				}
				if (y != 0) {
					player.freeFlyCamRotationMatrix = glm::rotate(player.freeFlyCamRotationMatrix, y * player.mouseSensitivity/27.0 * deltaTime, player.viewRightTarget);
				}
				if (glfwGetKey(window->GetHandle(), GLFW_KEY_Q)) {
					player.freeFlyCamRotationMatrix = glm::rotate(player.freeFlyCamRotationMatrix, player.tiltSpeed * deltaTime, player.viewForwardTarget);
				}
				if (glfwGetKey(window->GetHandle(), GLFW_KEY_E)) {
					player.freeFlyCamRotationMatrix = glm::rotate(player.freeFlyCamRotationMatrix, -player.tiltSpeed * deltaTime, player.viewForwardTarget);
				}
				player.RefreshViewTarget();
				if (player.flyCamSmoothness > 2.0) {
					player.viewUp = glm::mix(player.viewUp, player.viewUpTarget, 300.0 / player.flyCamSmoothness * deltaTime);
					player.viewForward = glm::mix(player.viewForward, player.viewForwardTarget, 300.0 / player.flyCamSmoothness * deltaTime);
					player.viewRight = glm::mix(player.viewRight, player.viewRightTarget, 300.0 / player.flyCamSmoothness * deltaTime);
				} else {
					player.viewUp = player.viewUpTarget;
					player.viewForward = player.viewForwardTarget;
					player.viewRight = player.viewRightTarget;
				}
			} else {
				if (x != 0 || y != 0) {
					player.horizontalAngle += double(x * player.mouseSensitivity/27.0 * deltaTime);
					player.verticalAngle -= double(y * player.mouseSensitivity/27.0 * deltaTime);
					if (player.verticalAngle < -1.5) player.verticalAngle = -1.5;
					if (player.verticalAngle > 1.5) player.verticalAngle = 1.5;
					player.viewForward = glm::normalize(glm::dvec3(
						cos(player.verticalAngle) * sin(player.horizontalAngle),
						cos(player.verticalAngle) * cos(player.horizontalAngle),
						sin(player.verticalAngle)
					));
					player.viewRight = glm::cross(player.viewForward, player.viewUp);
				}
			}
		}
		
	}
	
};
