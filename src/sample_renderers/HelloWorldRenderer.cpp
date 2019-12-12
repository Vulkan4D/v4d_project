#include "../config.hh"
#include <v4d.h>
#include "HelloWorldRenderer.hpp"

using namespace v4d::graphics;

Loader vulkanLoader;

std::atomic<bool> appRunning = true;

int main() {
	
	if (!vulkanLoader()) 
		throw std::runtime_error("Failed to load Vulkan library");
	
	// Create Window
	Window* window = new Window("TEST", 1280, 720);
	window->GetRequiredVulkanInstanceExtensions(vulkanLoader.requiredInstanceExtensions);
	
	auto* renderer = new HelloWorldRenderer(&vulkanLoader, "V4D Test", VK_MAKE_VERSION(1, 0, 0), window);
	
	renderer->LoadScene();
	renderer->LoadRenderer();
	
	// Input Events
	window->AddKeyCallback("app", [window, renderer](int key, int scancode, int action, int mods){
		if (action != GLFW_RELEASE) {
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
		glfwPollEvents();
		
		//...
		
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
