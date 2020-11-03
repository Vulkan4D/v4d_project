#define _V4D_MODULE
#include <v4d.h>
#include "common.hh"
#include "actions.hh"
#include "../V4D_multiplayer/ServerSideObjects.hh"

using namespace v4d::scene;
using namespace v4d::modular;
using namespace networking::actions;

std::shared_ptr<ListeningServer> server = nullptr;
Scene* scene = nullptr;
V4D_Server* mainModule = nullptr;

ServerSideObjects* serverSideObjects = nullptr;
CachedData* cachedData = nullptr;

V4D_MODULE_CLASS(V4D_Server) {
	
	V4D_MODULE_FUNC(void, Init, std::shared_ptr<ListeningServer> _srv, Scene* _s) {
		server = _srv;
		scene = _s;
		mainModule = V4D_Server::GetPrimaryModule();
		serverSideObjects = (ServerSideObjects*)mainModule->ModuleGetCustomPtr(0);
		cachedData = (CachedData*)V4D_Objects::LoadModule(THIS_MODULE)->ModuleGetCustomPtr(0);
	}
	
	V4D_MODULE_FUNC(void, ReceiveAction, v4d::io::SocketPtr stream, IncomingClientPtr client) {
		NetworkGameObjectPtr player = serverSideObjects->players.at(client->id);
		auto action = stream->Read<Action>();
		switch (action) {
			
			case CREATE_NEW_BUILD:{
				// Network data
				auto transform = stream->Read<NetworkGameObjectTransform>();
				auto block = stream->Read<Block>();
				std::scoped_lock lock(serverSideObjects->mutex, cachedData->serverObjectMapsMutex);
				auto obj = serverSideObjects->Add(THIS_MODULE, OBJECT_TYPE::Build);
				obj->SetTransformFromNetwork(transform);
				obj->isDynamic = true;
				obj->physicsClientID = client->id;
				cachedData->serverBuildBlocks[obj->id].push_back(block);
			}break;
		
			default: 
				LOG_ERROR("Server ReceiveAction UNRECOGNIZED MODULE ACTION " << std::to_string((int)action))
			break;
		}
	}
	
};
