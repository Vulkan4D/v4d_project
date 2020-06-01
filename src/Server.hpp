#pragma once
#include "app.hh"
#include "networking.hh"

namespace app {
	using namespace zapdata;
	using namespace app::networking;
	
	class Server : public v4d::networking::ListeningServer {
		std::atomic<ulong> nextClientId = 1;
	public:
		using ListeningServer::ListeningServer;
		std::string GetAppName() const override {
			return APP_NETWORKING_APPNAME;
		}
		std::string GetVersion() const override {
			return APP_NETWORKING_VERSION;
		}

		v4d::io::SocketPtr burstSocket;

		ulong Authenticate(v4d::data::ReadOnlyStream* authStream) override {
			if (authStream) {
				// auto[username, password, stuff] = zapdata::Auth::ReadFrom(authStream);
				return nextClientId++;
			} else {
				// Anonymous
				return nextClientId++;
			}
		}

		void HandleIncomingBurst(v4d::io::Socket& socket) {
			BURST_ACTION action = socket.Read<BURST_ACTION>();
			switch (action) {
				case BURST_ACTION::QUIT:default:break;
				
				case BURST_ACTION::MODULE:{
					v4d::modular::ModuleID moduleID(socket.Read<uint64_t>(), socket.Read<uint64_t>());
					auto module = V4D_Server::GetModule(moduleID.String());
					if (module && module->ReceiveBurst) {
						module->ReceiveBurst(socket);
					}
				}break;
				
			}
		}
		
		void HandleIncomingAction(v4d::io::Socket& socket) {
			ACTION action = socket.Read<ACTION>();
			switch (action) {
				case ACTION::QUIT:default:break;
				
				case ACTION::MODULE:{
					v4d::modular::ModuleID moduleID(socket.Read<uint64_t>(), socket.Read<uint64_t>());
					auto module = V4D_Server::GetModule(moduleID.String());
					if (module && module->ReceiveAction) {
						module->ReceiveAction(socket);
					}
				}break;
				
				case ACTION::BURST:
					HandleIncomingBurst(socket);
				break;
				
			}
		}
		
		void Start(uint16_t port) override {
			// Main listening thread
			ListeningServer::Start(port);
			
			// Incoming Stream listener
			burstSocket = std::make_shared<v4d::io::Socket>(v4d::io::UDP);
			burstSocket->Bind(app::networking::serverPort);
			burstSocket->StartListeningThread(10, [this](v4d::io::SocketPtr socket){
				HandleIncomingBurst(*socket);
			});
		}
		
		void RunClient(v4d::io::SocketPtr socket, std::shared_ptr<v4d::networking::IncomingClient>, byte /*clientType*/) override {
			while (socket->IsConnected()) {
				int polled = socket->Poll(APP_NETWORKING_POLL_TIMEOUT_MS);
				if (polled == 0) continue; // timeout, keep going
				if (polled == -1) { // error, stop here
					LOG_ERROR("Server ClientRUN Socket Poll error, disconnecting...")
					break;
				}
				HandleIncomingAction(*socket);
			}
			socket->Disconnect();
		}
		
	};
}
