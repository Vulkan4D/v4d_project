#define APPLICATION_NAME "V4D Test"
#define WINDOW_TITLE "TEST"
#define APPLICATION_VERSION_MAJOR 1
#define APPLICATION_VERSION_MINOR 0
#define APPLICATION_VERSION_PATCH 0

#include <v4d.h>
#include "app.hh"
#include "networking.hh"
#include "crypto.hh"
#include "Server.hpp"
#include "Client.hpp"
#include "events.hpp"
#include "modules.hpp"
#include "input.hpp"
#include "vulkan.hpp"
#include "graphics.hpp"
#include "GameLoop.hpp"
#include "SlowGameLoop.hpp"
#include "RenderingLoop.hpp"

int main(const int argc, const char** argv) {
	// handle command line arguments
	if (argc > 1) for (int i = 1; i < argc; ++i) {
		if (*argv[i] == '-') {
			app::ARG arg = app::Arg(std::string(argv[i]+1));
			auto nextArg = [&i, &argc, &argv](){
				return ++i < argc? argv[i] : "";
			};
			switch (arg) {
				case app::ARG::server: {
					app::isServer = true;
					app::hasGraphics = false;
				break;}
				case app::ARG::client: {
					app::isClient = true;
				break;}
				case app::ARG::host: {
					app::isServer = false;
					app::isClient = true;
					app::networking::remoteHost = nextArg();
				break;}
				case app::ARG::port: {
					app::networking::serverPort = atoi(nextArg());
				break;}
				case app::ARG::invalid_arg:default:{
					LOG_ERROR("Invalid option: " << std::string(argv[i]))
				break;}
			}
		} else {
			app::modulesList.push_back(argv[i]);
		}
	}
	
	// Load settings.ini
	app::settings->Load();
	
	// Default networking settings
	if (app::networking::remoteHost == "")
		app::networking::remoteHost = APP_NETWORKING_DEFAULT_HOST;
	if (app::networking::serverPort == 0)
		app::networking::serverPort = app::settings->default_server_port > 0 ? 
			app::settings->default_server_port : APP_NETWORKING_DEFAULT_PORT;
	
	// when no arguments defining server or client, use Solo mode
	#ifdef APP_ENABLE_SOLO
		if (!app::isClient && !app::isServer) {
			app::isClient = true;
			app::isServer = true;
		}
	#endif
	
	// Load serverRsaKey
	#ifdef APP_NETWORKING_USE_RSA_KEY
		serverRsaKey = app::crypto::LoadOrCreateServerPrivateKey();
	#endif
	
	// Load V4D Core
	if (!v4d::Init()) return -1;
	
	app::Start();
	app::Run();
	app::Stop();
}

void app::Start() {
	// Bind Events
	app::events::Bind();
	v4d::event::APP_KILLED << [](int){app::Stop();};
	v4d::event::APP_ERROR << [](int){app::Stop();};
	
	// Load Modules
	app::modules::Load();

	// Load Vulkan driver
	if (app::hasGraphics) {
		app::vulkan::Load();
	}
	
	// Load Graphics
	if (app::vulkanLoader) {
		app::graphics::CreateWindow();
		app::graphics::LoadUi();
		app::graphics::CreateRenderer();
	}
	
	// Load scene
	app::scene = new v4d::scene::Scene;
	app::modules::Init();
	app::modules::LoadScene();
	
	// Load Inputs
	if (app::window) {
		app::input::AddCallbacks();
	}
	
	// Load Renderer
	if (app::renderer) {
		app::graphics::LoadRenderer();
	}
	
	// Server
	if (app::isServer) {// server starts listening
		app::networking::server = std::make_shared<app::Server>(v4d::io::TCP, app::networking::serverRsaKey.get());
		app::modules::InitServer(app::networking::server);
		app::networking::server->Start(app::networking::serverPort);
		if (!app::isClient) LOG("Server has started listening")
	}
}

void app::Stop() {
	app::isRunning = false;
	
	if (app::networking::server) {
		app::networking::server->Stop();
		if (!app::isClient) LOG("Server has stopped listening")
		if (app::networking::serverRsaKey) app::networking::serverRsaKey.reset();
	}
	
	// Unload Renderer
	if (app::renderer) {
		app::renderer->UnloadRenderer();
	}
	
	// // Unload Inputs
	if (app::window) {
		app::input::RemoveCallbacks();
	}
	
	// Unload scene
	app::modules::UnloadScene();
	app::modules::Unload();
	app::scene->ClearAllRemainingObjects();
	delete app::scene;
	
	// Unload Graphics
	if (app::renderer) {
		app::graphics::UnloadUi();
		app::graphics::DestroyRenderer();
		app::graphics::DestroyWindow();
	}
	
	// Unload Vulkan
	if (app::vulkanLoader) {
		app::vulkan::Unload();
	}

	LOG("\nApplication terminated\n");
}

void app::Run() {
	
	// set main thread & input to run only on a specific core
	SET_CPU_AFFINITY(APP_CPU_AFFINITY_MAIN)
	
	// Client
	if (app::isClient) {// client connects to server
		v4d::crypto::RSA rsaPublicKey = app::crypto::GetServerPublicKey(app::networking::remoteHost, app::networking::serverPort);
		std::shared_ptr<app::Client> client = std::make_shared<app::Client>(v4d::io::TCP, rsaPublicKey.GetSize()? &rsaPublicKey:nullptr);
		app::modules::InitClient(client);
		client->SetAsync();
		
		// Force TCP for burst socket if host is localhost (SOLO or Testing) otherwise the UDP listener with conflict between client and server
		if ((app::isClient && app::isServer) || app::networking::remoteHost == "127.0.0.1" || app::networking::remoteHost == "localhost") {
			client->burstSocketType = v4d::io::TCP;
		}
		
		// Connect to server
		if (client->Connect(app::networking::remoteHost, app::networking::serverPort, 1/*ClientType*/)) {
			if (!app::networking::server) LOG_SUCCESS("Connected to remote server")
			
			// Game Loops
			if (app::renderer) {
				app::SlowGameLoop slowGameLoop(app::IsRunning, APP_CPU_AFFINITY_MAIN);
				app::GameLoop gameLoop(app::IsRunning, APP_CPU_AFFINITY_GAME);
				app::RenderingLoop renderingLoop(app::IsRunning, APP_CPU_AFFINITY_RENDER_PRIMARY, APP_CPU_AFFINITY_RENDER_SECONDARY);
				app::input::UpdateLoop([](){return app::window->IsActive();});
				app::isRunning = false;
			} else {
				// No graphics, still a client (compute cluster, admin console,...)
				//...
			}
		} else if (app::networking::server) {
			LOG_ERROR("Failed to connect to local server (SOLO MODE)")
		} else {
			LOG_ERROR("Failed to connect to remote server " << app::networking::remoteHost << " on port " << app::networking::serverPort)
		}
		
		client->Disconnect();
	}
	
	// Wait for Server to terminate
	if (app::networking::server) {
		if (app::isClient) {
			app::networking::server->Stop();
		} else {
			// Wait for server to finish listening
			while (app::IsRunning()) {
				//...
				SLEEP(10ms)
			}
		}
	}
}
