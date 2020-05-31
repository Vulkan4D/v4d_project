#pragma once
#include <v4d.h>
#include "settings.hh"

namespace app {

	// Settings file
	auto settings = ProjectSettings::Instance("settings.ini", 1000);

	std::atomic<bool> isRunning = true;
	bool IsRunning() {return isRunning;}
	std::mutex inputMutex;

	double primaryAvgFrameRate = 0;
	double primaryFrameTime = 0;
	#ifdef RENDER_SECONDARY_IN_ANOTHER_THREAD
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

	#define LIMIT_FRAMERATE(targetFps, frameTimeRef) {\
		static v4d::Timer t(true);\
		double elapsedTime = t.GetElapsedSeconds();\
		frameTimeRef = std::max(0.10001, elapsedTime * 1000.0);\
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

}
