#pragma once
#include "app.hh"
#include "networking.hh"

namespace app {
	using namespace zapdata;
	
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

		ulong Authenticate(v4d::data::ReadOnlyStream* authStream) override {
			if (authStream) {
				// auto[username, password, stuff] = zapdata::Auth::ReadFrom(authStream);
				return nextClientId++;
			} else {
				// Anonymous
				return nextClientId++;
			}
		}

		void RunClient(v4d::io::SharedSocket, std::shared_ptr<v4d::networking::IncomingClient>, byte /*clientType*/) override {
			LOG_VERBOSE("[SERVER] IncomingClient is running...")
		}
		
	};
}
