#pragma once
#include "globalscope.hh"

namespace app {
	using namespace zapdata;
	
	class Server : public v4d::networking::ListeningServer {
		std::atomic<ulong> nextClientId = 1;
	public:
		using ListeningServer::ListeningServer;

		std::string GetAppName() const override {
			return "V4D";
		}
		std::string GetVersion() const override {
			return "0.0.0";
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
			LOG_VERBOSE("[SERVER] Client is running...")
		}

	};
}
