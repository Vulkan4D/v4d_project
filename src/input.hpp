#pragma once
#include "app.hh"

namespace app::input {

	void AddCallbacks() {
		V4D_Mod::ForEachSortedModule([](auto* mod){
			if (mod->InputCallbackName) {
				std::string callbackName = mod->InputCallbackName();
				if (mod->InputKeyCallback) {
					window->AddKeyCallback(callbackName, [mod](int key, int scancode, int action, int mods){
						mod->InputKeyCallback(key, scancode, action, mods);
					});
				}
				if (mod->MouseButtonCallback) {
					window->AddMouseButtonCallback(callbackName, [mod](int button, int action, int mods){
						mod->MouseButtonCallback(button, action, mods);
					});
				}
				if (mod->InputScrollCallback) {
					window->AddScrollCallback(callbackName, [mod](double x, double y){
						mod->InputScrollCallback(x, y);
					});
				}
				if (mod->InputCharCallback) {
					window->AddCharCallback(callbackName, [mod](unsigned int c){
						mod->InputCharCallback(c);
					});
				}
			}
		});
	}
	void RemoveCallbacks() {
		V4D_Mod::ForEachSortedModule([](auto* mod){
			if (mod->InputCallbackName) {
				std::string callbackName = mod->InputCallbackName();
				window->RemoveKeyCallback(callbackName);
				window->RemoveMouseButtonCallback(callbackName);
				window->RemoveScrollCallback(callbackName);
				window->RemoveCharCallback(callbackName);
			}
		});
	}

	void UpdateLoop(bool (*loopCheckRunningFunc)()) {
		while (loopCheckRunningFunc()) {
			static double deltaTime = 0.01;
			CALCULATE_AVG_FRAMERATE(app::inputAvgFrameRate)
			CALCULATE_DELTATIME(deltaTime)
			
			{
				std::lock_guard inputLock(app::inputMutex);
			
				glfwPollEvents();
				
				V4D_Mod::ForEachSortedModule([](auto* mod){
					if (mod->InputUpdate) mod->InputUpdate(deltaTime);
				});
			}
			
			LIMIT_FRAMERATE_FRAMETIME(100, app::inputFrameTime)
		}
	}

}
