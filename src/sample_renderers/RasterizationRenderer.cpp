#include "../config.hh"
#include <v4d.h>

using namespace v4d::graphics;

#include "RasterizationRenderer.hpp"
Loader vulkanLoader;

std::atomic<bool> appRunning = true;

int main() {
	
	if (!vulkanLoader()) 
		throw std::runtime_error("Failed to load Vulkan library");
	
	// Create Window and Init Vulkan
	Window* window = new Window("TEST", 1280, 720);
	window->GetRequiredVulkanInstanceExtensions(vulkanLoader.requiredInstanceExtensions);
	
	auto* renderer = new RasterizationRenderer(&vulkanLoader, "V4D Test", VK_MAKE_VERSION(1, 0, 0), window);
	renderer->InitRenderer();
	renderer->ReadShaders();
	renderer->LoadScene();
	renderer->LoadRenderer();
	
	double camSpeed = 2.0, mouseSensitivity = 1.0;
	// double horizontalAngle = -2.5;
	// double verticalAngle = -0.5;
	double horizontalAngle = 0;
	double verticalAngle = 0;
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

	// Game Loop (stuff unrelated to rendering)
	std::thread gameLoopThread([&]{
		while (appRunning) {
			//...
			SLEEP(10ms)
		}
	});
	
	// Low-Priority Rendering Loop
	std::thread lowPriorityRenderingThread([&]{
		while (appRunning) {
			std::this_thread::yield();
			renderer->RenderLowPriority();
			SLEEP(10ms)
		}
	});
	
	// Rendering Loop
	std::thread renderingThread([&]{
		// Frame timer
		v4d::Timer timer(true);
		double elapsedTime = 0;
		int nbFrames = 0;
		double fps = 0;

		while (appRunning) {
			
			// Rendering
			renderer->Render();
			
			// Frame time
			++nbFrames;
			elapsedTime = timer.GetElapsedMilliseconds();
			if (elapsedTime > 1000) {
				fps = nbFrames / elapsedTime * 1000.0;
				// FPS counter
				glfwSetWindowTitle(window->GetHandle(), (std::to_string((int)fps)+" FPS").c_str());
				nbFrames = 0;
				timer.Reset();
			}
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
	
	gameLoopThread.join();
	renderingThread.join();
	lowPriorityRenderingThread.join();
	
	renderer->UnloadRenderer();
	renderer->UnloadScene();
	
	// Close Window and delete Vulkan
	delete renderer;
	delete window;

	LOG("\n\nApplication terminated\n\n");
	
}
