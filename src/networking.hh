#pragma once
#include <v4d.h>

#define APP_NETWORKING_APPNAME "V4D_TESTING"
#define APP_NETWORKING_VERSION "0.0-tests"

namespace app {
	class Server;
	class Client;
}

namespace app::networking {
	std::shared_ptr<app::Server> server = nullptr;
	std::string remoteHost = "";
	int serverPort = 0;
	std::shared_ptr<v4d::crypto::RSA> serverRsaKey = nullptr;
	
	enum class ACTION : byte {
		QUIT=0,
		MODULE=1,
		BURST=2,
	};
	
	enum class BURST_ACTION : byte {
		QUIT=0,
		MODULE=1,
	};
}

// namespace v4d::networking::ZAP::data {
// 	ZAPDATA( Auth, 
// 		String username;
// 		String password;
// 		Vector<Int32> stuff;
// 	)
// }
