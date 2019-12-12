#include <v4d.h>
#include "MinimalRenderer.hpp"

using namespace v4d::graphics;

Loader vulkanLoader;

int main() {
	// Load Vulkan
	if (!vulkanLoader()) 
		throw std::runtime_error("Failed to load Vulkan library");
	
	// Create Window
	Window* window = new Window("TEST", 1280, 720);
	window->GetRequiredVulkanInstanceExtensions(vulkanLoader.requiredInstanceExtensions);
	
	// Instanciate Renderer (this is also a Vulkan instance wrapper)
	auto* renderer = new MinimalRenderer(&vulkanLoader, "V4D Test", VK_MAKE_VERSION(1, 0, 0), window);
	renderer->LoadRenderer();
	
	// Render loop
	while (window->IsActive()) {
		glfwPollEvents();
		renderer->Render();
	}
	
	// Unload renderer and Window
	renderer->UnloadRenderer();
	delete renderer;
	delete window;
}
