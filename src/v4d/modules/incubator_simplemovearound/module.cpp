// Metadata
#define _V4D_MODULE
#define THIS_MODULE_VENDOR_ID 16
#define THIS_MODULE_ID 101
#define THIS_MODULE_REV 1
#define THIS_MODULE_NAME "SimpleMoveAround"
#define THIS_MODULE_DESCRIPTION "Simple Move-Around controls"

// V4D Core Header
#include <v4d.h>

struct Camera {
	glm::dvec3 worldPosition {0};
	glm::dvec3 viewDirection {0,1,0};
	glm::dvec3 velocity {0};
	glm::dvec3 viewUp = {0,0,1};
} camera;

class Input : public v4d::modules::Input {
	
	// Normal: 10 km/s, Shift: 1000 km/s, Alt: 10 m/s
	double camSpeed = 10000.0, mouseSensitivity = 1.0;
	double horizontalAngle = 0;
	double verticalAngle = 0;
	
public:
	Camera* camera;
	
	Input(Camera* camera) : camera(camera) {}
	
	void KeyCallback(int key, int scancode, int action, int mods) override {
		if (action != GLFW_RELEASE) {
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
				
			}
		}
	}
	
	void MouseButtonCallback(int button, int action, int mods) override {
		if (action == GLFW_RELEASE) {
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
	
	void Update() override {
		
		double deltaTime = 0.005f; // No need to calculate it... This seems to already be taken into account in GLFW ???????
		
		// Camera Movements
		double camSpeedMult = glfwGetKey(window->GetHandle(), GLFW_KEY_LEFT_SHIFT)? 100.0 : (glfwGetKey(window->GetHandle(), GLFW_KEY_LEFT_ALT)? 0.001 : 1.0);
		
		camera->velocity = glm::dvec3{0};
		
		if (glfwGetKey(window->GetHandle(), GLFW_KEY_W)) {
			camera->velocity = +camera->viewDirection * camSpeed * camSpeedMult;
		}
		if (glfwGetKey(window->GetHandle(), GLFW_KEY_S)) {
			camera->velocity = -camera->viewDirection * camSpeed * camSpeedMult;
		}
		if (glfwGetKey(window->GetHandle(), GLFW_KEY_A)) {
			camera->velocity = -glm::cross(camera->viewDirection, glm::dvec3(0,0,1)) * camSpeed * camSpeedMult;
		}
		if (glfwGetKey(window->GetHandle(), GLFW_KEY_D)) {
			camera->velocity = +glm::cross(camera->viewDirection, glm::dvec3(0,0,1)) * camSpeed * camSpeedMult;
		}
		if (glfwGetKey(window->GetHandle(), GLFW_KEY_SPACE)) {
			camera->velocity = +glm::dvec3(0,0,1) * camSpeed * camSpeedMult;
		}
		if (glfwGetKey(window->GetHandle(), GLFW_KEY_LEFT_CONTROL)) {
			camera->velocity = -glm::dvec3(0,0,1) * camSpeed * camSpeedMult;
		}
		
		camera->worldPosition += camera->velocity * deltaTime;
		
		if (glfwGetInputMode(window->GetHandle(), GLFW_CURSOR) == GLFW_CURSOR_DISABLED) {
			double x, y;
			glfwGetCursorPos(window->GetHandle(), &x, &y);
			glfwSetCursorPos(window->GetHandle(), 0, 0);
			if (x != 0 || y != 0) {
				horizontalAngle += double(x * mouseSensitivity * deltaTime);
				verticalAngle -= double(y * mouseSensitivity * deltaTime);
				if (verticalAngle < -1.5) verticalAngle = -1.5;
				if (verticalAngle > 1.5) verticalAngle = 1.5;
			}
		}
		
		camera->viewDirection = glm::normalize(glm::dvec3(
			cos(verticalAngle) * sin(horizontalAngle),
			cos(verticalAngle) * cos(horizontalAngle),
			sin(verticalAngle)
		));
		
	}
	
};

class Rendering : public v4d::modules::Rendering {
public:
	Camera* camera;
	
	Rendering(Camera* camera) : camera(camera) {}
	
	void FrameUpdate(uint imageIndex, glm::dmat4& projection, glm::dmat4& view) override {
		view = glm::lookAt(camera->worldPosition, camera->worldPosition + camera->viewDirection, camera->viewUp);
	}
};

V4DMODULE void V4D_ModuleCreate() {
	V4D_LOAD_SUBMODULE(Input, &camera)
	V4D_LOAD_SUBMODULE(Rendering, &camera)
}

V4DMODULE void V4D_ModuleDestroy() {
	V4D_UNLOAD_SUBMODULES(Input)
	V4D_UNLOAD_SUBMODULES(Rendering)
}
