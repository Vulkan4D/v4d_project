// #define USE_RAY_TRACING

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

#include "incubator_rendering/Window.hpp"

// Window.cpp
// using namespace v4d::graphics;
std::unordered_map<int, Window*> Window::windows{};

// Vulkan
#ifdef USE_RAY_TRACING
	#include "incubator_rendering/VulkanRayTracingRenderer.hpp"
#else
	#include "incubator_rendering/VulkanRasterizationRenderer.hpp"
#endif
VulkanLoader vulkanLoader;

int main() {
	
	if (!vulkanLoader()) 
		throw std::runtime_error("Failed to load Vulkan library");
		
	// Needed for RayTracing
	vulkanLoader.requiredInstanceExtensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);

	// Create Window and Init Vulkan
	Window* window = new Window("TEST", 1280, 720);
	window->GetRequiredVulkanInstanceExtensions(vulkanLoader.requiredInstanceExtensions);
	
#ifdef USE_RAY_TRACING
	auto* vulkan = new VulkanRayTracingRenderer(&vulkanLoader, "V4D Test", VK_MAKE_VERSION(1, 0, 0), window);
#else
	auto* vulkan = new VulkanRasterizationRenderer(&vulkanLoader, "V4D Test", VK_MAKE_VERSION(1, 0, 0), window);
#endif
	
	vulkan->LoadRenderer();
	vulkan->LoadScene();
	vulkan->SendGraphicsToDevice();
	
	double camSpeed = 2.0, mouseSensitivity = 1.0;
	double horizontalAngle = -2.5;
	double verticalAngle = -0.5;
	vulkan->camDirection = glm::dvec3(
		cos(verticalAngle) * sin(horizontalAngle),
		cos(verticalAngle) * cos(horizontalAngle),
		sin(verticalAngle)
	);
	
	// Input Events
	window->AddKeyCallback("app", [window, vulkan](int key, int scancode, int action, int mods){
		vulkan->LockUBO();
		// Quit application upon pressing the Escape key
		if (action != GLFW_RELEASE) {
			// LOG(scancode) //TODO build platform-specific mapping for scancode when key == -1
			switch (key) {
				
				// Quit
				case GLFW_KEY_ESCAPE:
					glfwSetWindowShouldClose(window->GetHandle(), 1);
					break;
					
#ifdef USE_RAY_TRACING
				// Moving the light's position/intensity
				case GLFW_KEY_LEFT:
					vulkan->light.x += 0.5f;
					break;
				case GLFW_KEY_RIGHT:
					vulkan->light.x -= 0.5f;
					break;
				case GLFW_KEY_DOWN:
					vulkan->light.y += 0.5f;
					break;
				case GLFW_KEY_UP:
					vulkan->light.y -= 0.5f;
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
				case GLFW_KEY_1:
				case GLFW_KEY_2:
				case GLFW_KEY_3:
				case GLFW_KEY_4:
				case GLFW_KEY_5:
				case GLFW_KEY_6:
				case GLFW_KEY_7:
				case GLFW_KEY_8:
				case GLFW_KEY_9:
					vulkan->rtx_reflection_max_recursion = key - 48;
					break;
				// Samples per pixel
				case GLFW_KEY_KP_ADD:
					vulkan->samplesPerPixel++;
					break;
				case GLFW_KEY_KP_SUBTRACT:
					vulkan->samplesPerPixel--;
					if (vulkan->samplesPerPixel < 1) vulkan->samplesPerPixel = 1;
					if (vulkan->samplesPerPixel > 100) vulkan->samplesPerPixel = 100;
					break;
				
				// RTX Shadows
				case GLFW_KEY_KP_ENTER:
					vulkan->rtx_shadows = !vulkan->rtx_shadows;
					break;
#endif
				
				// Reload Renderer
				case GLFW_KEY_R:
					vulkan->ReloadRenderer();
					break;
					
			}
		}
		vulkan->UnlockUBO();
	});
	
	// Mouse buttons
	window->AddMouseButtonCallback("app", [window, vulkan](int button, int action, int mods){
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

	// Frame timer
	v4d::Timer timer(true);
	double currentFrameTime = 10;
	double deltaTime = 0.01f;
	
	// GameLoop
	while (window->IsActive()) {
		// Events
		glfwPollEvents();
		
		// Camera Movements
		double camSpeedMult = glfwGetKey(window->GetHandle(), GLFW_KEY_LEFT_SHIFT)? 10.0 : 1.0;
		if (glfwGetKey(window->GetHandle(), GLFW_KEY_W)) {
			vulkan->camPosition += vulkan->camDirection * camSpeed * camSpeedMult * deltaTime;
		}
		if (glfwGetKey(window->GetHandle(), GLFW_KEY_S)) {
			vulkan->camPosition -= vulkan->camDirection * camSpeed * camSpeedMult * deltaTime;
		}
		if (glfwGetKey(window->GetHandle(), GLFW_KEY_A)) {
			vulkan->camPosition -= glm::cross(vulkan->camDirection, glm::dvec3(0,0,1)) * camSpeed * camSpeedMult * deltaTime;
		}
		if (glfwGetKey(window->GetHandle(), GLFW_KEY_D)) {
			vulkan->camPosition += glm::cross(vulkan->camDirection, glm::dvec3(0,0,1)) * camSpeed * camSpeedMult * deltaTime;
		}
		if (glfwGetKey(window->GetHandle(), GLFW_KEY_SPACE)) {
			vulkan->camPosition += glm::dvec3(0,0,1) * camSpeed * camSpeedMult * deltaTime;
		}
		if (glfwGetKey(window->GetHandle(), GLFW_KEY_LEFT_CONTROL)) {
			vulkan->camPosition -= glm::dvec3(0,0,1) * camSpeed * camSpeedMult * deltaTime;
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
				vulkan->camDirection = glm::dvec3(
					cos(verticalAngle) * sin(horizontalAngle),
					cos(verticalAngle) * cos(horizontalAngle),
					sin(verticalAngle)
				);
			}
		}
		
		// Rendering
		vulkan->Render();
		
		// Frame time
		currentFrameTime = timer.GetElapsedMilliseconds();
		deltaTime = (double)currentFrameTime / 1000.0f;
		timer.Reset();
		
		// FPS counter
		glfwSetWindowTitle(window->GetHandle(), (std::to_string((int)(1000.0/currentFrameTime))+" FPS").c_str());
	}
	
	vulkan->DeleteGraphicsFromDevice();
	vulkan->UnloadScene();
	vulkan->UnloadRenderer();
	
	// Close Window and delete Vulkan
	delete vulkan;
	delete window;

	LOG("\n\nApplication terminated\n\n");
	
}
