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
	// LOG_DEBUG("Server AddNewObject " << obj->id)
	return obj;
}

V4D_MODULE_CLASS(V4D_Server) {
	
	V4D_MODULE_FUNC(void, Init, std::shared_ptr<ListeningServer> _srv, Scene* _s) {
		server = _srv;
		scene = _s;
	}
	
	V4D_MODULE_FUNC(void, UnloadScene) {
		std::lock_guard lock(objectsMutex);
		players.clear();
		objects.clear();
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
	
	V4D_MODULE_FUNC(void, EnqueueAction, v4d::data::WriteOnlyStream& stream, IncomingClientPtr client) {
		std::lock_guard lock(actionQueueMutex);
		actionQueuePerClient[client->id].emplace(stream);
	}
	
	V4D_MODULE_FUNC(void, IncomingClient, IncomingClientPtr client) {
		LOG("Server: IncomingClient " << client->id)
		std::scoped_lock lock(objectsMutex, actionQueueMutex);
		auto obj = AddNewObject(THIS_MODULE, OBJECT_TYPE::Player);
		obj->physicsClientID = client->id;
		obj->isDynamic = true;
		players[client->id] = obj;
		// LOG_DEBUG("Server EnqueueAction ASSIGN for obj id " << obj->id << ", client " << client->id)
		v4d::data::WriteOnlyStream stream(sizeof(ASSIGN) + sizeof(obj->id));
			stream << ASSIGN;
			stream << obj->id;
		EnqueueAction(stream, client);
	}
	
	V4D_MODULE_FUNC(void, SendActions, v4d::io::SocketPtr stream, IncomingClientPtr client) {
		v4d::data::WriteOnlyStream tmpStream(CUSTOM_OBJECT_DATA_INITIAL_STREAM_SIZE);
		{std::scoped_lock lock(objectsMutex, actionQueueMutex);
			for (auto& [objID, obj] : objects) {
				if (obj->active) {
					
					uint32_t clientIteration;
					try {
						clientIteration = obj->clientIterations.at(client->id);
					} catch (...) {
						clientIteration = obj->clientIterations[client->id] = 0;
					}
					
					if (obj->iteration > clientIteration) {
						stream->Begin();
							if (clientIteration == 0) {
								// LOG_DEBUG("Server SendAction ADD_OBJECT for obj id " << obj->id << ", client " << client->id)
								// Add
								*stream << ADD_OBJECT;
								*stream << obj->moduleID.vendor;
								*stream << obj->moduleID.module;
								*stream << obj->type;
							} else {
								LOG_DEBUG("Server SendAction UPDATE_OBJECT for obj id " << obj->id << ", client " << client->id)
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
						obj->clientIterations[client->id] = obj->iteration;
					}
				} else /* obj is not active */ {
					try {
						uint32_t& clientIteration = obj->clientIterations.at(client->id);
						v4d::data::WriteOnlyStream removeStream(8);
							removeStream << REMOVE_OBJECT;
							removeStream << objID;
						EnqueueAction(removeStream, client);
						obj->clientIterations.erase(client->id);
					} catch(...) {}// NO ERROR HERE, it's normal that the object has already been removed for this client
				}
			}
			
			auto& actionQueue = actionQueuePerClient[client->id];
			while (actionQueue.size()) {
				// LOG_DEBUG("Server SendActionFromQueue for client " << client->id)
				stream->Begin();
					stream->EmplaceStream(actionQueue.front());
				stream->End();
				actionQueue.pop();
			}
		}
	}
	
	V4D_MODULE_FUNC(void, SendBursts, v4d::io::SocketPtr stream, IncomingClientPtr client) {
		v4d::data::WriteOnlyStream tmpStream(CUSTOM_OBJECT_TRANSFORM_DATA_MAX_STREAM_SIZE);
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
						DEBUG_ASSERT_WARN(tmpStream.GetWriteBufferSize() <= CUSTOM_OBJECT_TRANSFORM_DATA_MAX_STREAM_SIZE, "V4D_Server::SendBursts for module '" << mod->ModuleName() << "', CustomTransformData for Object type " << obj->type << " stream size was " << tmpStream.GetWriteBufferSize() << " bytes, but should be at most " << CUSTOM_OBJECT_TRANSFORM_DATA_MAX_STREAM_SIZE << " bytes")
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
					auto dir = stream->Read<DVector3>();
					std::lock_guard lock(objectsMutex);
					// Launch ball
					auto ball = AddNewObject(THIS_MODULE, OBJECT_TYPE::Ball);
					ball->SetTransform(player->GetWorldPosition() + glm::dvec3{dir.x, dir.y, dir.z} * 5.0);
					ball->velocity = glm::dvec3{dir.x, dir.y, dir.z}*40.0;
					ball->isDynamic = true;
					ball->physicsClientID = client->id;
				}
				else if (key == "balls") {
					std::lock_guard lock(objectsMutex);
					auto dir = stream->Read<DVector3>();
					// Launch 10 balls
					for (int i = 0; i < 10; ++i) {
						auto ball = AddNewObject(THIS_MODULE, OBJECT_TYPE::Ball);
						ball->SetTransform(player->GetWorldPosition() + glm::dvec3{dir.x, dir.y, dir.z} * 5.0);
						ball->velocity = glm::dvec3{dir.x, dir.y, dir.z}*100.0;
						ball->isDynamic = true;
						ball->physicsClientID = client->id;
					}
				}
				else if (key == "light") {
					auto dir = stream->Read<DVector3>();
					std::lock_guard lock(objectsMutex);
					// Launch light
					auto ball = AddNewObject(THIS_MODULE, OBJECT_TYPE::Light);
					ball->SetTransform(player->GetWorldPosition() + glm::dvec3{dir.x, dir.y, dir.z} * 5.0);
					ball->velocity = glm::dvec3{dir.x, dir.y, dir.z}*40.0;
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
		
			default: 
				LOG_ERROR("Server ReceiveAction UNRECOGNIZED MODULE ACTION " << std::to_string((int)action))
			break;
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
					LOG_ERROR("Server ReceiveAction SYNC_OBJECT_TRANSFORM : " << err.what())
				}
			}break;
			
			default: 
				LOG_ERROR("Server ReceiveBurst UNRECOGNIZED MODULE ACTION " << std::to_string((int)action))
			break;
		}
	}
	
};
