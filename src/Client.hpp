#pragma once
#include "app.hh"
#include "networking.hh"

namespace app {
	using namespace zapdata;
	using namespace app::networking;
	
	class Client : public v4d::networking::OutgoingConnection {
	public:
		using OutgoingConnection::OutgoingConnection;
		std::string GetAppName() const override {
			return APP_NETWORKING_APPNAME;
		}
		std::string GetVersion() const override {
			return APP_NETWORKING_VERSION;
		}

		v4d::io::SocketPtr burstSocket;
		v4d::io::SOCKET_TYPE burstSocketType = v4d::io::UDP;

		void Authenticate(v4d::data::Stream* authStream) override {
			// *authStream << zapdata::Auth{"bob", "12345", {4,16,512}};
		}
		
		void HandleIncomingBurst(v4d::io::Socket& socket) {
			BURST_ACTION action = socket.Read<BURST_ACTION>();
			switch (action) {
				case BURST_ACTION::QUIT:default:break;
				
				case BURST_ACTION::MODULE:{
					v4d::modular::ModuleID moduleID(socket.Read<uint64_t>(), socket.Read<uint64_t>());
					auto module = V4D_Client::GetModule(moduleID.String());
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
					auto module = V4D_Client::GetModule(moduleID.String());
					if (module && module->ReceiveAction) {
						module->ReceiveAction(socket);
					}
				}break;
				
			}
		}
		
		void Run(v4d::io::Socket& socket) override {
			burstSocket = std::make_shared<v4d::io::Socket>(burstSocketType);
			burstSocket->Bind(app::networking::serverPort);
			burstSocket->StartListeningThread(10, [this](v4d::io::SocketPtr socket){
				HandleIncomingBurst(*socket);
			});
			
			while (socket.IsConnected()) {
				int polled = socket.Poll(APP_NETWORKING_POLL_TIMEOUT_MS);
				if (polled == 0) continue; // timeout, keep going
				if (polled == -1) { // error, stop here
					LOG_ERROR("Client RUN Socket Poll error, disconnecting...")
					break;
				}
				HandleIncomingAction(socket);
			}
			socket.Disconnect();
		}

	};
}
