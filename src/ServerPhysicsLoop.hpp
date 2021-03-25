#pragma once
#include "app.hh"

namespace app {

	class ServerPhysicsLoop {
		std::thread thread;
		bool (*loopCheckRunning)() = nullptr;
	public:
		ServerPhysicsLoop(bool (*loopCheckRunningFunc)()) : loopCheckRunning(loopCheckRunningFunc) {
			thread = std::thread{[&]{
				THREAD_BEGIN("Server Physics", 1) {
					if (!app::isServer) return;
					// SET_CPU_AFFINITY(...)
					while (loopCheckRunning()) {
						THREAD_TICK
						
						static double deltaTime = 0.01;
						CALCULATE_AVG_FRAMERATE(app::serverPhysicsLoopAvgFrameRate)
						CALCULATE_DELTATIME(deltaTime)
						
						// Run Server-side physics
						V4D_Mod::ForEachSortedModule([](auto* mod){
							if (mod->ServerPhysicsUpdate) mod->ServerPhysicsUpdate(deltaTime);
						});
						
						LIMIT_FRAMERATE_FRAMETIME(200, app::serverPhysicsLoopFrameTime)
					}
				}THREAD_END(true)
			}};
		}
		~ServerPhysicsLoop() {
			thread.join();
		}
	};

}
