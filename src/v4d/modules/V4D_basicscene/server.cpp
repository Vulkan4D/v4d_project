#define _V4D_MODULE
#include <v4d.h>
#include "common.hh"
#include "networkActions.hh"

using namespace v4d::scene;
using namespace v4d::networking;
using namespace app::networking::action;
using namespace v4d::modular;

std::shared_ptr<ListeningServer> server = nullptr;
Scene* scene = nullptr;
std::recursive_mutex objectsMutex;
std::unordered_map<uint32_t /* objectID */, NetworkGameObjectPtr> objects {}; // also includes all players
std::unordered_map<uint64_t /* clientID */, NetworkGameObjectPtr> players {};

std::mutex actionQueueMutex;
std::unordered_map<uint64_t /* clientID */, std::queue<v4d::data::Stream>> actionQueuePerClient {};
// std::mutex objectsToRemoveMutex;
// std::queue<uint32_t> objectsToRemove {};

NetworkGameObjectPtr AddNewObject(ModuleID moduleID, NetworkGameObject::Type type, NetworkGameObject::Parent parent = 0) {
	std::lock_guard lock(objectsMutex);
	NetworkGameObjectPtr obj = std::make_shared<NetworkGameObject>(moduleID, type, parent);
	objects[obj->id] = obj;
	return obj;
}

// void EnqueueObjectToRemove(uint32_t objectID) {
// 	std::lock_guard lock(objectsToRemoveMutex);
// 	objectsToRemove.emplace(objectID);
// }

V4D_MODULE_CLASS(V4D_Server) {
	
	V4D_MODULE_FUNC(void, Init, std::shared_ptr<ListeningServer> _srv, Scene* _s) {
		server = _srv;
		scene = _s;
		
		// Cornell boxes
		AddNewObject(THIS_MODULE, OBJECT_TYPE::CornellBox)
			->SetTransform({0,250,-30}, 180.0);
		AddNewObject(THIS_MODULE, OBJECT_TYPE::CornellBox)
			->SetTransform({200,250,-30}, 120.0);
		AddNewObject(THIS_MODULE, OBJECT_TYPE::CornellBox)
			->SetTransform({-200,250,-30}, -120.0);
		for (int i = 0; i < 100; ++i)
			AddNewObject(THIS_MODULE, OBJECT_TYPE::CornellBox)
				->SetTransform({0,500,-30 + (i*90)}, 180.0);
	}
	
	V4D_MODULE_FUNC(void, EnqueueAction, v4d::data::Stream& stream, IncomingClientPtr client) {
		std::lock_guard lock(actionQueueMutex);
		actionQueuePerClient[client->id].emplace(stream);
	}
	
	V4D_MODULE_FUNC(void, IncomingClient, IncomingClientPtr client) {
		std::lock_guard lock(objectsMutex);
		auto obj = AddNewObject(THIS_MODULE, OBJECT_TYPE::Player);
		obj->physicsControl = client->id;
		players[client->id] = obj;
		v4d::data::Stream stream(sizeof(ASSIGN) + sizeof(obj->id));
			stream << ASSIGN;
			stream << obj->id;
		EnqueueAction(stream, client);
	}
	
	V4D_MODULE_FUNC(void, SendActions, v4d::io::SocketPtr stream, IncomingClientPtr client) {
		{std::lock_guard lock(objectsMutex);
			for (auto& [objID, obj] : objects) {
				uint32_t clientIteration = obj->clientIterations[client->id];
				if (obj->iteration > clientIteration) {
					stream->Begin();
						if (clientIteration == 0) {
							// Add
							*stream << ADD_OBJECT;
							*stream << obj->moduleID.vendor;
							*stream << obj->moduleID.module;
							*stream << obj->type;
						} else {
							// Update
							*stream << UPDATE_OBJECT;
						}
						*stream << obj->parent;
						*stream << obj->id;
						*stream << (obj->physicsClientID == client->id);
						*stream << obj->GetAttributes();
						*stream << obj->iteration;
						*stream << obj->GetNetworkTransform();
						
						//TODO fix this... will crash when (obj->iteration != clientIteration)
						auto mod = V4D_Objects::GetModule(obj->moduleID.String());
						if (mod && mod->SendStreamCustomObjectData) {
							mod->SendStreamCustomObjectData(obj, stream);
						}
						if (mod && mod->SendStreamCustomTransformData) {
							mod->SendStreamCustomTransformData(obj, stream);
						}
					stream->End();
					obj->clientIterations[client->id] = obj->iteration;
				}
			}
		}
		std::lock_guard lock(actionQueueMutex);
		auto& actionQueue = actionQueuePerClient[client->id];
		while (actionQueue.size()) {
			stream->Begin();
				*stream << actionQueue.front();
				actionQueue.pop();
			stream->End();
		}
	}
	
	V4D_MODULE_FUNC(void, SendBursts, v4d::io::SocketPtr stream, IncomingClientPtr client) {
		std::lock_guard lock(objectsMutex);
		for (auto& [objID, obj] : objects) {
			if (obj->iteration == obj->clientIterations[client->id]) {
				stream->Begin();
					*stream << SYNC_OBJECT_TRANSFORM;
					*stream << obj->id;
					*stream << obj->iteration;
					*stream << obj->GetNetworkTransform();
					
					//TODO fix this... will crash when (obj->iteration != clientIteration)
					auto mod = V4D_Objects::GetModule(obj->moduleID.String());
					if (mod && mod->SendStreamCustomTransformData) {
						mod->SendStreamCustomTransformData(obj, stream);
					}
				stream->End();
			}
		}
	}
	
	V4D_MODULE_FUNC(void, ReceiveAction, v4d::io::SocketPtr stream, IncomingClientPtr client) {
		auto action = stream->Read<app::networking::Action>();
		switch (action) {
			
			case CUSTOM:{
				auto key = stream->Read<std::string>();
				NetworkGameObjectPtr player = players.at(client->id);
				
				if (key == "ball") {
					std::lock_guard lock(objectsMutex);
					// Launch ball
					auto ball = AddNewObject(THIS_MODULE, OBJECT_TYPE::Ball);
					ball->SetTransform(player->GetWorldPosition() + player->GetLookDirection() * 5.0);
					ball->velocity = player->GetLookDirection()*40.0;
				}
				else if (key == "balls") {
					std::lock_guard lock(objectsMutex);
					// Launch 10 balls
					for (int i = 0; i < 10; ++i) {
						auto ball = AddNewObject(THIS_MODULE, OBJECT_TYPE::Ball);
						ball->SetTransform(player->GetWorldPosition() + player->GetLookDirection() * 5.0);
						ball->velocity = player->GetLookDirection()*100.0;
					}
				}
				else if (key == "light") {
					std::lock_guard lock(objectsMutex);
					// Launch light
					auto ball = AddNewObject(THIS_MODULE, OBJECT_TYPE::Light);
					ball->SetTransform(player->GetWorldPosition() + player->GetLookDirection() * 5.0);
					ball->velocity = player->GetLookDirection()*40.0;
				}
				// else if (key == "clear") {
				// 	std::lock_guard lock(objectsMutex);
				// 	for (auto& [objID, obj] : objects) if (obj->physicsClientID == client->id && obj->id != player->id) {
				// 		EnqueueObjectToRemove(obj->id);
				// 		scene->RemoveObjectInstance(obj->objectInstance);
				// 	}
				// }
			}break;
			
		}
	}
	
	V4D_MODULE_FUNC(void, ReceiveBurst, v4d::io::SocketPtr stream, IncomingClientPtr client) {
		auto action = stream->Read<app::networking::Action>();
		switch (action) {
			case SYNC_OBJECT_TRANSFORM:{
				auto id = stream->Read<uint32_t>();
				auto iteration = stream->Read<uint32_t>();
				auto transform = stream->Read<NetworkGameObjectTransform>();
				try {
					std::lock_guard lock(objectsMutex);
					auto obj = objects.at(id);
					if (obj->iteration == iteration) {
						obj->SetTransformFromNetwork(transform);
						
						//TODO fix this... will crash when (obj->iteration != iteration)
						auto* mod = V4D_Objects::GetModule(obj->moduleID.String());
						if (mod && mod->ReceiveStreamCustomTransformData) {
							mod->ReceiveStreamCustomTransformData(obj, stream);
						}
						
						obj->UpdateObjectInstanceTransform();
					}
				} catch(...) {
					LOG_ERROR("Client ReceiveAction UPDATE_OBJECT")
				}
			}break;
		}
	}
	
};
