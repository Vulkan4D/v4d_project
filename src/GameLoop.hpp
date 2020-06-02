#pragma once
#include "app.hh"

namespace app {

	class GameLoop {
		std::thread thread;
		V4D_Physics* primaryPhysicsModule;
		bool (*loopCheckRunning)() = nullptr;
	public:
		GameLoop(bool (*loopCheckRunningFunc)(), int cpuCoreIndex = -1) : loopCheckRunning(loopCheckRunningFunc) {
			primaryPhysicsModule = V4D_Physics::GetPrimaryModule();
			thread = std::thread{[&]{
				if (cpuCoreIndex >= 0) SET_CPU_AFFINITY(cpuCoreIndex)
				while (loopCheckRunning()) {
					static double deltaTime = 0.01;
					CALCULATE_AVG_FRAMERATE(app::gameLoopAvgFrameRate)
					CALCULATE_DELTATIME(deltaTime)
					
					V4D_Game::ForEachSortedModule([](auto* mod){
						if (mod->Update) mod->Update(deltaTime);
					});
					
					// Run physics
					if (primaryPhysicsModule && primaryPhysicsModule->StepSimulation) {
						primaryPhysicsModule->StepSimulation(deltaTime);
					} else {
						V4D_Physics::ForEachSortedModule([](auto* mod){
							if (mod->StepSimulation) mod->StepSimulation(deltaTime);
						});
					}
					
					LIMIT_FRAMERATE_FRAMETIME(60, app::gameLoopFrameTime)
				}
			}};
		}
		~GameLoop() {
			thread.join();
		}
	};

}
