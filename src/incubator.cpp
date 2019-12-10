#include "config.hh"
#include <v4d.h>

using namespace v4d::graphics;
#include "incubator_rendering/VeryBasicRenderer.hpp"

Loader vulkanLoader;

std::atomic<bool> appRunning = true;

int main() {
	
	if (!vulkanLoader()) 
		throw std::runtime_error("Failed to load Vulkan library");
		
	// Needed for RayTracing
	vulkanLoader.requiredInstanceExtensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);

	// Create Window and Init Vulkan
	Window* window = new Window("TEST", 1280, 720);
	window->GetRequiredVulkanInstanceExtensions(vulkanLoader.requiredInstanceExtensions);
	
	auto* vulkan = new VeryBasicRenderer(&vulkanLoader, "V4D Test", VK_MAKE_VERSION(1, 0, 0), window);
	
	vulkan->LoadScene();
	vulkan->LoadRenderer();
	
	// Input Events
	window->AddKeyCallback("app", [window, vulkan](int key, int scancode, int action, int mods){
		if (action != GLFW_RELEASE) {
			// LOG(scancode) //TODO build platform-specific mapping for scancode when key == -1
			switch (key) {
				
				// Quit
				case GLFW_KEY_ESCAPE:
					glfwSetWindowShouldClose(window->GetHandle(), 1);
					break;
					
				// Reload Renderer
				case GLFW_KEY_R:
					vulkan->ReloadRenderer();
					break;
					
			}
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
			vulkan->Render();
			
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
		glfwPollEvents();
		
		//...
		
		SLEEP(10ms)
	}
	
	appRunning = false;
	
	renderingThread.join();
	
	vulkan->UnloadRenderer();
	vulkan->UnloadScene();
	
	// Close Window and delete Vulkan
	delete vulkan;
	delete window;

	LOG("\n\nApplication terminated\n\n");
	
}
