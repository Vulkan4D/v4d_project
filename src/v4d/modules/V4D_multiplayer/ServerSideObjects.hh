#include <v4d.h>

using namespace v4d::scene;
using namespace v4d::networking;
using namespace v4d::modular;

struct ServerSideObjects {
	std::recursive_mutex mutex;
	NetworkGameObjects objects {}; // also includes players
	std::unordered_map<uint64_t /* clientID */, NetworkGameObjectPtr> players {};
	
	NetworkGameObjectPtr Add(ModuleID moduleID, NetworkGameObject::Type type, NetworkGameObject::Parent parent = 0) {
		std::lock_guard lock(mutex);
		NetworkGameObjectPtr obj = std::make_shared<NetworkGameObject>(moduleID, type, parent);
		objects[obj->id] = obj;
		obj->active = true;
		// LOG_DEBUG("Server AddNewObject " << obj->id)
		return obj;
	}
};
