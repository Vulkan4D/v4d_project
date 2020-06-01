#pragma once

// Modules
	#define APP_DEFAULT_RENDERER_MODULE "V4D_hybrid"
	#define APP_DEFAULT_MODULES "V4D_basicscene","V4D_bullet"
	// #define APP_DEFAULT_MODULES "V4D_planetdemo"

// Logger
	#define LOGGER_INSTANCE v4d::io::Logger::ConsoleInstance()

// Networking
	#define APP_ENABLE_SOLO
	#define APP_NETWORKING_DEFAULT_HOST "127.0.0.1"
	#define APP_NETWORKING_DEFAULT_PORT 12345
	// #define APP_NETWORKING_USE_RSA_KEY

// Threads
	#define APP_CPU_AFFINITY_MAIN 0
	#define APP_CPU_AFFINITY_GAME 1
	#define APP_CPU_AFFINITY_RENDER_PRIMARY 2
	// #define APP_RENDER_SECONDARY_IN_ANOTHER_THREAD
	#define APP_CPU_AFFINITY_RENDER_SECONDARY -1
