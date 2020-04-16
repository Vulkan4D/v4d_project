#include <v4d.h>

// Settings file
#include "settings.hh"
auto settings = ProjectSettings::Instance("settings.ini", 1000);

using namespace v4d::graphics;

#define APPLICATION_NAME "V4D Test"
#define WINDOW_TITLE "TEST"
#define WINDOW_WIDTH 1024
#define WINDOW_HEIGHT 768
#define APPLICATION_VERSION VK_MAKE_VERSION(1, 0, 0)

#include "incubator_rendering/V4DRenderer2.hpp"

static std::vector<std::string> v4dModules {
	"incubator_simplemovearound",
	// "incubator_galaxy4d",
	"test1",
	// "test_planets_rtx",
};

#if defined(_DEBUG)
	// Shaders to watch for modifications to automatically reload the renderer
	static std::unordered_map<v4d::io::FilePath, double> shaderFilesToWatch {
		// {"incubator_rendering/assets/shaders/v4d_galaxy.meta", 0},
		// {"incubator_rendering/assets/shaders/rtx_galaxies.meta", 0},
		// {"incubator_galaxy4d/assets/shaders/planetRayMarching.meta", 0},
		// {"incubator_galaxy4d/assets/shaders/planetRaster.meta", 0},
		{"incubator_rendering/assets/shaders/v4d_lighting.meta", 0},
		{"incubator_rendering/assets/shaders/v4d_postProcessing.meta", 0},
		{"incubator_rendering/assets/shaders/v4d_histogram.meta", 0},
		{"modules/incubator_galaxy4d/assets/shaders/planetTerrain.meta", 0},
		{"modules/incubator_galaxy4d/assets/shaders/planetAtmosphere.meta", 0},
		{"modules/incubator_pbr_test/assets/shaders/test.meta", 0},
		{"incubator_rendering/assets/shaders/rtx.meta", 0},
		{"modules/test_planets_rtx/assets/shaders/planets.meta", 0},
		{"incubator_rendering/assets/shaders/raster.meta", 0},
	};
#endif

Loader vulkanLoader;

std::atomic<bool> appRunning = true;

double primaryAvgFrameRate = 0;
double secondaryAvgFrameRate = 0;
double gameLoopAvgFrameRate = 0;
double slowLoopAvgFrameRate = 0;
double inputAvgFrameRate = 0;

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

#define CALCULATE_DELTATIME(varRef) {\
	static v4d::Timer deltaTimer(true);\
	varRef = deltaTimer.GetElapsedSeconds();\
	deltaTimer.Reset();\
}

int main() {
	// Load settings
	settings->Load();

	SET_CPU_AFFINITY(0)
	
	// Core & Modules
	V4D_PROJECT_INSTANTIATE_CORE_IN_MAIN ( v4dCore )
	for (auto module : v4dModules) v4dCore->LoadModule(module);

	// Input Submodules
	auto inputSubmodules = v4d::modules::GetSubmodules<v4d::modules::Input>();
	
	// Validation layers
	#if defined(_DEBUG) && defined(_LINUX)
		vulkanLoader.requiredInstanceLayers.push_back("VK_LAYER_KHRONOS_validation");
	#endif
	
	// Vulkan
	if (!vulkanLoader()) 
		throw std::runtime_error("Failed to load Vulkan library");
	
	// Needed for RayTracing
	vulkanLoader.requiredInstanceExtensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);

	// Create Window and Init Vulkan
	Window* window = new Window(WINDOW_TITLE, WINDOW_WIDTH, WINDOW_HEIGHT);
	window->GetRequiredVulkanInstanceExtensions(vulkanLoader.requiredInstanceExtensions);

	// ImGui
	#ifdef _ENABLE_IMGUI
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO(); // (void)io;
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
		ImGui::StyleColorsDark();
		ImGui_ImplGlfw_InitForVulkan(window->GetHandle(), true);
	#endif
	
	// Create Renderer (and Vulkan Instance)
	auto* renderer = new V4DRenderer2(&vulkanLoader, APPLICATION_NAME, APPLICATION_VERSION, window);
	// Load renderer
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
	
	// Slow Loop (stuff unrelated to rendering that does not require any performance)
	std::thread slowLoopThread([&]{
		SET_CPU_AFFINITY(0)
		while (appRunning) {
			CALCULATE_FRAMERATE(slowLoopAvgFrameRate)
			if (!appRunning) break;
			
			// Auto-reload modified shaders
			#if defined(_DEBUG)
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
			
			SLEEP(500ms)
		}
	});
	
	// Game Loop (stuff unrelated to rendering)
	std::thread gameLoopThread([&]{
		SET_CPU_AFFINITY(1)
		while (appRunning) {
			CALCULATE_FRAMERATE(gameLoopAvgFrameRate)
			if (!appRunning) break;
			
			//...
			
			SLEEP(20ms)
		}
	});
	
	// Low-Priority Rendering Loop
	std::thread lowPriorityRenderingThread([&]{
		SET_CPU_AFFINITY(2)
		while (appRunning) {
			CALCULATE_FRAMERATE(secondaryAvgFrameRate)
			if (!appRunning) break;
			
			// ImGui
			#ifdef _ENABLE_IMGUI
				static bool showOtherUI = true;
				ImGui_ImplVulkan_NewFrame();
				
				ImGui_ImplGlfw_NewFrame();// this function calls glfwGetWindowAttrib() to check for focus before fetching mouse pos and always returns false if called on a secondary thread... 
				// The quick fix is simply to always fetch the mouse position right here...
				double mouse_x, mouse_y;
				glfwGetCursorPos(window->GetHandle(), &mouse_x, &mouse_y);
				io.MousePos = ImVec2((float)mouse_x, (float)mouse_y);
				
				ImGui::NewFrame();
				
				// Main info UI
				ImGui::SetNextWindowPos({20,0});
				ImGui::SetNextWindowSizeConstraints({400, 140}, {400, 140});
				ImGui::Begin("Vulkan4D: V4DRenderer2 (Incubator)");
				ImGui::Text("Primary rendering thread : %.1f FPS", primaryAvgFrameRate);
				ImGui::Text("Secondary rendering thread & UI : %.1f FPS", secondaryAvgFrameRate);
				ImGui::Text("Input thread : %.1f FPS", inputAvgFrameRate);
				ImGui::Text("Game Loop thread : %.1f FPS", gameLoopAvgFrameRate);
				ImGui::Text("Slow Loop thread : %.1f FPS", slowLoopAvgFrameRate);
				ImGui::Checkbox("Show other UI windows", &showOtherUI);
				ImGui::End();
				ImGui::SetNextWindowPos({425,0});
				
				if (showOtherUI) {
					renderer->RunImGui();
				}
				
				ImGui::Render();
			#endif
			
			renderer->RenderLowPriority();
			
			SLEEP(10ms)
		}
	});
	
	// Rendering Loop
	std::thread renderingThread([&]{
		SET_CPU_AFFINITY(3)
		while (appRunning) {
			CALCULATE_FRAMERATE(primaryAvgFrameRate)
			renderer->Render();
		}
	});
	
	// Input loop
	while (window->IsActive()) {
		static double deltaTime = 0.01;
		CALCULATE_FRAMERATE(inputAvgFrameRate)
		CALCULATE_DELTATIME(deltaTime)
	
		glfwPollEvents();
	
		for (auto* submodule : inputSubmodules) {
			submodule->Update(deltaTime);
		}
		
		SLEEP(10ms)
	}
	
	appRunning = false;
	
	gameLoopThread.join();
	slowLoopThread.join();
	renderingThread.join();
	lowPriorityRenderingThread.join();
	
	for (auto* submodule : inputSubmodules) {
		submodule->RemoveCallbacks();
	}
	
	renderer->UnloadRenderer();
	renderer->UnloadScene();
	
	// ImGui
	#ifdef _ENABLE_IMGUI
		ImGui_ImplGlfw_Shutdown();
		ImGui::DestroyContext();
	#endif
	
	// Close Window and delete Vulkan
	delete renderer;
	delete window;

	// Unload modules
	for (auto module : v4dModules) v4dCore->UnloadModule(module);
	
	LOG("\n\nApplication terminated\n\n");
}
