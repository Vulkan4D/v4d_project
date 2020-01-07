#include "config.hh"
#include <v4d.h>

using namespace v4d::graphics;

#include "incubator_rendering/V4DRenderer.hpp"
Loader vulkanLoader;

std::atomic<bool> appRunning = true;

#ifdef _WINDOWS
	//TODO find windows equivalent, and put it in v4d helper macros
	#define SET_CPU_AFFINITY(n)
#else
	#define SET_CPU_AFFINITY(n) \
		cpu_set_t cpuset;\
		CPU_ZERO(&cpuset);\
		CPU_SET(std::min((int)std::thread::hardware_concurrency()-1, n), &cpuset);\
		int rc = pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);\
		if (rc != 0) {\
			LOG_ERROR("Error calling pthread_setaffinity_np: " << rc)\
		}
#endif

int main() {
	SET_CPU_AFFINITY(0)
	
	if (!vulkanLoader()) 
		throw std::runtime_error("Failed to load Vulkan library");
	
	// Needed for RayTracing
	vulkanLoader.requiredInstanceExtensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);

	// Create Window and Init Vulkan
	Window* window = new Window("TEST", 1280, 720);
	window->GetRequiredVulkanInstanceExtensions(vulkanLoader.requiredInstanceExtensions);
	
	auto* renderer = new V4DRenderer(&vulkanLoader, "V4D Test", VK_MAKE_VERSION(1, 0, 0), window);
	renderer->preferredPresentModes = {
		VK_PRESENT_MODE_MAILBOX_KHR,	// TripleBuffering (No Tearing, low latency)
		VK_PRESENT_MODE_IMMEDIATE_KHR,	// VSync OFF (With Tearing, no latency)
		VK_PRESENT_MODE_FIFO_KHR,		// VSync ON (No Tearing, more latency)
	};
	renderer->InitRenderer();
	renderer->ReadShaders();
	renderer->LoadScene();
	renderer->LoadRenderer();
	
	double camSpeed = 2000.0, mouseSensitivity = 1.0;
	double horizontalAngle = 0;
	double verticalAngle = 0;
	renderer->mainCamera.SetWorldPosition(0, 0, 0);
	renderer->mainCamera.SetViewDirection(
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
		SET_CPU_AFFINITY(1)
		while (appRunning) {
			//...
			SLEEP(10ms)
		}
	});
	
	// Low-Priority Rendering Loop
	std::thread lowPriorityRenderingThread([&]{
		SET_CPU_AFFINITY(2)
		while (appRunning) {
			// std::this_thread::yield();
			if (!appRunning) break;
			renderer->RenderLowPriority();
			// SLEEP(10ms)
		}
	});
	
	// Rendering Loop
	std::thread renderingThread([&]{
		SET_CPU_AFFINITY(3)
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
		SET_CPU_AFFINITY(1)
		
		double deltaTime = 0.005f; // No need to calculate it... This seems to already be taken into account in GLFW ???????
		
		// Events
		glfwPollEvents();
		
		// Camera Movements
		double camSpeedMult = glfwGetKey(window->GetHandle(), GLFW_KEY_LEFT_SHIFT)? 100.0 : (glfwGetKey(window->GetHandle(), GLFW_KEY_LEFT_ALT)? 0.01 : 1.0);
		renderer->mainCamera.SetVelocity(glm::dvec3{0});
		if (glfwGetKey(window->GetHandle(), GLFW_KEY_W)) {
			renderer->mainCamera.SetVelocity(+renderer->mainCamera.GetViewDirection() * camSpeed * camSpeedMult * deltaTime);
			renderer->mainCamera.SetWorldPosition(renderer->mainCamera.GetWorldPosition() + renderer->mainCamera.GetVelocity());
		}
		if (glfwGetKey(window->GetHandle(), GLFW_KEY_S)) {
			renderer->mainCamera.SetVelocity(-renderer->mainCamera.GetViewDirection() * camSpeed * camSpeedMult * deltaTime);
			renderer->mainCamera.SetWorldPosition(renderer->mainCamera.GetWorldPosition() + renderer->mainCamera.GetVelocity());
		}
		if (glfwGetKey(window->GetHandle(), GLFW_KEY_A)) {
			renderer->mainCamera.SetVelocity(-glm::cross(renderer->mainCamera.GetViewDirection(), glm::dvec3(0,0,1)) * camSpeed * camSpeedMult * deltaTime);
			renderer->mainCamera.SetWorldPosition(renderer->mainCamera.GetWorldPosition() + renderer->mainCamera.GetVelocity());
		}
		if (glfwGetKey(window->GetHandle(), GLFW_KEY_D)) {
			renderer->mainCamera.SetVelocity(+glm::cross(renderer->mainCamera.GetViewDirection(), glm::dvec3(0,0,1)) * camSpeed * camSpeedMult * deltaTime);
			renderer->mainCamera.SetWorldPosition(renderer->mainCamera.GetWorldPosition() + renderer->mainCamera.GetVelocity());
		}
		if (glfwGetKey(window->GetHandle(), GLFW_KEY_SPACE)) {
			renderer->mainCamera.SetVelocity(+glm::dvec3(0,0,1) * camSpeed * camSpeedMult * deltaTime);
			renderer->mainCamera.SetWorldPosition(renderer->mainCamera.GetWorldPosition() + renderer->mainCamera.GetVelocity());
		}
		if (glfwGetKey(window->GetHandle(), GLFW_KEY_LEFT_CONTROL)) {
			renderer->mainCamera.SetVelocity(-glm::dvec3(0,0,1) * camSpeed * camSpeedMult * deltaTime);
			renderer->mainCamera.SetWorldPosition(renderer->mainCamera.GetWorldPosition() + renderer->mainCamera.GetVelocity());
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
				renderer->mainCamera.SetViewDirection(
					cos(verticalAngle) * sin(horizontalAngle),
					cos(verticalAngle) * cos(horizontalAngle),
					sin(verticalAngle)
				);
			}
		}
		
		#if defined(_DEBUG) && defined(_LINUX)
			// Watch shader modifications to automatically reload the renderer
			static std::unordered_map<v4d::io::FilePath, double> shaderFilesToWatch {
				// {"incubator_rendering/assets/shaders/v4d_galaxy.meta", 0},
				// {"incubator_rendering/assets/shaders/rtx_galaxies.meta", 0},
				// {"incubator_galaxy4d/assets/shaders/planetRayMarching.meta", 0},
				{"incubator_galaxy4d/assets/shaders/planetRaster.meta", 0},
			};
			for (auto&[f, t] : shaderFilesToWatch) {
				if (t == 0) {
					t = f.GetLastWriteTime();
				} else if (f.GetLastWriteTime() > t) {
					t = 0;
					renderer->ReloadRenderer();
					break;
				}
			}
		#endif
		
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
