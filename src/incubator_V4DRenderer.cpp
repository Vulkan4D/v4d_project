#include "config.hh"
#include <v4d.h>

using namespace v4d::graphics;

#define APPLICATION_NAME "V4D Test"
#define WINDOW_TITLE "TEST"
#define WINDOW_WIDTH 1280
#define WINDOW_HEIGHT 720
#define APPLICATION_VERSION VK_MAKE_VERSION(1, 0, 0)

#include "incubator_rendering/V4DRenderer.hpp"

static std::vector<std::string> v4dModules {
	"incubator_simplemovearound",
	"incubator_galaxy4d",
};

#if defined(_DEBUG) && defined(_LINUX)
	// Shaders to watch for modifications to automatically reload the renderer
	static std::unordered_map<v4d::io::FilePath, double> shaderFilesToWatch {
		// {"incubator_rendering/assets/shaders/v4d_galaxy.meta", 0},
		// {"incubator_rendering/assets/shaders/rtx_galaxies.meta", 0},
		// {"incubator_galaxy4d/assets/shaders/planetRayMarching.meta", 0},
		{"incubator_galaxy4d/assets/shaders/planetRaster.meta", 0},
	};
#endif

Loader vulkanLoader;

std::atomic<bool> appRunning = true;

int main() {
	// SET_CPU_AFFINITY(0)
	
	// Core & Modules
	V4D_PROJECT_INSTANTIATE_CORE_IN_MAIN ( v4dCore )
	for (auto module : v4dModules) v4dCore->LoadModule(module);

	// Input Submodules
	auto inputSubmodules = v4d::modules::GetSubmodules<v4d::modules::Input>();
	
	// Vulkan
	if (!vulkanLoader()) 
		throw std::runtime_error("Failed to load Vulkan library");
	
	// // Needed for RayTracing
	// vulkanLoader.requiredInstanceExtensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);

	// Create Window and Init Vulkan
	Window* window = new Window(WINDOW_TITLE, WINDOW_WIDTH, WINDOW_HEIGHT);
	window->GetRequiredVulkanInstanceExtensions(vulkanLoader.requiredInstanceExtensions);
	
	// Renderer
	auto* renderer = new V4DRenderer(&vulkanLoader, APPLICATION_NAME, APPLICATION_VERSION, window);
	renderer->preferredPresentModes = {
		VK_PRESENT_MODE_MAILBOX_KHR,	// TripleBuffering (No Tearing, low latency)
		VK_PRESENT_MODE_IMMEDIATE_KHR,	// VSync OFF (With Tearing, no latency)
		VK_PRESENT_MODE_FIFO_KHR,		// VSync ON (No Tearing, more latency)
	};
	renderer->InitRenderer();
	renderer->ReadShaders();
	renderer->LoadScene();
	renderer->LoadRenderer();
	
	// Submodules
	for (auto* submodule : inputSubmodules) {
		submodule->SetWindow(window);
		submodule->SetRenderer(renderer);
		submodule->Init();
		submodule->AddCallbacks();
	}
	
	// Game Loop (stuff unrelated to rendering)
	std::thread gameLoopThread([&]{
		// SET_CPU_AFFINITY(1)
		while (appRunning) {
			//...
			SLEEP(10ms)
		}
	});
	
	// Low-Priority Rendering Loop
	std::thread lowPriorityRenderingThread([&]{
		// SET_CPU_AFFINITY(2)
		while (appRunning) {
			// std::this_thread::yield();
			if (!appRunning) break;
			renderer->RenderLowPriority();
			// SLEEP(10ms)
		}
	});
	
	// Rendering Loop
	std::thread renderingThread([&]{
		// SET_CPU_AFFINITY(3)
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
		// SET_CPU_AFFINITY(1)
		
		// Events
		glfwPollEvents();
		
		for (auto* submodule : inputSubmodules) {
			submodule->Update();
		}
		
		#if defined(_DEBUG) && defined(_LINUX)
			// Watch shader modifications to automatically reload the renderer
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
	
	for (auto* submodule : inputSubmodules) {
		submodule->RemoveCallbacks();
	}
	
	renderer->UnloadRenderer();
	renderer->UnloadScene();
	
	// Close Window and delete Vulkan
	delete renderer;
	delete window;

	// Unload modules
	for (auto module : v4dModules) v4dCore->UnloadModule(module);
	
	LOG("\n\nApplication terminated\n\n");
}
