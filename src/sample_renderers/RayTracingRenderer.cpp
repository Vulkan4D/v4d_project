#include <v4d.h>

using namespace v4d::graphics;

// Vulkan
#include "RayTracingRenderer.hpp"
Loader vulkanLoader;

std::atomic<bool> appRunning = true;

#define CALCULATE_FRAMERATE(varRef) {\
	static v4d::Timer frameTimer(true);\
	static double elapsedTime = 0.01;\
	static int nbFrames = 0;\
	++nbFrames;\
	elapsedTime = frameTimer.GetElapsedSeconds();\
	if (elapsedTime > 1.0) {\
		varRef = nbFrames / elapsedTime;\
		nbFrames = 0;\
		frameTimer.Reset();\
	}\
}

double primaryAvgFrameRate = 0;

int main() {
	
	if (!vulkanLoader()) 
		throw std::runtime_error("Failed to load Vulkan library");
		
	// Needed for RayTracing
	vulkanLoader.requiredInstanceExtensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);

	// Create Window and Init Vulkan
	Window* window = new Window("TEST", 1280, 720);
	window->GetRequiredVulkanInstanceExtensions(vulkanLoader.requiredInstanceExtensions);
	
	auto* renderer = new RayTracingRenderer(&vulkanLoader, "V4D Test", VK_MAKE_VERSION(1, 0, 0), window);
	renderer->InitRenderer();
	renderer->ReadShaders();
	renderer->LoadScene();
	renderer->LoadRenderer();
	
	double camSpeed = 2.0, mouseSensitivity = 1.0;
	double horizontalAngle = -2.5;
	double verticalAngle = -0.5;
	renderer->camDirection = glm::dvec3(
		cos(verticalAngle) * sin(horizontalAngle),
		cos(verticalAngle) * cos(horizontalAngle),
		sin(verticalAngle)
	);
	
	// Input Events
	window->AddKeyCallback("app", [window, renderer](int key, int scancode, int action, int mods){
		
		// Might want to lock UBO in some cases
		
		// Quit application upon pressing the Escape key
		if (action != GLFW_RELEASE) {
			// LOG(scancode) //TODO build platform-specific mapping for scancode when key == -1
			switch (key) {
				
				// Quit
				case GLFW_KEY_ESCAPE:
					glfwSetWindowShouldClose(window->GetHandle(), 1);
					break;
					
				// Moving the light's position/intensity
				case GLFW_KEY_LEFT:
					renderer->light.x += 0.5f;
					break;
				case GLFW_KEY_RIGHT:
					renderer->light.x -= 0.5f;
					break;
				case GLFW_KEY_DOWN:
					renderer->light.y += 0.5f;
					break;
				case GLFW_KEY_UP:
					renderer->light.y -= 0.5f;
					break;
				case GLFW_KEY_PAGE_DOWN:
					renderer->light.z -= 0.5f;
					break;
				case GLFW_KEY_PAGE_UP:
					renderer->light.z += 0.5f;
					break;
				case GLFW_KEY_END:
					renderer->light.w -= 0.1f;
					break;
				case GLFW_KEY_HOME:
					renderer->light.w += 0.1f;
					break;
					
				// RTX Bounce Recursion
				case GLFW_KEY_1:
				case GLFW_KEY_2:
				case GLFW_KEY_3:
				case GLFW_KEY_4:
				case GLFW_KEY_5:
				case GLFW_KEY_6:
				case GLFW_KEY_7:
				case GLFW_KEY_8:
				case GLFW_KEY_9:
					renderer->rtx_reflection_max_recursion = key - 48;
					break;
				// Samples per pixel
				case GLFW_KEY_KP_ADD:
					renderer->samplesPerPixel++;
					break;
				case GLFW_KEY_KP_SUBTRACT:
					renderer->samplesPerPixel--;
					if (renderer->samplesPerPixel < 1) renderer->samplesPerPixel = 1;
					if (renderer->samplesPerPixel > 100) renderer->samplesPerPixel = 100;
					break;
				
				// RTX Shadows
				case GLFW_KEY_KP_ENTER:
					renderer->rtx_shadows = !renderer->rtx_shadows;
					break;
				
				// Reload Renderer
				case GLFW_KEY_R:
					renderer->ReloadRenderer();
					break;
					
				// Toggle Shader Test
				case GLFW_KEY_T:
					renderer->toggleTest = !renderer->toggleTest;
					LOG("ToggleTest = " << (renderer->toggleTest? "On":"Off"))
					break;
				
				// Toggle Continuous Galaxy Generation
				case GLFW_KEY_G:
					renderer->continuousGalaxyGen = !renderer->continuousGalaxyGen;
					LOG("Continuous Galaxy Generation = " << (renderer->continuousGalaxyGen? "On":"Off"))
					break;
				
			}
		}
	});
	
	// Mouse buttons
	window->AddMouseButtonCallback("app", [window, renderer](int button, int action, int mods){
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
	});

	// Rendering Loop
	std::thread renderingThread([&]{
		while (appRunning) {
			CALCULATE_FRAMERATE(primaryAvgFrameRate)
			
			// Rendering
			renderer->Render();
			
			// FPS counter
			glfwSetWindowTitle(window->GetHandle(), (std::to_string((int)primaryAvgFrameRate)+" FPS").c_str());
		}
	});
	
	while (window->IsActive()) {
		
		double deltaTime = 0.005f; // No need to calculate it... This seems to already be taken into account in GLFW ???????
		
		// Events
		glfwPollEvents();
		
		// Camera Movements
		renderer->speed = 0;
		double camSpeedMult = glfwGetKey(window->GetHandle(), GLFW_KEY_LEFT_SHIFT)? 10.0 : (glfwGetKey(window->GetHandle(), GLFW_KEY_LEFT_ALT)? 0.1 : 1.0);
		if (glfwGetKey(window->GetHandle(), GLFW_KEY_W)) {
			renderer->velocity = renderer->camDirection * camSpeed * camSpeedMult * deltaTime;
			renderer->camPosition += renderer->velocity;
			renderer->speed = 1;
		}
		if (glfwGetKey(window->GetHandle(), GLFW_KEY_S)) {
			renderer->velocity = -renderer->camDirection * camSpeed * camSpeedMult * deltaTime;
			renderer->camPosition += renderer->velocity;
			renderer->speed = 1;
		}
		if (glfwGetKey(window->GetHandle(), GLFW_KEY_A)) {
			renderer->velocity = -glm::cross(renderer->camDirection, glm::dvec3(0,0,1)) * camSpeed * camSpeedMult * deltaTime;
			renderer->camPosition += renderer->velocity;
			renderer->speed = 1;
		}
		if (glfwGetKey(window->GetHandle(), GLFW_KEY_D)) {
			renderer->velocity = +glm::cross(renderer->camDirection, glm::dvec3(0,0,1)) * camSpeed * camSpeedMult * deltaTime;
			renderer->camPosition += renderer->velocity;
			renderer->speed = 1;
		}
		if (glfwGetKey(window->GetHandle(), GLFW_KEY_SPACE)) {
			renderer->velocity = +glm::dvec3(0,0,1) * camSpeed * camSpeedMult * deltaTime;
			renderer->camPosition += renderer->velocity;
			renderer->speed = 1;
		}
		if (glfwGetKey(window->GetHandle(), GLFW_KEY_LEFT_CONTROL)) {
			renderer->velocity = -glm::dvec3(0,0,1) * camSpeed * camSpeedMult * deltaTime;
			renderer->camPosition += renderer->velocity;
			renderer->speed = 1;
		}
		if (glfwGetInputMode(window->GetHandle(), GLFW_CURSOR) == GLFW_CURSOR_DISABLED) {
			double x, y;
			glfwGetCursorPos(window->GetHandle(), &x, &y);
			glfwSetCursorPos(window->GetHandle(), 0, 0);
			if (x != 0 || y != 0) {
				horizontalAngle += double(x * mouseSensitivity * deltaTime);
				verticalAngle -= double(y * mouseSensitivity * deltaTime);
				if (verticalAngle < -1.5) verticalAngle = -1.5;
				if (verticalAngle > 1.5) verticalAngle = 1.5;
				renderer->camDirection = glm::dvec3(
					cos(verticalAngle) * sin(horizontalAngle),
					cos(verticalAngle) * cos(horizontalAngle),
					sin(verticalAngle)
				);
			}
		}
		
		SLEEP(10ms)
	}
	
	appRunning = false;
	
	renderingThread.join();
	
	renderer->UnloadRenderer();
	renderer->UnloadScene();
	
	// Close Window and delete Vulkan
	delete renderer;
	delete window;

	LOG("\n\nApplication terminated\n\n");
	
}
