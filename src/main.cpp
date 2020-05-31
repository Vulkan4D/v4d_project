#define APPLICATION_NAME "V4D Test"
#define WINDOW_TITLE "TEST"
#define APPLICATION_VERSION_MAJOR 1
#define APPLICATION_VERSION_MINOR 0
#define APPLICATION_VERSION_PATCH 0

#include <v4d.h>
#include "app.hh"
#include "networking.hh"
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

app::ServerPtr server = nullptr;

std::string serverIP = "127.0.0.1";
int serverPort = 8881;

// Cryptography
v4d::crypto::RSA* serverRsaKey = nullptr;

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
		if (!serverRsaKey) {
			serverRsaKey = new v4d::crypto::RSA(2048, 3);
		}
		server = std::make_shared<app::Server>(v4d::io::TCP, serverRsaKey);
		app::modules::InitServer(server);
		server->Start(serverPort);
		if (!app::isClient) LOG("Server has started listening")
	}
}

void app::Stop() {
	app::isRunning = false;
	
	if (server) {
		server->Stop();
		if (serverRsaKey) delete serverRsaKey;
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

	LOG("\n\nApplication terminated\n\n");
}

void app::Run() {
	
	// Client
	if (app::isClient) {// client connects to server
		auto rsaPublicKey = v4d::crypto::RSA::FromPublicKeyPEM(app::Client{v4d::io::TCP}.GetServerPublicKey(serverIP, serverPort));
		app::ClientPtr client = std::make_shared<app::Client>(v4d::io::TCP, &rsaPublicKey);
		if (client->Connect(serverIP, serverPort, 1/*ClientType*/)) {
			app::modules::InitClient(client);
			if (!server) LOG_SUCCESS("Connected to remote server")
			
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
		
		client->Disconnect();
		if (!server) LOG("Disconnected from remote server")
	}
	
	// Wait for Server to terminate
	if (server) {
		if (app::isClient) {
			server->Stop();
		} else {
			// Wait for server to finish
			while (app::IsRunning()) {
				//...
				SLEEP(10ms)
			}
		}
	}
}

int main(const int argc, const char** argv) {
	//TODO handle command line arguments
	
	// Load settings.ini
	app::settings->Load();
	
	//TODO Load serverRsaKey
	
	// set main thread to run only on core 0
	SET_CPU_AFFINITY(0)
	
	// Load V4D Core
	if (!v4d::Init()) return -1;
	
	app::Start();
	app::Run();
	app::Stop();
}
