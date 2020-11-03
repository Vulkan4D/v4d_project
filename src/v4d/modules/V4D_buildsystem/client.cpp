#define _V4D_MODULE
#include <v4d.h>
#include "common.hh"
#include "actions.hh"

using namespace v4d::scene;
using namespace v4d::networking;
using namespace v4d::modular;

std::shared_ptr<OutgoingConnection> client = nullptr;
Scene* scene = nullptr;

std::recursive_mutex actionQueueMutex;
std::queue<v4d::data::Stream> actionQueue {};

V4D_MODULE_CLASS(V4D_Client) {
	
	V4D_MODULE_FUNC(void, Init, std::shared_ptr<OutgoingConnection> _c, Scene* _s) {
		client = _c;
		scene = _s;
	}
	
	V4D_MODULE_FUNC(void, EnqueueAction, v4d::data::WriteOnlyStream& stream) {
		std::lock_guard lock(actionQueueMutex);
		actionQueue.emplace(stream);
	}
	
	V4D_MODULE_FUNC(void, SendActions, v4d::io::SocketPtr stream) {
		std::lock_guard lock(actionQueueMutex);
		while (actionQueue.size()) {
			stream->Begin();
				stream->EmplaceStream(actionQueue.front());
			stream->End();
			actionQueue.pop();
		}
	}
	
};
