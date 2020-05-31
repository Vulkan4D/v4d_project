#pragma once
#include "app.hh"
#include "networking.hh"

namespace app {
	using namespace zapdata;
	
	class Client : public v4d::networking::OutgoingConnection {
	public:
		using OutgoingConnection::OutgoingConnection;
		std::string GetAppName() const override {
			return APP_NETWORKING_APPNAME;
		}
		std::string GetVersion() const override {
			return APP_NETWORKING_VERSION;
		}

		void Authenticate(v4d::data::Stream* authStream) override {
			// *authStream << zapdata::Auth{"bob", "12345", {4,16,512}};
		}
		
		void Run(v4d::io::Socket& /*socket*/) override {
			LOG_VERBOSE("[CLIENT] Client is running...")
		}

	};
	
	typedef std::shared_ptr<Client> ClientPtr;
}
