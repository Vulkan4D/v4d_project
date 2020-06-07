#pragma once
#include "app.hh"

namespace app {

	class SlowGameLoop {
		std::unordered_map<v4d::io::FilePath, double> shaderFilesToWatch {};
		std::thread thread;
		bool (*loopCheckRunning)() = nullptr;
	public:
		SlowGameLoop(bool (*loopCheckRunningFunc)(), int cpuCoreIndex = -1) : loopCheckRunning(loopCheckRunningFunc) {
			
			#ifdef _DEBUG
				// Shaders to watch for modifications to automatically reload the app::renderer
				v4d::io::StringListFile::Instance("watchedShaders.txt", 1000)->Load([this](v4d::io::ASCIIFile* file){
					shaderFilesToWatch.clear();
					for (auto& shader : ((v4d::io::StringListFile*)file)->lines) shaderFilesToWatch[shader] = 0;
				});
			#endif
			
			// Slow Loop (stuff unrelated to rendering that does not require any performance)
			thread = std::thread{[&]{
				if (cpuCoreIndex >= 0) SET_CPU_AFFINITY(cpuCoreIndex)
				while (loopCheckRunning()) {
					static double deltaTime = 0.01;
					CALCULATE_AVG_FRAMERATE(app::slowLoopAvgFrameRate)
					CALCULATE_DELTATIME(deltaTime)
					
					V4D_Game::ForEachSortedModule([](auto* mod){
						if (mod->SlowUpdate) mod->SlowUpdate(deltaTime);
					});
					
					V4D_Physics::ForEachSortedModule([](auto* mod){
						if (mod->SlowStepSimulation) mod->SlowStepSimulation(deltaTime);
					});
					
					if (app::isServer) {
						V4D_Server::ForEachSortedModule([](auto* mod){
							if (mod->SlowGameLoop) mod->SlowGameLoop();
						});
					}
					
					if (app::isClient) {
						V4D_Client::ForEachSortedModule([](auto* mod){
							if (mod->SlowGameLoop) mod->SlowGameLoop();
						});
					}
					
					// Auto-reload modified shaders
					#if defined(_DEBUG)
						// Watch shader modifications to automatically reload the app::renderer
						for (auto&[f, t] : shaderFilesToWatch) {
							if (t == 0) {
								t = f.GetLastWriteTime();
							} else if (f.GetLastWriteTime() > t) {
								t = 0;
								SLEEP(500ms)
								app::renderer->ReloadRenderer();
								break;
							}
						}
						for (auto&[f, t] : shaderFilesToWatch) {
							t = f.GetLastWriteTime();
						}
					#endif
					
					LIMIT_FRAMERATE_FRAMETIME(4, app::slowLoopFrameTime)
				}
			}};
		}
		~SlowGameLoop() {
			thread.join();
		}
	};

}
