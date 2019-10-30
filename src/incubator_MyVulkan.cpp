#include <common/pch.hh>
#include "config.hh"
#include <v4d.h>

// GLFW
// #define GLFW_INCLUDE_VULKAN
#include <xvk.hpp>
#include <GLFW/glfw3.h>

// GLM
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/hash.hpp>

/////////////////////////////////////////////

#define VULKAN_API_VERSION VK_API_VERSION_1_1
#define V4D_ENGINE_NAME "Vulkan4D"
#define V4D_ENGINE_VERSION VK_MAKE_VERSION(1, 0, 0)
// #define APPLICATION_NAME "V4D Vulkan Incubator"
// #define APPLICATION_VERSION VK_MAKE_VERSION(1, 0, 0)

std::vector<const char*> vulkanRequiredExtensions = {
	// extensions from glfw for creating a window are automatically added to this list
	#ifdef _DEBUG
	VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
	#endif
};
std::vector<const char*> vulkanRequiredLayers = {
	#ifdef _DEBUG
	"VK_LAYER_LUNARG_standard_validation",
	#endif
};


#include "incubator_MyVulkan/MyVulkan.hpp"
#include "incubator_MyVulkan/Window.hpp"


// Window.cpp
// using namespace v4d::graphics;
std::unordered_map<int, Window*> Window::windows{};


// Vulkan Dynamic Loader
xvk::Loader vulkanLoader;

int main() {
	
	if (!vulkanLoader()) 
		throw std::runtime_error("Failed to load Vulkan library");

	try {

		// Create Window and Init Vulkan
		Window *window;
		MyVulkan *vulkan;
		window = new Window("TEST", 1440, 900);
		vulkan = new MyVulkan(&vulkanLoader, "V4D Test", VK_MAKE_VERSION(1, 0, 0), window);
		
		// Input Events
		glfwSetKeyCallback(window->GetHandle(), [](GLFWwindow* window, int key, int scancode, int action, int mods){
			// Quit application upon pressing the Escape key
			if (action == GLFW_PRESS && key == GLFW_KEY_ESCAPE) {
				glfwSetWindowShouldClose(window, 1);
			}
		});

		// FPS
		std::queue<double> frameTimeQueue;
		auto start = std::chrono::high_resolution_clock::now();
		long frameNumber = 0;
		// double deltaTime;

		// GameLoop
		while (window->IsActive()) {

			// FPS
			start = std::chrono::high_resolution_clock::now();

			glfwPollEvents();

			// DRAW CALLS HERE
			vulkan->RenderFrame();

			// window->SwapBuffers(); // not sure if this is really needed...


			// FPS
			std::chrono::duration<double, std::milli> duration = std::chrono::high_resolution_clock::now() - start;
			frameTimeQueue.push(duration.count());
			if (frameTimeQueue.size() == 10001) frameTimeQueue.pop();
			frameNumber++;
			// deltaTime = duration.count() / 1000.0;
		}


		// FPS
		double totalTime = 0;
		size_t nbTimes = frameTimeQueue.size();
		while (frameTimeQueue.size() > 0) {
			totalTime += frameTimeQueue.front();
			frameTimeQueue.pop();
		}
		auto frameTime = nbTimes > 0? (totalTime / (double)nbTimes) : 0;
		auto FPS = frameTime > 0.0 ? 1000/frameTime : 0;
		LOG("Average Frame Time: " << frameTime << " ms (" << (FPS) << " FPS)");


		
		// Close Window and delete Vulkan
		delete vulkan;
		delete window;

		LOG("\n\nApplication terminated\n\n");
	// } catch (vk::SystemError &e) {
	// 	LOG_ERROR("vk::SystemError: " << e.what())
	} catch (std::exception &e) {
		LOG_ERROR(e.what())
	} catch (...) {
		LOG_ERROR("Unknown error")
	}
	
}
