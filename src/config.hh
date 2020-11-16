#pragma once

// Modules
	#define APP_DEFAULT_RENDERER_MODULE "V4D_hybrid"
	// #define APP_DEFAULT_MODULES "V4D_sample",APP_DEFAULT_RENDERER_MODULE,"V4D_flycam","V4D_basicscene","V4D_multiplayer","V4D_bullet","V4D_buildsystem"
	#define APP_DEFAULT_MODULES "V4D_sample",APP_DEFAULT_RENDERER_MODULE,"V4D_flycam","V4D_planetdemo","V4D_multiplayer","V4D_bullet","V4D_buildsystem","V4D_galaxy4d"
	
	#define APP_DEFAULT_MODULES_SAVE_TO_MODULES_TXT_WHEN_EMPTY

// Logger
	#define LOGGER_INSTANCE v4d::io::Logger::ConsoleInstance()

// Networking
	#define APP_ENABLE_SOLO
	#define APP_ENABLE_BURST_STREAMS
	#define APP_NETWORKING_DEFAULT_HOST "127.0.0.1"
	#define APP_NETWORKING_DEFAULT_PORT 12345
	// #define APP_NETWORKING_USE_RSA_KEY
	#define APP_NETWORKING_POLL_TIMEOUT_MS 100
	#define APP_NETWORKING_MAX_ACTION_STREAMS_PER_SECOND 20
	#define APP_NETWORKING_MAX_BURST_STREAMS_PER_SECOND 30
	#define APP_NETWORKING_ACTION_BUFFER_SIZE 1024*1024
	#define APP_NETWORKING_BURST_BUFFER_MAXIMUM_SIZE 508

// Renderer
	#define APP_RENDER_SECONDARY_IN_ANOTHER_THREAD
	// #define RENDERER_RAY_TRACING_INSTANCES_DEFRAG_MODE_INDIVIDUAL_DELETE
