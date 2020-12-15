#define _V4D_MODULE
#include <v4d.h>
#include "common.hh"
#include "actions.hh"
#include "ServerSideObjects.hh"
#include "ClientSideObjects.hh"
#include "../V4D_flycam/common.hh"

using namespace v4d::scene;
using namespace v4d::networking;
using namespace networking::action;
using namespace v4d::modular;

std::shared_ptr<ListeningServer> server = nullptr;
std::shared_ptr<OutgoingConnection> client = nullptr;
Scene* scene = nullptr;

std::recursive_mutex serverActionQueueMutex;
std::unordered_map<uint64_t /* clientID */, std::queue<v4d::data::Stream>> serverActionQueuePerClient {};

ServerSideObjects serverSideObjects {};
ClientSideObjects clientSideObjects {};

float interpolationSpeed = 15.0;

V4D_MODULE_CLASS(V4D_Mod) {
	
	V4D_MODULE_FUNC(void*, ModuleGetCustomPtr, int which) {
		switch (which) {
			case CUSTOM_PTR_SERVER_OBJECTS: return &serverSideObjects;
			case CUSTOM_PTR_CLIENT_OBJECTS: return &clientSideObjects;
			default: return nullptr;
		}
	}
	
	V4D_MODULE_FUNC(int, OrderIndex) {
		return -1000;
	}
	
	#pragma region Server
	
	V4D_MODULE_FUNC(void, InitServer, std::shared_ptr<ListeningServer> _srv) {
		server = _srv;
	}
	
	V4D_MODULE_FUNC(void, InitClient, std::shared_ptr<OutgoingConnection> _c) {
		client = _c;
	}
	
	V4D_MODULE_FUNC(void, LoadScene, Scene* _s) {
		scene = _s;
	}
	
	V4D_MODULE_FUNC(void, UnloadScene) {
		{
			std::lock_guard lock(serverSideObjects.mutex);
			serverSideObjects.players.clear();
			serverSideObjects.objects.clear();
		}
		{
			std::lock_guard lock(clientSideObjects.mutex);
			clientSideObjects.objects.clear();
		}
	}
	
	V4D_MODULE_FUNC(void, GameLoopUpdate, double deltaTime) {
		// Update gameObject Positions on client-side
		std::lock_guard lock(clientSideObjects.mutex);
		for (auto& [objID, obj] : clientSideObjects.objects) {
			if (obj->active && !obj->physicsControl && obj->posInit) {
				obj->SmoothlyInterpolateGameObjectTransform(deltaTime * interpolationSpeed);
			}
		}
	}
	
	V4D_MODULE_FUNC(void, SlowLoopUpdate, double deltaTime) {
		// Erase inactive gameObjects on server-side
		std::lock_guard lock(serverSideObjects.mutex);
		std::vector<uint32_t> objectsToRemove {};
		for (auto& [objID, obj] : serverSideObjects.objects) {
			if (!obj->active && obj->clientIterations.size() == 0) {
				objectsToRemove.push_back(objID);
			}
		}
		for (auto id : objectsToRemove) {
			serverSideObjects.objects.erase(id);
		}
	}
	
	V4D_MODULE_FUNC(void, DrawUi2) {
		#ifdef _ENABLE_IMGUI
			ImGui::SliderFloat("Network interpolation smooth speed", &interpolationSpeed, 0.0f, 30.0f);
		#endif
	}
	
	V4D_MODULE_FUNC(void, ServerSendActions, v4d::io::SocketPtr stream, IncomingClientPtr client) {
		v4d::data::WriteOnlyStream tmpStream(CUSTOM_OBJECT_DATA_INITIAL_STREAM_SIZE);
		{std::scoped_lock lock(serverSideObjects.mutex, serverActionQueueMutex);
			for (auto& [objID, obj] : serverSideObjects.objects) {
				if (obj->active) {
					
					NetworkGameObject::Iteration clientIteration;
					try {
						clientIteration = obj->clientIterations.at(client->id);
					} catch (...) {
						clientIteration = obj->clientIterations[client->id] = 0;
					}
					
					if (obj->GetIteration() > clientIteration) {
						stream->Begin();
							if (clientIteration == 0) {
								// LOG_DEBUG("Server SendAction ADD_OBJECT for obj id " << obj->id << ", client " << client->id)
								// Add
								*stream << ADD_OBJECT;
								*stream << obj->moduleID.vendor;
								*stream << obj->moduleID.module;
								*stream << obj->type;
							} else {
								// LOG_DEBUG("Server SendAction UPDATE_OBJECT for obj id " << obj->id << ", client " << client->id)
								// Update
								*stream << UPDATE_OBJECT;
							}
							*stream << obj->parent;
							*stream << obj->id;
							*stream << (obj->physicsClientID == client->id);
							*stream << obj->GetAttributes();
							*stream << obj->GetIteration();
							*stream << obj->GetNetworkTransform();
							
							auto mod = V4D_Mod::GetModule(obj->moduleID.String());
							
							tmpStream.ClearWriteBuffer();
							if (mod && mod->SendStreamCustomGameObjectData) mod->SendStreamCustomGameObjectData(obj, tmpStream);
							stream->WriteStream(tmpStream);
							
							tmpStream.ClearWriteBuffer();
							if (mod && mod->SendStreamCustomGameObjectTransformData) mod->SendStreamCustomGameObjectTransformData(obj, tmpStream);
							stream->WriteStream(tmpStream);
							
						stream->End();
						obj->clientIterations[client->id] = obj->GetIteration();
					}
				} else /* obj is not active */ {
					try {
						uint32_t& clientIteration = obj->clientIterations.at(client->id);
						v4d::data::WriteOnlyStream removeStream(8);
							removeStream << REMOVE_OBJECT;
							removeStream << objID;
						serverActionQueuePerClient[client->id].emplace(removeStream);
						obj->clientIterations.erase(client->id);
					} catch(...) {}// NO ERROR HERE, it's normal that the object has already been removed for this client
				}
			}
			
			auto& actionQueue = serverActionQueuePerClient[client->id];
			while (actionQueue.size()) {
				// LOG_DEBUG("Server SendActionFromQueue for client " << client->id)
				stream->Begin();
					stream->EmplaceStream(actionQueue.front());
				stream->End();
				actionQueue.pop();
			}
		}
	}
	
	V4D_MODULE_FUNC(void, ServerSendBursts, v4d::io::SocketPtr stream, IncomingClientPtr client) {
		v4d::data::WriteOnlyStream tmpStream(CUSTOM_OBJECT_TRANSFORM_DATA_MAX_STREAM_SIZE);
		std::lock_guard lock(serverSideObjects.mutex);
		for (auto& [objID, obj] : serverSideObjects.objects) {
			if (obj->active && obj->isDynamic && obj->physicsClientID != client->id) {
				if (obj->GetIteration() == obj->clientIterations[client->id]) {
					stream->Begin();
						*stream << SYNC_OBJECT_TRANSFORM;
						*stream << obj->id;
						*stream << obj->GetIteration();
						*stream << obj->GetNetworkTransform();
						
						auto mod = V4D_Mod::GetModule(obj->moduleID.String());
						
						tmpStream.ClearWriteBuffer();
						if (mod && mod->SendStreamCustomGameObjectTransformData) mod->SendStreamCustomGameObjectTransformData(obj, tmpStream);
						DEBUG_ASSERT_WARN(tmpStream.GetWriteBufferSize() <= CUSTOM_OBJECT_TRANSFORM_DATA_MAX_STREAM_SIZE, "V4D_Server::SendBursts for module '" << mod->ModuleName() << "', CustomTransformData for Object type " << obj->type << " stream size was " << tmpStream.GetWriteBufferSize() << " bytes, but should be at most " << CUSTOM_OBJECT_TRANSFORM_DATA_MAX_STREAM_SIZE << " bytes")
						stream->WriteStream(tmpStream);
						
					stream->End();
				}
			}
		}
	}
	
	V4D_MODULE_FUNC(void, ServerReceiveAction, v4d::io::SocketPtr stream, IncomingClientPtr client) {
		auto action = stream->Read<Action>();
		switch (action) {
			default: 
				LOG_ERROR("Server ReceiveAction UNRECOGNIZED MODULE ACTION " << std::to_string((int)action))
			break;
		}
	}
	
	V4D_MODULE_FUNC(void, ServerReceiveBurst, v4d::io::SocketPtr stream, IncomingClientPtr client) {
		auto action = stream->Read<Action>();
		switch (action) {
			case SYNC_OBJECT_TRANSFORM:{
				auto id = stream->Read<NetworkGameObject::Id>();
				auto iteration = stream->Read<NetworkGameObject::Iteration>();
				auto transform = stream->Read<NetworkGameObjectTransform>();
				auto tmpStream = stream->ReadStream();
				try {
					std::lock_guard lock(serverSideObjects.mutex);
					auto obj = serverSideObjects.objects.at(id);
					if (obj->GetIteration() == iteration) {
						obj->SetTransformFromNetwork(transform);
						
						auto* mod = V4D_Mod::GetModule(obj->moduleID.String());
						if (mod) {
							if (mod->ReceiveStreamCustomGameObjectTransformData) mod->ReceiveStreamCustomGameObjectTransformData(obj, tmpStream);
						}
						
					}
				} catch(std::exception& err) {
					// LOG_ERROR("Server ReceiveAction SYNC_OBJECT_TRANSFORM : " << err.what())
				}
			}break;
			
			default: 
				LOG_ERROR("Server ReceiveBurst UNRECOGNIZED MODULE ACTION " << std::to_string((int)action))
			break;
		}
	}
	
	#pragma endregion
	
	#pragma region Client
	
	V4D_MODULE_FUNC(void, ClientSendBursts, v4d::io::SocketPtr stream) {
		v4d::data::WriteOnlyStream tmpStream(CUSTOM_OBJECT_TRANSFORM_DATA_MAX_STREAM_SIZE);
		std::lock_guard lock(clientSideObjects.mutex);
		for (auto&[objID, obj] : clientSideObjects.objects) {
			if (obj->physicsControl) {
				if (obj->ReverseUpdateGameObjectTransform()) {
					stream->Begin();
						*stream << SYNC_OBJECT_TRANSFORM;
						*stream << obj->id;
						*stream << obj->GetIteration();
						*stream << obj->GetNetworkTransform();
						
						auto mod = V4D_Mod::GetModule(obj->moduleID.String());
						
						tmpStream.ClearWriteBuffer();
						if (mod && mod->SendStreamCustomGameObjectTransformData) mod->SendStreamCustomGameObjectTransformData(obj, tmpStream);
						DEBUG_ASSERT_WARN(tmpStream.GetWriteBufferSize() <= CUSTOM_OBJECT_TRANSFORM_DATA_MAX_STREAM_SIZE, "V4D_Client::SendBursts for module '" << mod->ModuleName() << "', CustomTransformData for Object type " << obj->type << " stream size was " << tmpStream.GetWriteBufferSize() << " bytes, but should be at most " << CUSTOM_OBJECT_TRANSFORM_DATA_MAX_STREAM_SIZE << " bytes")
						stream->WriteStream(tmpStream);
						
					stream->End();
				}
			}
		}
	}
	
	V4D_MODULE_FUNC(void, ClientReceiveAction, v4d::io::SocketPtr stream) {
		auto action = stream->Read<Action>();
		switch (action) {
			case ADD_OBJECT:{
				auto _vendor = stream->Read<typeof ModuleID::vendor>();
				auto _module = stream->Read<typeof ModuleID::module>();
				ModuleID moduleID(_vendor, _module);
				auto type = stream->Read<NetworkGameObject::Type>();
				auto parent = stream->Read<NetworkGameObject::Parent>();
				auto id = stream->Read<NetworkGameObject::Id>();
				auto physicsControl = stream->Read<bool>();
				auto attributes = stream->Read<NetworkGameObject::Attributes>();
				auto iteration = stream->Read<NetworkGameObject::Iteration>();
				auto transform = stream->Read<NetworkGameObjectTransform>();
				auto tmpStream1 = stream->ReadStream();
				auto tmpStream2 = stream->ReadStream();
				if (moduleID.IsValid()) {
					std::lock_guard lock(clientSideObjects.mutex);
					// LOG_DEBUG("Client ReceiveAction ADD_OBJECT for obj id " << id)
					
					auto obj = std::make_shared<NetworkGameObject>(moduleID, type, parent, id);
					obj->physicsControl = physicsControl;
					obj->SetAttributes(attributes);
					obj->SetIteration(iteration);
					obj->SetTransformFromNetwork(transform);
					
					auto* mod = V4D_Mod::GetModule(moduleID.String());
					if (mod) {
						if (mod->CreateGameObject) mod->CreateGameObject(obj);
						if (mod->ReceiveStreamCustomGameObjectData) mod->ReceiveStreamCustomGameObjectData(obj, tmpStream1);
						if (mod->ReceiveStreamCustomGameObjectTransformData) mod->ReceiveStreamCustomGameObjectTransformData(obj, tmpStream2);
						if (mod->AddGameObjectToScene) {
							mod->AddGameObjectToScene(obj, scene);
							obj->UpdateGameObject();
							obj->UpdateGameObjectTransform();
						}
					} else {
						LOG_ERROR("Client ReceiveAction ADD_OBJECT : module " << moduleID.String() << " is not loaded")
					}
					
					clientSideObjects.objects[id] = obj;
				} else {
					LOG_ERROR("Client ReceiveAction ADD_OBJECT : moduleID is not valid")
				}
			}break;
			case UPDATE_OBJECT:{
				auto parent = stream->Read<NetworkGameObject::Parent>();
				auto id = stream->Read<NetworkGameObject::Id>();
				auto physicsControl = stream->Read<bool>();
				auto attributes = stream->Read<NetworkGameObject::Attributes>();
				auto iteration = stream->Read<NetworkGameObject::Iteration>();
				auto transform = stream->Read<NetworkGameObjectTransform>();
				auto tmpStream1 = stream->ReadStream();
				auto tmpStream2 = stream->ReadStream();
				try {
					std::lock_guard lock(clientSideObjects.mutex);
					// LOG_DEBUG("Client ReceiveAction UPDATE_OBJECT for obj id " << id)
					
					auto obj = clientSideObjects.objects.at(id);
					obj->parent = parent;
					obj->physicsControl = physicsControl;
					obj->SetAttributes(attributes);
					obj->SetIteration(iteration);
					obj->SetTransformFromNetwork(transform);
					
					auto* mod = V4D_Mod::GetModule(obj->moduleID.String());
					if (mod) {
						if (mod->ReceiveStreamCustomGameObjectData) mod->ReceiveStreamCustomGameObjectData(obj, tmpStream1);
						if (mod->ReceiveStreamCustomGameObjectTransformData) mod->ReceiveStreamCustomGameObjectTransformData(obj, tmpStream2);
					}
					obj->UpdateGameObject();
					obj->UpdateGameObjectTransform();
				} catch(std::exception& err) {
					LOG_ERROR("Client ReceiveAction UPDATE_OBJECT : " << err.what())
				}
			}break;
			case REMOVE_OBJECT:{
				auto id = stream->Read<NetworkGameObject::Id>();
				// LOG_DEBUG("Client ReceiveAction REMOVE_OBJECT for obj id " << id)
				try {
					std::lock_guard lock(clientSideObjects.mutex);
					
					auto obj = clientSideObjects.objects.at(id);
					
					auto* mod = V4D_Mod::GetModule(obj->moduleID.String());
					if (mod) {
						if (mod->DestroyGameObject) mod->DestroyGameObject(obj, scene);
					}
					
					obj->RemoveGameObject();
					
					clientSideObjects.objects.erase(obj->id);
					
				} catch(std::exception& err) {
					LOG_ERROR("Client ReceiveAction REMOVE_OBJECT : " << err.what())
				}
			}break;
			default: 
				LOG_ERROR("Client ReceiveAction UNRECOGNIZED MODULE ACTION " << std::to_string((int)action))
			break;
		}
	}
	
	V4D_MODULE_FUNC(void, ClientReceiveBurst, v4d::io::SocketPtr stream) {
		auto action = stream->Read<Action>();
		switch (action) {
			case SYNC_OBJECT_TRANSFORM:{
				auto id = stream->Read<NetworkGameObject::Id>();
				auto iteration = stream->Read<NetworkGameObject::Iteration>();
				auto transform = stream->Read<NetworkGameObjectTransform>();
				auto tmpStream = stream->ReadStream();
				try {
					std::lock_guard lock(clientSideObjects.mutex);
					// LOG_DEBUG("Client ReceiveBurst for obj id " << id)
					auto obj = clientSideObjects.objects.at(id);
					if (obj->GetIteration() == iteration) {
						obj->SetTransformFromNetwork(transform);
						
						auto* mod = V4D_Mod::GetModule(obj->moduleID.String());
						if (mod) {
							if (mod->ReceiveStreamCustomGameObjectTransformData) mod->ReceiveStreamCustomGameObjectTransformData(obj, tmpStream);
						}
						obj->UpdateGameObjectTransform();
					}
				} catch(std::exception& err) {
					LOG_ERROR_VERBOSE("Client ReceiveBurst SYNC_OBJECT_TRANSFORM : " << err.what())
				}
			}break;
			
			default: 
				LOG_ERROR("Client ReceiveBurst UNRECOGNIZED MODULE ACTION " << std::to_string((int)action))
			break;
		}
	}
	
	#pragma endregion
	
};
