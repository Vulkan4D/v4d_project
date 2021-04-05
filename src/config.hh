#pragma once

// Modules
	#define APP_MAIN_RENDER_MODULE "V4D_raytracing"
	#define APP_MAIN_MULTIPLAYER_MODULE "V4D_multiplayer"
	#define APP_DEFAULT_MODULES APP_MAIN_RENDER_MODULE,APP_MAIN_MULTIPLAYER_MODULE,"V4D_andromeda","V4D_flycam","V4D_buildsystem","V4D_galaxy4d"//,"V4D_avatar"
	
	#define APP_DEFAULT_MODULES_SAVE_TO_MODULES_TXT_WHEN_EMPTY

// Logger
	#define LOGGER_INSTANCE v4d::io::Logger::ConsoleInstance()

// Networking
	#define APP_ENABLE_SOLO
	#define APP_ENABLE_BURST_STREAMS
	#define APP_NETWORKING_DEFAULT_HOST "testserver1.vulkan4d.com"
	#define APP_NETWORKING_DEFAULT_PORT 12345
	// #define APP_NETWORKING_USE_RSA_KEY
	#define APP_NETWORKING_POLL_TIMEOUT_MS 100
	#define APP_NETWORKING_MAX_ACTION_STREAMS_PER_SECOND 10
	#define APP_NETWORKING_CLIENT_SEND_MAX_BURST_STREAMS_PER_SECOND 25
	#define APP_NETWORKING_SERVER_SEND_MAX_BURST_STREAMS_PER_SECOND 25
	#define APP_NETWORKING_ACTION_BUFFER_SIZE 1024*1024
	#define APP_NETWORKING_BURST_BUFFER_MAXIMUM_SIZE 508

// Renderer
	#define APP_RENDER_SECONDARY_IN_ANOTHER_THREAD
