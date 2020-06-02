#define _V4D_MODULE
#include <v4d.h>
using namespace v4d::networking;

std::shared_ptr<OutgoingConnection> client = nullptr;
v4d::scene::Scene* scene = nullptr;

V4D_MODULE_CLASS(V4D_Client) {
	
	V4D_MODULE_FUNC(void, Init, std::shared_ptr<OutgoingConnection> _c, v4d::scene::Scene* _s) {
		client = _c;
		scene = _s;
	}
	
	V4D_MODULE_FUNC(void, SendActions, std::vector<v4d::data::Stream>& streams) {
		
	}
	
	V4D_MODULE_FUNC(void, SendBursts, std::vector<v4d::data::Stream>& streams) {
		
	}
	
	V4D_MODULE_FUNC(void, ReceiveAction, v4d::data::ReadOnlyStream& stream) {
		
	}
	
	V4D_MODULE_FUNC(void, ReceiveBurst, v4d::data::ReadOnlyStream& stream) {
		
	}
	
};
