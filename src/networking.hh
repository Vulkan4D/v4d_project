#pragma once
#include <v4d.h>
#include "networkActions.hh"

#define APP_NETWORKING_APPNAME "V4D_TESTING"
#define APP_NETWORKING_VERSION "0.0-tests"

namespace app {
	class Server;
	class Client;
	#ifdef APP_ENABLE_BURST_STREAMS
		class BurstServer;
		class BurstClient;
	#endif
}

namespace app::networking {
	std::shared_ptr<app::Server> server = nullptr;
	#ifdef APP_ENABLE_BURST_STREAMS
		std::shared_ptr<app::BurstServer> burstServer = nullptr;
	#endif
	std::string remoteHost = "";
	int serverPort = 0;
	std::shared_ptr<v4d::crypto::RSA> serverRsaKey = nullptr;
	
	enum class ACTION : byte {
		QUIT=0,
		MODULE=1,
		BURST=2,
	};
	
	#ifdef APP_ENABLE_BURST_STREAMS
		enum class BURST_ACTION : byte {
			QUIT=0,
			MODULE=1,
			INIT=2,
		};
	#endif
	
	namespace CLIENT_TYPE {
		byte INITIAL = 1;
		byte BURST = 2;
	}
	
}
