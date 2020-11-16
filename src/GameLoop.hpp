#pragma once
#include "app.hh"

namespace app {

	class GameLoop {
		std::thread thread;
		bool (*loopCheckRunning)() = nullptr;
	public:
		GameLoop(bool (*loopCheckRunningFunc)()) : loopCheckRunning(loopCheckRunningFunc) {
			thread = std::thread{[&]{
				// SET_CPU_AFFINITY(...)
				while (loopCheckRunning()) {
					static double deltaTime = 0.01;
					CALCULATE_AVG_FRAMERATE(app::gameLoopAvgFrameRate)
					CALCULATE_DELTATIME(deltaTime)
					
					V4D_Mod::ForEachSortedModule([](auto* mod){
						if (mod->GameLoopUpdate) mod->GameLoopUpdate(deltaTime);
					});
					
					// Run physics
					V4D_Mod::ForEachSortedModule([](auto* mod){
						if (mod->PhysicsUpdate) mod->PhysicsUpdate(deltaTime);
					});
					
					LIMIT_FRAMERATE_FRAMETIME(60, app::gameLoopFrameTime)
				}
			}};
		}
		~GameLoop() {
			thread.join();
		}
	};

}
