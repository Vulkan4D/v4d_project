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

std::recursive_mutex actionQueueMutex;
std::unordered_map<uint64_t /* clientID */, std::queue<v4d::data::Stream>> actionQueuePerClient {};

NetworkGameObjectPtr AddNewObject(ModuleID moduleID, NetworkGameObject::Type type, NetworkGameObject::Parent parent = 0) {
	std::lock_guard lock(objectsMutex);
	NetworkGameObjectPtr obj = std::make_shared<NetworkGameObject>(moduleID, type, parent);
	objects[obj->id] = obj;
	obj->active = true;
	return obj;
}

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
	
	V4D_MODULE_FUNC(void, SlowGameLoop) {
		// Erase inactive objects
		std::lock_guard lock(objectsMutex);
		std::vector<uint32_t> objectsToRemove {};
		for (auto& [objID, obj] : objects) {
			if (obj->clientIterations.size() == 0) {
				objectsToRemove.push_back(objID);
			}
		}
		for (auto id : objectsToRemove) {
			objects.erase(id);
		}
	}
	
	V4D_MODULE_FUNC(void, EnqueueAction, v4d::data::Stream& stream, IncomingClientPtr client) {
		std::lock_guard lock(actionQueueMutex);
		actionQueuePerClient[client->id].emplace(stream);
	}
	
	V4D_MODULE_FUNC(void, IncomingClient, IncomingClientPtr client) {
		std::lock_guard lock(objectsMutex);
		auto obj = AddNewObject(THIS_MODULE, OBJECT_TYPE::Player);
		obj->physicsControl = client->id;
		obj->isDynamic = true;
		players[client->id] = obj;
		v4d::data::Stream stream(sizeof(ASSIGN) + sizeof(obj->id));
			stream << ASSIGN;
			stream << obj->id;
		EnqueueAction(stream, client);
	}
	
	V4D_MODULE_FUNC(void, SendActions, v4d::io::SocketPtr stream, IncomingClientPtr client) {
		v4d::data::WriteOnlyStream tmpStream(512);
		{std::lock_guard lock(objectsMutex);
			for (auto& [objID, obj] : objects) {
				if (obj->active) {
					uint32_t& clientIteration = obj->clientIterations[client->id];
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
							
							auto mod = V4D_Objects::GetModule(obj->moduleID.String());
							
							tmpStream.ClearWriteBuffer();
							if (mod && mod->SendStreamCustomObjectData) mod->SendStreamCustomObjectData(obj, tmpStream);
							stream->WriteStream(tmpStream);
							
							tmpStream.ClearWriteBuffer();
							if (mod && mod->SendStreamCustomTransformData) mod->SendStreamCustomTransformData(obj, tmpStream);
							stream->WriteStream(tmpStream);
							
						stream->End();
						clientIteration = obj->iteration;
					}
				} else /* obj is not active */ {
					try {
						uint32_t& clientIteration = obj->clientIterations.at(objID);
						v4d::data::WriteOnlyStream removeStream(8);
						removeStream << REMOVE_OBJECT;
						removeStream << objID;
						EnqueueAction(removeStream, client);
						obj->clientIterations.erase(objID);
					} catch(...) {}
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
		v4d::data::WriteOnlyStream tmpStream(256);
		std::lock_guard lock(objectsMutex);
		for (auto& [objID, obj] : objects) {
			if (obj->active && obj->isDynamic && obj->physicsClientID != client->id) {
				if (obj->iteration == obj->clientIterations[client->id]) {
					stream->Begin();
						*stream << SYNC_OBJECT_TRANSFORM;
						*stream << obj->id;
						*stream << obj->iteration;
						*stream << obj->GetNetworkTransform();
						
						auto mod = V4D_Objects::GetModule(obj->moduleID.String());
						
						tmpStream.ClearWriteBuffer();
						if (mod && mod->SendStreamCustomTransformData) mod->SendStreamCustomTransformData(obj, tmpStream);
						stream->WriteStream(tmpStream);
						
					stream->End();
				}
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
					ball->isDynamic = true;
					ball->physicsClientID = client->id;
				}
				else if (key == "balls") {
					std::lock_guard lock(objectsMutex);
					// Launch 10 balls
					for (int i = 0; i < 10; ++i) {
						auto ball = AddNewObject(THIS_MODULE, OBJECT_TYPE::Ball);
						ball->SetTransform(player->GetWorldPosition() + player->GetLookDirection() * 5.0);
						ball->velocity = player->GetLookDirection()*100.0;
						ball->isDynamic = true;
						ball->physicsClientID = client->id;
					}
				}
				else if (key == "light") {
					std::lock_guard lock(objectsMutex);
					// Launch light
					auto ball = AddNewObject(THIS_MODULE, OBJECT_TYPE::Light);
					ball->SetTransform(player->GetWorldPosition() + player->GetLookDirection() * 5.0);
					ball->velocity = player->GetLookDirection()*40.0;
					ball->isDynamic = true;
					ball->physicsClientID = client->id;
				}
				else if (key == "clear") {
					std::lock_guard lock(objectsMutex);
					for (auto& [objID, obj] : objects) if (obj->physicsClientID == client->id && obj->id != player->id) {
						obj->active = false;
					}
				}
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
				auto tmpStream = stream->ReadStream();
				try {
					std::lock_guard lock(objectsMutex);
					auto obj = objects.at(id);
					if (obj->iteration == iteration) {
						obj->SetTransformFromNetwork(transform);
						
						auto* mod = V4D_Objects::GetModule(obj->moduleID.String());
						if (mod) {
							if (mod->ReceiveStreamCustomTransformData) mod->ReceiveStreamCustomTransformData(obj, tmpStream);
						}
						obj->UpdateObjectInstanceTransform();
					}
				} catch(std::exception& err) {
					LOG_ERROR("Client ReceiveAction UPDATE_OBJECT : " << err.what())
				}
			}break;
		}
	}
	
};