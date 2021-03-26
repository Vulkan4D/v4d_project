#pragma once

#include "app.hh"
#include "utilities/io/StringListFile.h"

namespace app {

	class SlowGameLoop {
		std::unordered_map<v4d::io::FilePath, double> shaderFilesToWatch {};
		std::thread thread;
		bool (*loopCheckRunning)() = nullptr;
	public:
		SlowGameLoop(bool (*loopCheckRunningFunc)()) : loopCheckRunning(loopCheckRunningFunc) {
			
			#ifdef _DEBUG
				// Shaders to watch for modifications to automatically reload the app::renderer
				v4d::io::StringListFile::Instance("watchedShaders.txt", 1000)->Load([this](v4d::io::ASCIIFile* file){
					shaderFilesToWatch.clear();
					for (auto& shader : ((v4d::io::StringListFile*)file)->lines) shaderFilesToWatch[shader] = 0;
				});
			#endif
			
			// Slow Loop (stuff unrelated to rendering that does not require any performance)
			thread = std::thread{[&]{
				// SET_CPU_AFFINITY(...)
				while (loopCheckRunning()) {
					static double deltaTime = 0.01;
					CALCULATE_AVG_FRAMERATE(app::slowLoopAvgFrameRate)
					CALCULATE_DELTATIME(deltaTime)
					
					V4D_Mod::ForEachSortedModule([](auto* mod){
						if (mod->SlowLoopUpdate) mod->SlowLoopUpdate(deltaTime);
					});
					
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
