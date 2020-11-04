#define _V4D_MODULE
#include <v4d.h>
#include "common.hh"
#include "actions.hh"
#include "ClientSideObjects.hh"

using namespace v4d::scene;
using namespace v4d::networking;
using namespace networking::action;
using namespace v4d::modular;

std::shared_ptr<OutgoingConnection> client = nullptr;
Scene* scene = nullptr;

ClientSideObjects clientSideObjects {};

std::recursive_mutex actionQueueMutex;
std::queue<v4d::data::Stream> actionQueue {};

V4D_MODULE_CLASS(V4D_Client) {
	V4D_MODULE_FUNC(bool, ModuleIsPrimary) {return true;}
	
	V4D_MODULE_FUNC(void*, ModuleGetCustomPtr, int) {
		return &clientSideObjects;
	}
	
	V4D_MODULE_FUNC(void, Init, std::shared_ptr<OutgoingConnection> _c, Scene* _s) {
		client = _c;
		scene = _s;
	}
	
	V4D_MODULE_FUNC(void, UnloadScene) {
		std::lock_guard lock(clientSideObjects.mutex);
		clientSideObjects.objects.clear();
	}
	
	V4D_MODULE_FUNC(void, EnqueueAction, v4d::data::WriteOnlyStream& stream) {
		std::lock_guard lock(actionQueueMutex);
		actionQueue.emplace(stream);
	}
	
	V4D_MODULE_FUNC(void, SendActions, v4d::io::SocketPtr stream) {
		std::lock_guard lock(actionQueueMutex);
		while (actionQueue.size()) {
			// LOG_DEBUG("Client SendActionFromQueue")
			stream->Begin();
				stream->EmplaceStream(actionQueue.front());
			stream->End();
			actionQueue.pop();
		}
	}
	
	V4D_MODULE_FUNC(void, SendBursts, v4d::io::SocketPtr stream) {
		v4d::data::WriteOnlyStream tmpStream(CUSTOM_OBJECT_TRANSFORM_DATA_MAX_STREAM_SIZE);
		std::lock_guard lock(clientSideObjects.mutex);
		for (auto&[objID, obj] : clientSideObjects.objects) {
			if (obj->physicsControl) {
				obj->ReverseUpdateObjectInstanceTransform();
				stream->Begin();
					*stream << SYNC_OBJECT_TRANSFORM;
					*stream << obj->id;
					*stream << obj->GetIteration();
					*stream << obj->GetNetworkTransform();
					
					auto mod = V4D_Objects::GetModule(obj->moduleID.String());
					
					tmpStream.ClearWriteBuffer();
					if (mod && mod->SendStreamCustomTransformData) mod->SendStreamCustomTransformData(obj, tmpStream);
					DEBUG_ASSERT_WARN(tmpStream.GetWriteBufferSize() <= CUSTOM_OBJECT_TRANSFORM_DATA_MAX_STREAM_SIZE, "V4D_Client::SendBursts for module '" << mod->ModuleName() << "', CustomTransformData for Object type " << obj->type << " stream size was " << tmpStream.GetWriteBufferSize() << " bytes, but should be at most " << CUSTOM_OBJECT_TRANSFORM_DATA_MAX_STREAM_SIZE << " bytes")
					stream->WriteStream(tmpStream);
					
				stream->End();
			}
		}
	}
	
	V4D_MODULE_FUNC(void, ReceiveAction, v4d::io::SocketPtr stream) {
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
					obj->iteration = iteration;
					obj->SetTransformFromNetwork(transform);
					
					auto* mod = V4D_Objects::GetModule(moduleID.String());
					if (mod) {
						if (mod->CreateObject) mod->CreateObject(obj);
						if (mod->ReceiveStreamCustomObjectData) mod->ReceiveStreamCustomObjectData(obj, tmpStream1);
						if (mod->ReceiveStreamCustomTransformData) mod->ReceiveStreamCustomTransformData(obj, tmpStream2);
						if (mod->AddToScene) {
							mod->AddToScene(obj, scene);
							obj->UpdateObjectInstance();
							obj->UpdateObjectInstanceTransform();
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
					obj->iteration = iteration;
					obj->SetTransformFromNetwork(transform);
					
					auto* mod = V4D_Objects::GetModule(obj->moduleID.String());
					if (mod) {
						if (mod->ReceiveStreamCustomObjectData) mod->ReceiveStreamCustomObjectData(obj, tmpStream1);
						if (mod->ReceiveStreamCustomTransformData) mod->ReceiveStreamCustomTransformData(obj, tmpStream2);
					}
					obj->UpdateObjectInstance();
					obj->UpdateObjectInstanceTransform();
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
					
					auto* mod = V4D_Objects::GetModule(obj->moduleID.String());
					if (mod) {
						if (mod->DestroyObject) mod->DestroyObject(obj, scene);
					}
					
					obj->RemoveObjectInstance(scene);
					
					clientSideObjects.objects.erase(obj->id);
					
				} catch(std::exception& err) {
					LOG_ERROR("Client ReceiveAction REMOVE_OBJECT : " << err.what())
				}
			}break;
			case ASSIGN:{ // assign object to client for camera
				auto id = stream->Read<NetworkGameObject::Id>();
				// LOG_DEBUG("Client ReceiveAction ASSIGN for obj id " << id)
				try {
					std::lock_guard lock(clientSideObjects.mutex);
					auto obj = clientSideObjects.objects.at(id);
					scene->cameraParent = obj->objectInstance;
					obj->objectInstance->rayTracingMaskRemoved |= GEOMETRY_ATTR_PRIMARY_VISIBLE;
					obj->objectInstance->SetGeometriesDirty();
				} catch (std::exception& err) {
					LOG_ERROR("Client ReceiveAction ASSIGN ("<<id<<") : " << err.what())
				}
			}break;
			default: 
				LOG_ERROR("Client ReceiveAction UNRECOGNIZED MODULE ACTION " << std::to_string((int)action))
			break;
		}
	}
	
	V4D_MODULE_FUNC(void, ReceiveBurst, v4d::io::SocketPtr stream) {
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
					if (obj->iteration == iteration) {
						obj->SetTransformFromNetwork(transform);
						
						auto* mod = V4D_Objects::GetModule(obj->moduleID.String());
						if (mod) {
							if (mod->ReceiveStreamCustomTransformData) mod->ReceiveStreamCustomTransformData(obj, tmpStream);
						}
						obj->UpdateObjectInstanceTransform();
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
	
};
