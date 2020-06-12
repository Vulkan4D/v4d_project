#pragma once
#include <v4d.h>
#include "settings.hh"

namespace app {

	// Settings file
	auto settings = ProjectSettings::Instance("settings.ini", 1000);

	std::atomic<bool> isRunning = true;
	bool IsRunning() {return isRunning;}
	std::recursive_mutex inputMutex;

	double primaryAvgFrameRate = 0;
	double primaryFrameTime = 0;
	#ifdef APP_RENDER_SECONDARY_IN_ANOTHER_THREAD
		double secondaryAvgFrameRate = 0;
		double secondaryFrameTime = 0;
	#endif
	double gameLoopAvgFrameRate = 0;
	double gameLoopFrameTime = 0;
	double slowLoopAvgFrameRate = 0;
	double slowLoopFrameTime = 0;
	double inputFrameTime = 0;
	double inputAvgFrameRate = 0;

	#define CALCULATE_AVG_FRAMERATE(varRef) {\
		static v4d::Timer t(true);\
		static double elapsedTime = 0.01;\
		static int nbFrames = 0;\
		++nbFrames;\
		elapsedTime = t.GetElapsedSeconds();\
		if (elapsedTime > 1.0) {\
			varRef = nbFrames / elapsedTime;\
			nbFrames = 0;\
			t.Reset();\
		}\
	}

	#define CALCULATE_DELTATIME(varRef) {\
		static v4d::Timer t(true);\
		varRef = t.GetElapsedSeconds();\
		t.Reset();\
	}

	#define LIMIT_FRAMERATE_FRAMETIME(targetFps, frameTimeRef) {\
		static v4d::Timer t(true);\
		double elapsedTime = t.GetElapsedSeconds();\
		frameTimeRef = std::max(0.10001, elapsedTime * 1000.0);\
		double timeToSleep = 1.0/targetFps - 1.0*elapsedTime;\
		if (timeToSleep > 0.001) SLEEP(1.0s * timeToSleep)\
		t.Reset();\
	}
	
	#define LIMIT_FRAMERATE(targetFps) {\
		static v4d::Timer t(true);\
		double elapsedTime = t.GetElapsedSeconds();\
		double timeToSleep = 1.0/targetFps - 1.0*elapsedTime;\
		if (timeToSleep > 0.001) SLEEP(1.0s * timeToSleep)\
		t.Reset();\
	}
	
	enum class ARG {invalid_arg=0
		,server
		,client
		,host
		,port
	};
	ARG Arg(std::string arg) {
		if (arg == "server") return ARG::server;
		if (arg == "client") return ARG::client;
		if (arg == "host") return ARG::host;
		if (arg == "port") return ARG::port;
		return ARG::invalid_arg;
	}

	std::vector<std::string> modulesList {};

	v4d::graphics::vulkan::Loader* vulkanLoader = nullptr;
	v4d::scene::Scene* scene = nullptr;
	v4d::graphics::Renderer* renderer = nullptr;
	v4d::graphics::Window* window = nullptr;

	#ifdef _ENABLE_IMGUI
		ImGuiIO* imGuiIO = nullptr;
	#endif

	bool isServer = false;
	bool isClient = false;
	bool hasGraphics = true;

	void Start();
	void Run();
	void Stop();

	// Thread Watcher
	#ifdef _DEBUG
		namespace threads {
			std::thread* threadWatcher = nullptr;
			std::mutex activeThreadsMutex;
			std::unordered_map<std::string, double> threadTickTimes {};
			std::unordered_map<std::string, bool> activeThreads {};
			std::unordered_map<std::string, double> threadMaxDeltas {};
			
			void StartThreadWatcher() {
				threadWatcher = new std::thread([](){
					while (app::isRunning) {
						{std::lock_guard lock(activeThreadsMutex);
							for (auto&[name, time] : threadTickTimes) {
								if (time != 0) activeThreads[name] = (time > v4d::Timer::GetCurrentTimestamp() - threadMaxDeltas[name]);
							}
						}
						SLEEP(100ms)
					}
				});
			}
			
			void EndThreadWatcher() {
				std::lock_guard lock(activeThreadsMutex);
				if (threadWatcher && threadWatcher->joinable()) {
					threadWatcher->join();
					delete threadWatcher;
					threadWatcher = nullptr;
				}
			}
		}
		
		#define THREAD_BEGIN(threadName, maxDeltaTime) std::string _threadName {threadName}; {\
			std::lock_guard lock(app::threads::activeThreadsMutex);\
			app::threads::activeThreads[_threadName] = true;\
			app::threads::threadTickTimes[_threadName] = v4d::Timer::GetCurrentTimestamp();\
			app::threads::threadMaxDeltas[_threadName] = maxDeltaTime;\
		}
		#define THREAD_END(keep) {\
			std::lock_guard lock(app::threads::activeThreadsMutex);\
			if (keep) {\
				app::threads::activeThreads[_threadName] = false;\
				app::threads::threadTickTimes[_threadName] = 0;\
			} else {\
				app::threads::activeThreads.erase(_threadName);\
				app::threads::threadTickTimes.erase(_threadName);\
			}\
		}
		#define THREAD_TICK {\
			std::lock_guard lock(app::threads::activeThreadsMutex);\
			app::threads::threadTickTimes[_threadName] = v4d::Timer::GetCurrentTimestamp();\
		}
	#else
		#define THREAD_BEGIN(threadName, maxDeltaTime)
		#define THREAD_END(keep)
		#define THREAD_TICK
	#endif

}
