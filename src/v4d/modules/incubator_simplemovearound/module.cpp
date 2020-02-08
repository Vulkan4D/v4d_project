// Metadata
#define _V4D_MODULE
#define THIS_MODULE_VENDOR_ID 16
#define THIS_MODULE_ID 101
#define THIS_MODULE_REV 1
#define THIS_MODULE_NAME "SimpleMoveAround"
#define THIS_MODULE_DESCRIPTION "Simple Move-Around controls"

// V4D Core Header
#include <v4d.h>

struct PlayerView {
	double camSpeed = 1000.0, mouseSensitivity = 1.0, tiltSpeed = 2.0;
	double horizontalAngle = 0;
	double verticalAngle = 0;
	glm::dvec3 worldPosition {0};
	glm::dvec3 velocity {0};
	glm::dvec3 viewUp = {0,0,1};
	glm::dvec3 viewForward {0,1,0};
	glm::dvec3 viewRight = glm::cross(viewForward, viewUp);
	glm::dvec3 viewUpTarget = viewUp;
	glm::dvec3 viewForwardTarget = viewForward;
	glm::dvec3 viewRightTarget = viewRight;
	bool useFreeFlyCam = true;
	float flyCamSmoothness = 25.0;
	glm::dmat4 freeFlyCamRotationMatrix {1};
} player;

class Input : public v4d::modules::Input {
	
public:
	PlayerView* player;
	
	Input(PlayerView* player) : player(player) {}
	
	void KeyCallback(int key, int scancode, int action, int mods) override {
		if (action != GLFW_RELEASE
			#ifdef _ENABLE_IMGUI
				&& (!ImGui::IsAnyWindowFocused() || key == GLFW_KEY_ESCAPE)
			#endif
		) {
			// LOG(scancode) //TODO build platform-specific mapping for scancode when key == -1
			switch (key) {
				
				// Quit
				case GLFW_KEY_ESCAPE:
					glfwSetWindowShouldClose(window->GetHandle(), 1);
					break;
					
				// Reload Renderer
				case GLFW_KEY_R:
					renderer->ReloadRenderer();
					break;
				
				// Increase speed
				case GLFW_KEY_PAGE_UP:
					player->camSpeed *= 10;
					LOG("Movement speed: " << player->camSpeed << " m/s")
					break;
				
				// Decrease speed
				case GLFW_KEY_PAGE_DOWN:
					player->camSpeed *= 0.1;
					LOG("Movement speed: " << player->camSpeed << " m/s")
					break;
				
			}
		}
	}
	
	void MouseButtonCallback(int button, int action, int mods) override {
		if (action == GLFW_RELEASE
			#ifdef _ENABLE_IMGUI
				&& !ImGui::IsAnyWindowFocused()
			#endif
		) {
			switch (button) {
				case GLFW_MOUSE_BUTTON_1:
					glfwSetInputMode(window->GetHandle(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);
					glfwSetCursorPos(window->GetHandle(), 0, 0);
					break;
				case GLFW_MOUSE_BUTTON_2:
					glfwSetCursorPos(window->GetHandle(), 0, 0);
					glfwSetInputMode(window->GetHandle(), GLFW_CURSOR, GLFW_CURSOR_NORMAL);
					break;
			}
		}
	}
	
	void Update(double deltaTime) override {
		
		// double deltaTime = 0.005f; // No need to calculate it... This seems to already be taken into account in GLFW ???????
		
		// Camera Movements
		double camSpeedMult = glfwGetKey(window->GetHandle(), GLFW_KEY_LEFT_SHIFT)? 4.0 : (glfwGetKey(window->GetHandle(), GLFW_KEY_LEFT_ALT)? 0.25 : 1.0);
		
		player->velocity = glm::dvec3{0};
		
		if (true
			#ifdef _ENABLE_IMGUI
				&& !ImGui::IsAnyWindowFocused()
			#endif
		) {
			if (glfwGetKey(window->GetHandle(), GLFW_KEY_W)) {
				player->velocity += +player->viewForward * player->camSpeed * camSpeedMult;
			}
			if (glfwGetKey(window->GetHandle(), GLFW_KEY_S)) {
				player->velocity += -player->viewForward * player->camSpeed * camSpeedMult;
			}
			if (glfwGetKey(window->GetHandle(), GLFW_KEY_A)) {
				player->velocity += -player->viewRight * player->camSpeed * camSpeedMult;
			}
			if (glfwGetKey(window->GetHandle(), GLFW_KEY_D)) {
				player->velocity += +player->viewRight * player->camSpeed * camSpeedMult;
			}
			if (glfwGetKey(window->GetHandle(), GLFW_KEY_SPACE)) {
				player->velocity += +player->viewUp * player->camSpeed * camSpeedMult;
			}
			if (glfwGetKey(window->GetHandle(), GLFW_KEY_LEFT_CONTROL)) {
				player->velocity += -player->viewUp * player->camSpeed * camSpeedMult;
			}
		}
		
		player->worldPosition += player->velocity * deltaTime;
		
		if (glfwGetInputMode(window->GetHandle(), GLFW_CURSOR) == GLFW_CURSOR_DISABLED
			#ifdef _ENABLE_IMGUI
				&& !ImGui::IsAnyWindowFocused()
			#endif
		) {
			double x, y;
			glfwGetCursorPos(window->GetHandle(), &x, &y);
			glfwSetCursorPos(window->GetHandle(), 0, 0);
			if (player->useFreeFlyCam) {
				if (x != 0) {
					player->freeFlyCamRotationMatrix = glm::rotate(player->freeFlyCamRotationMatrix, x * player->mouseSensitivity * deltaTime, player->viewUpTarget);
				}
				if (y != 0) {
					player->freeFlyCamRotationMatrix = glm::rotate(player->freeFlyCamRotationMatrix, y * player->mouseSensitivity * deltaTime, player->viewRightTarget);
				}
				if (glfwGetKey(window->GetHandle(), GLFW_KEY_Q)) {
					player->freeFlyCamRotationMatrix = glm::rotate(player->freeFlyCamRotationMatrix, player->tiltSpeed * deltaTime, player->viewForwardTarget);
				}
				if (glfwGetKey(window->GetHandle(), GLFW_KEY_E)) {
					player->freeFlyCamRotationMatrix = glm::rotate(player->freeFlyCamRotationMatrix, -player->tiltSpeed * deltaTime, player->viewForwardTarget);
				}
				player->viewUpTarget = glm::normalize(glm::dvec3(glm::inverse(player->freeFlyCamRotationMatrix) * glm::dvec4(0,0,1, 0)));
				player->viewForwardTarget = glm::normalize(glm::dvec3(glm::inverse(player->freeFlyCamRotationMatrix) * glm::dvec4(0,1,0, 0)));
				player->viewRightTarget = glm::cross(player->viewForwardTarget, player->viewUpTarget);
				if (player->flyCamSmoothness > 2.0) {
					player->viewUp = glm::mix(player->viewUp, player->viewUpTarget, 300.0 / player->flyCamSmoothness * deltaTime);
					player->viewForward = glm::mix(player->viewForward, player->viewForwardTarget, 300.0 / player->flyCamSmoothness * deltaTime);
					player->viewRight = glm::mix(player->viewRight, player->viewRightTarget, 300.0 / player->flyCamSmoothness * deltaTime);
				} else {
					player->viewUp = player->viewUpTarget;
					player->viewForward = player->viewForwardTarget;
					player->viewRight = player->viewRightTarget;
				}
			} else {
				if (x != 0 || y != 0) {
					player->horizontalAngle += double(x * player->mouseSensitivity * deltaTime);
					player->verticalAngle -= double(y * player->mouseSensitivity * deltaTime);
					if (player->verticalAngle < -1.5) player->verticalAngle = -1.5;
					if (player->verticalAngle > 1.5) player->verticalAngle = 1.5;
					player->viewForward = glm::normalize(glm::dvec3(
						cos(player->verticalAngle) * sin(player->horizontalAngle),
						cos(player->verticalAngle) * cos(player->horizontalAngle),
						sin(player->verticalAngle)
					));
					player->viewRight = glm::cross(player->viewForward, player->viewUp);
				}
			}
		}
		
	}
	
};

class Rendering : public v4d::modules::Rendering {
public:
	PlayerView* player;
	
	Rendering(PlayerView* player) : player(player) {}

	int OrderIndex() const override {return -1;}
	
	#ifdef _ENABLE_IMGUI
		void RunImGui() override {
			ImGui::SetNextWindowSizeConstraints({250,90},{250,90});
			ImGui::Begin("Inputs");
			float speed = (float)glm::length(player->velocity);
			if (speed < 1000) {
				ImGui::Text("Movement speed: %d m/s", (int)std::ceil(speed));
			} else {
				ImGui::Text("Movement speed: %d km/s", (int)std::ceil(speed/1000.0));
			}
			ImGui::Text("Mouse look");
			ImGui::SliderFloat("Smoothness", &player->flyCamSmoothness, 0.0f, 100.0f);
			ImGui::SetNextWindowPos({ImGui::GetWindowPos().x + ImGui::GetWindowSize().x + 5, 0});
			ImGui::End();
		}
	#endif
	
	void FrameUpdate(v4d::graphics::Scene& scene) override {
		scene.camera.worldPosition = player->worldPosition;
		scene.camera.lookDirection = player->viewForward;
		scene.camera.viewUp = player->viewUp;
		scene.camera.RefreshViewMatrix();
	}
};

V4DMODULE void V4D_ModuleCreate() {
	V4D_LOAD_SUBMODULE(Input, &player)
	V4D_LOAD_SUBMODULE(Rendering, &player)
}

V4DMODULE void V4D_ModuleDestroy() {
	V4D_UNLOAD_SUBMODULES(Input)
	V4D_UNLOAD_SUBMODULES(Rendering)
}
