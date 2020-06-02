#define _V4D_MODULE
#include <v4d.h>
using namespace v4d::networking;

std::shared_ptr<ListeningServer> server = nullptr;
v4d::scene::Scene* scene = nullptr;

V4D_MODULE_CLASS(V4D_Server) {
	
	V4D_MODULE_FUNC(void, Init, std::shared_ptr<ListeningServer> _srv, v4d::scene::Scene* _s) {
		server = _srv;
		scene = _s;
	}
	
	V4D_MODULE_FUNC(void, SendActions, std::vector<v4d::data::Stream>& streams, IncomingClientPtr client) {
		
	}
	
	V4D_MODULE_FUNC(void, SendBursts, std::vector<v4d::data::Stream>& streams, IncomingClientPtr client) {
		
	}
	
	V4D_MODULE_FUNC(void, ReceiveAction, v4d::data::ReadOnlyStream& stream, IncomingClientPtr client) {
		
	}
	
	V4D_MODULE_FUNC(void, ReceiveBurst, v4d::data::ReadOnlyStream& burstStream, IncomingClientPtr client) {
		
	}
	
};
