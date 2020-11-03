#define _V4D_MODULE
#include <v4d.h>
#include "common.hh"
#include "networkActions.hh"
#include "../V4D_multiplayer/ServerSideObjects.hh"

using namespace v4d::scene;
using namespace v4d::networking;
using namespace app::networking::action;
using namespace v4d::modular;

std::shared_ptr<ListeningServer> server = nullptr;
Scene* scene = nullptr;
V4D_Server* mainModule = nullptr;

ServerSideObjects* serverSideObjects = nullptr;

V4D_MODULE_CLASS(V4D_Server) {
	
	V4D_MODULE_FUNC(void, Init, std::shared_ptr<ListeningServer> _srv, Scene* _s) {
		server = _srv;
		scene = _s;
		mainModule = V4D_Server::GetPrimaryModule();
		serverSideObjects = (ServerSideObjects*)mainModule->ModuleGetCustomPtr(0);
	}
	
	V4D_MODULE_FUNC(void, ReceiveAction, v4d::io::SocketPtr stream, IncomingClientPtr client) {
		auto action = stream->Read<app::networking::Action>();
		switch (action) {
			
			case CUSTOM:{
				auto key = stream->Read<std::string>();
				NetworkGameObjectPtr player = serverSideObjects->players.at(client->id);
				
				if (key == "newbuild") {
					//...
				}
				
			}break;
		
			default: 
				LOG_ERROR("Server ReceiveAction UNRECOGNIZED MODULE ACTION " << std::to_string((int)action))
			break;
		}
	}
	
};
