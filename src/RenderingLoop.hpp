#pragma once
#include "app.hh"

namespace app {

	class RenderingLoop {
		std::thread thread;
		#ifdef APP_RENDER_SECONDARY_IN_ANOTHER_THREAD
			std::thread thread2;
		#endif
		bool (*loopCheckRunning)() = nullptr;
			
		std::function<void()> RunSecondaryRendering = [](){
			std::scoped_lock lock(app::renderer->renderMutex2);
			
			// ImGui
			static bool showOtherUI = true;
			#ifdef _ENABLE_IMGUI
				if (imGuiIO->Fonts->IsBuilt()) {
					std::lock_guard inputLock(app::inputMutex);
					
					ImGui_ImplVulkan_NewFrame();
					
					ImGui_ImplGlfw_NewFrame();// this function calls glfwGetWindowAttrib() to check for focus before fetching mouse pos and always returns false if called on a secondary thread... 
					// The quick fix is simply to always fetch the mouse position right here...
					double mouse_x, mouse_y;
					glfwGetCursorPos(app::window->GetHandle(), &mouse_x, &mouse_y);
					imGuiIO->MousePos = ImVec2((float)mouse_x, (float)mouse_y);
					
					
					ImGui::NewFrame();
					
					// Main info UI
					ImGui::SetNextWindowPos({20,0}, ImGuiCond_FirstUseEver);
					ImGui::SetNextWindowSize({400, 140}, ImGuiCond_FirstUseEver);
					ImGui::Begin("Vulkan4D: Renderer (Incubator)");
					#ifdef V4D_DEMO_DEBUG_FULL_FRAMERATE
						ImGui::Text("Primary rendering : %.1f / %d FPS (%d ms)", app::primaryAvgFrameRate, (int)std::round(1000.0/app::primaryFrameTime), (int)std::round(app::primaryFrameTime));
						#ifdef APP_RENDER_SECONDARY_IN_ANOTHER_THREAD
							ImGui::Text("Secondary rendering & UI : %.1f / %d FPS (%d ms)", app::secondaryAvgFrameRate, (int)std::round(1000.0/app::secondaryFrameTime), (int)std::round(app::secondaryFrameTime));
						#endif
						ImGui::Text("Input thread : %.1f / %d FPS (%d ms)", app::inputAvgFrameRate, (int)std::round(1000.0/app::inputFrameTime), (int)std::round(app::inputFrameTime));
						ImGui::Text("Game Loop thread : %.1f / %d FPS (%d ms)", app::gameLoopAvgFrameRate, (int)std::round(1000.0/app::gameLoopFrameTime), (int)std::round(app::gameLoopFrameTime));
						ImGui::Text("Slow Loop thread : %.1f / %d FPS (%d ms)", app::slowLoopAvgFrameRate, (int)std::round(1000.0/app::slowLoopFrameTime), (int)std::round(app::slowLoopFrameTime));
					#else
						if (settings->framerate_limit_rendering)
							ImGui::Text("Primary rendering : %.1f avg FPS (limited)", app::primaryAvgFrameRate);
						else
							ImGui::Text("Primary rendering : %.1f avg FPS (unlimited)", app::primaryAvgFrameRate);
						#ifdef APP_RENDER_SECONDARY_IN_ANOTHER_THREAD
							if (settings->framerate_limit_ui)
								ImGui::Text("Secondary rendering & UI : %.1f avg FPS (limited)", app::secondaryAvgFrameRate);
							else
								ImGui::Text("Secondary rendering & UI : %.1f avg FPS (unlimited)", app::secondaryAvgFrameRate);
						#endif
						ImGui::Text("Input thread : %.1f avg FPS (limited)", app::inputAvgFrameRate);
						ImGui::Text("Game Loop thread : %.1f avg FPS (limited)", app::gameLoopAvgFrameRate);
						ImGui::Text("Slow Loop thread : %.1f avg FPS (limited)", app::slowLoopAvgFrameRate);
						if (settings->framerate_limit_physics) {
							ImGui::Text("Server Physics thread : %.1f avg FPS (limited)", app::serverPhysicsLoopAvgFrameRate);
						} else {
							ImGui::Text("Server Physics thread : %.1f avg FPS (unlimited)", app::serverPhysicsLoopAvgFrameRate);
						}
					#endif
					ImGui::Checkbox("Show other UI windows", &showOtherUI);
					
					#ifdef _DEBUG
						ImGui::Separator();
						ImGui::Text("Thread Watcher");
						{
							using namespace app::threads;
							std::lock_guard lock(activeThreadsMutex);
							for (auto&[name, active] : activeThreads) {
								if (active) {
									ImGui::TextColored({0,1,0,1}, std::string(name + " : active").c_str());
								} else if (threadTickTimes[name]) {
									ImGui::TextColored({1,1,0,1}, std::string(name + " : frozen").c_str());
								} else {
									ImGui::TextColored({1,0,1,1}, std::string(name + " : ended").c_str());
								}
							}
						}
					#endif
					
					ImGui::End();
					ImGui::SetNextWindowPos({425,0}, ImGuiCond_FirstUseEver);
			#endif
					
			if (showOtherUI) {
				// Modules
				V4D_Mod::ForEachSortedModule([](auto* mod){
					if (mod->DrawUi) mod->DrawUi();
				});
			}
			
			#ifdef _ENABLE_IMGUI
					ImGui::Render();
				}
			#endif
			
			if (!app::renderer->graphicsLoadedToDevice || app::renderer->mustReload) return;
			
			{
				V4D_Mod::ForEachSortedModule([](auto* mod){
					if (mod->SecondaryRenderUpdate) mod->SecondaryRenderUpdate();
				});
			}
			
		};

	public:
		RenderingLoop(bool (*loopCheckRunningFunc)()) : loopCheckRunning(loopCheckRunningFunc) {
			thread = std::thread{[&]{
				
				// CPU Affinity
				if (std::thread::hardware_concurrency() > 4) SET_CPU_AFFINITY(std::thread::hardware_concurrency()/2+1)
				else SET_CPU_AFFINITY(1)
				
				SET_THREAD_HIGHEST_PRIORITY()
				while (loopCheckRunning()) {
					static double deltaTime = 1.0/60;
					CALCULATE_AVG_FRAMERATE(app::primaryAvgFrameRate)
					CALCULATE_DELTATIME(deltaTime)
					
					#ifndef APP_RENDER_SECONDARY_IN_ANOTHER_THREAD
						RunSecondaryRendering();
					#endif
					
					app::renderer->Update(deltaTime);
				
					// // Run physics
					// V4D_Mod::ForEachSortedModule([](auto* mod){
					// 	if (mod->PhysicsUpdate) mod->PhysicsUpdate(deltaTime);
					// });
					
					if (settings->framerate_limit_rendering) LIMIT_FRAMERATE_FRAMETIME(settings->framerate_limit_rendering, app::primaryFrameTime)
				}
			}};
			#ifdef APP_RENDER_SECONDARY_IN_ANOTHER_THREAD
				// Low-Priority Rendering Loop
				thread2 = std::thread{[&]{
					
					if (std::thread::hardware_concurrency() > 4) UNSET_CPU_AFFINITY(0, 1, std::thread::hardware_concurrency()/2, std::thread::hardware_concurrency()/2+1)
					else SET_CPU_AFFINITY(0)
					
					while (loopCheckRunning()) {
						CALCULATE_AVG_FRAMERATE(app::secondaryAvgFrameRate)
						if (!loopCheckRunning()) break;
						if (!app::renderer->graphicsLoadedToDevice || app::renderer->mustReload) continue;
						
						RunSecondaryRendering();
						
						if (settings->framerate_limit_ui) LIMIT_FRAMERATE_FRAMETIME(settings->framerate_limit_ui, app::secondaryFrameTime)
					}
				}};
			#endif
		}
		~RenderingLoop() {
			thread.join();
			#ifdef APP_RENDER_SECONDARY_IN_ANOTHER_THREAD
				thread2.join();
			#endif
		}
	};

}
