#pragma once
#include "app.hh"

// Fix for a microsoft macro that is not respecting the C++ standards
#ifdef CreateWindow
	#undef CreateWindow
#endif

namespace app::graphics {

	void CreateWindow() {
		// Create Window and Init Vulkan
		app::window = new v4d::graphics::Window(WINDOW_TITLE, 1024, 768);
		glfwMaximizeWindow(app::window->GetHandle());
		app::window->GetRequiredVulkanInstanceExtensions(app::vulkanLoader->requiredInstanceExtensions);
	}
	void DestroyWindow() {
		delete app::window;
	}

	void LoadUi() {// Graphical interface
		// ImGui
		#ifdef _ENABLE_IMGUI
			IMGUI_CHECKVERSION();
			ImGui::CreateContext();
			app::imGuiIO = &ImGui::GetIO();
			app::imGuiIO->ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
			ImGui::StyleColorsDark();
			ImGui::GetStyle().Alpha = 0.8f;
			ImGui_ImplGlfw_InitForVulkan(app::window->GetHandle(), true);
		#endif
		
	}
	void UnloadUi (){
		// ImGui
		#ifdef _ENABLE_IMGUI
			ImGui_ImplGlfw_Shutdown();
			ImGui::DestroyContext();
		#endif
	}

	void CreateRenderer() {
		app::renderer = new v4d::graphics::Renderer(app::vulkanLoader, APPLICATION_NAME, VK_MAKE_VERSION(APPLICATION_VERSION_MAJOR, APPLICATION_VERSION_MINOR, APPLICATION_VERSION_PATCH));
		app::renderer->preferredPresentModes = {
			VK_PRESENT_MODE_MAILBOX_KHR,	// TripleBuffering (No Tearing, low latency)
			VK_PRESENT_MODE_IMMEDIATE_KHR,	// VSync OFF (With Tearing, no latency)
			VK_PRESENT_MODE_FIFO_KHR,		// VSync ON (No Tearing, more latency)
		};
		app::renderer->surface = app::window->CreateVulkanSurface(app::renderer->handle);
	}
	void DestroyRenderer() {
		app::renderer->DestroySurfaceKHR(app::renderer->surface, nullptr);
		delete app::renderer;
	}

	void LoadRenderer() {
		app::renderer->InitRenderer();
		app::renderer->ReadShaders();
		app::renderer->LoadRenderer();
	}

}
