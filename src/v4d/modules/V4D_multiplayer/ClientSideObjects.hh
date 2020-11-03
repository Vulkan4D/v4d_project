#include <v4d.h>

using namespace v4d::scene;
using namespace v4d::networking;

struct ClientSideObjects {
	std::recursive_mutex mutex;
	NetworkGameObjects objects {};
};
