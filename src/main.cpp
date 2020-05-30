#define APPLICATION_NAME "V4D Test"
#define WINDOW_TITLE "TEST"
#define APPLICATION_VERSION_MAJOR 1
#define APPLICATION_VERSION_MINOR 0
#define APPLICATION_VERSION_PATCH 0

#include <v4d.h>
#include "globalscope.hh"
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
	//TODO handle command line arguments
	
	// Load settings.ini
	app::settings->Load();
	
	// set main thread to run only on core 0
	SET_CPU_AFFINITY(0)
	
	// Load V4D Core
	if (!v4d::Init()) return -1;
	
	// Bind Events
	app::events::Bind();
	
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
	
	/////////////////////////////////////
	// Application is ready to loop here
	/////////////////////////////////////
	{
		const std::string serverIP = "127.0.0.1";
		const int serverPort = 8881;
		
		v4d::crypto::RSA* serverRSA = nullptr;
		app::Server* server = nullptr;
		// Server
		if (app::isServer) {// server starts listening
			serverRSA = new v4d::crypto::RSA(2048, 3);
			server = new app::Server(v4d::io::TCP, serverRSA);
			server->Start(serverPort);
			if (!app::isClient) LOG("Server has started listening...")
		}
		
		// Client
		if (app::isClient) {// client connects to server
			auto rsaPublicKey = v4d::crypto::RSA::FromPublicKeyPEM(app::Client{v4d::io::TCP}.GetServerPublicKey(serverIP, serverPort));
			app::Client client(v4d::io::TCP, &rsaPublicKey);
			if (client.Connect(serverIP, serverPort, 1/*ClientType*/)) {
				if (!server) {
					LOG_SUCCESS("Connected to remote server")
				}
				
				// Game Loops
				if (app::renderer) {
					app::SlowGameLoop slowGameLoop(app::IsRunning, 0);
					app::GameLoop gameLoop(app::IsRunning, 1);
					app::RenderingLoop renderingLoop(app::IsRunning, 2);
					app::input::UpdateLoop([](){return app::window->IsActive();});
					app::isRunning = false;
				} else {
					// No graphics, still a client (compute cluster, admin console,...)
					//...
				}
			} else if (server) {
				LOG_ERROR("Failed to connect to local server (SOLO MODE)")
			} else {
				LOG_ERROR("Failed to connect to remote server " << serverIP << " on port " << serverPort)
			}
		}
		
		// Server
		if (server) {
			// server stops listening
			delete server;
			if (!app::isClient) LOG("Server has stopped listening")
		}
	}
	///////////////////////////////////
	// Application is terminating here
	///////////////////////////////////
	
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

	LOG("\n\nApplication terminated\n\n");
}
