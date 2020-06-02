#pragma once
#include "app.hh"

namespace app::input {

	void AddCallbacks() {
		V4D_Input::ForEachSortedModule([](auto* mod){
			mod->AddCallbacks(app::window);
		});
	}
	void RemoveCallbacks() {
		V4D_Input::ForEachSortedModule([](auto* mod){
			mod->RemoveCallbacks(app::window);
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
				
				V4D_Input::ForEachSortedModule([](auto* mod){
					if (mod->Update) mod->Update(deltaTime);
				});
			}
			
			LIMIT_FRAMERATE_FRAMETIME(100, app::inputFrameTime)
		}
	}

}
