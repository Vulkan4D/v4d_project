#include "config.hh"
#include <common/pch.hh>
#include <numeric>
#include <v4d.h>

#define XVK_INTERFACE_RAW_FUNCTIONS_ACCESSIBILITY private
#include "xvk.hpp"

// GLFW
#include <GLFW/glfw3.h>

// GLM
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/hash.hpp>

#include "incubator_MyVulkan/Window.hpp"

// Window.cpp
// using namespace v4d::graphics;
std::unordered_map<int, Window*> Window::windows{};

// Vulkan
#include "incubator_MyVulkan/MyVulkan_rtx.hpp"
VulkanLoader vulkanLoader;

int main() {
	
	if (!vulkanLoader()) 
		throw std::runtime_error("Failed to load Vulkan library");
		
	// Needed for RayTracing
	vulkanLoader.requiredInstanceExtensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);

	// try {

		// Create Window and Init Vulkan
		Window* window = new Window("TEST", 1440, 900);
		window->GetRequiredVulkanInstanceExtensions(vulkanLoader.requiredInstanceExtensions);
		
		MyVulkanTest* vulkan = new MyVulkanTest(&vulkanLoader, "V4D Test", VK_MAKE_VERSION(1, 0, 0), window);
		
		vulkan->LoadRenderer();
		
		// Input Events
		window->AddKeyCallback("app", [window, vulkan](int key, int scancode, int action, int mods){
			// Quit application upon pressing the Escape key
			if (action == GLFW_PRESS) {
				switch (key) {
					
					// Quit
					case GLFW_KEY_ESCAPE:
						glfwSetWindowShouldClose(window->GetHandle(), 1);
						break;
						
					// Toggle between Rasterization/RayTracing
					case GLFW_KEY_R:
						vulkan->ToggleRayTracing();
						break;
						
					// Moving the light's position/intensity
					case GLFW_KEY_LEFT:
						vulkan->light.x -= 0.5f;
						break;
					case GLFW_KEY_RIGHT:
						vulkan->light.x += 0.5f;
						break;
					case GLFW_KEY_DOWN:
						vulkan->light.y -= 0.5f;
						break;
					case GLFW_KEY_UP:
						vulkan->light.y += 0.5f;
						break;
					case GLFW_KEY_PAGE_DOWN:
						vulkan->light.z -= 0.5f;
						break;
					case GLFW_KEY_PAGE_UP:
						vulkan->light.z += 0.5f;
						break;
					case GLFW_KEY_END:
						vulkan->light.w -= 0.1f;
						break;
					case GLFW_KEY_HOME:
						vulkan->light.w += 0.1f;
						break;
						
					// RTX Bounce Recursion
					case GLFW_KEY_KP_ADD:
						vulkan->rtx_reflection_max_recursion++;
						break;
					case GLFW_KEY_KP_SUBTRACT:
						vulkan->rtx_reflection_max_recursion--;
						if (vulkan->rtx_reflection_max_recursion < 1) vulkan->rtx_reflection_max_recursion = 1;
						break;
				}
			}
		});

		// for FPS counter
		v4d::Timer timer;
		const int fpsNbFramesAvg = 20;
		std::array<double, fpsNbFramesAvg> frameTimes {10};
		int frameTimesCursor = 0;
		double minFrameTime = 1.0;

		// GameLoop
		while (window->IsActive()) {
			glfwPollEvents();
			
			timer.Start();

			vulkan->RenderFrame();

			// FPS counter
			double currentFrameTime = timer.GetElapsedMilliseconds();
			{// Hack to make it work on Windows because of chrono precision issues
				if (currentFrameTime > 0.0 && currentFrameTime < minFrameTime) minFrameTime = currentFrameTime;
				else if (currentFrameTime <= 0.0) currentFrameTime = minFrameTime;
			}
			if (frameTimesCursor >= fpsNbFramesAvg) frameTimesCursor = 0;
			frameTimes[frameTimesCursor++] = currentFrameTime;
			double avgFrameTime = std::accumulate(frameTimes.begin(), frameTimes.end(), 0.0) / fpsNbFramesAvg;
			glfwSetWindowTitle(window->GetHandle(), (std::to_string((int)(1000.0/avgFrameTime))+" FPS via " + (vulkan->IsUsingRayTracing()? "RayTracing" : "Rasterization")).c_str());
			
			SLEEP(20ms)
			// break;
			
		}
		
		// SLEEP(10s)
		
		vulkan->UnloadRenderer();
		
		// Close Window and delete Vulkan
		delete vulkan;
		delete window;

		LOG("\n\nApplication terminated\n\n");
	// } catch (std::exception &e) {
	// 	LOG_ERROR(e.what())
	// } catch (...) {
	// 	LOG_ERROR("Unknown error")
	// }
	
}
