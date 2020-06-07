#define _V4D_MODULE
#include <v4d.h>
#include "common.hh"
#include "networkActions.hh"

using namespace v4d::scene;
using namespace v4d::networking;
using namespace app::networking::action;
using namespace v4d::modular;

std::shared_ptr<OutgoingConnection> client = nullptr;
Scene* scene = nullptr;

std::recursive_mutex objectsMutex;
ClientObjects objects {};

std::recursive_mutex actionQueueMutex;
std::queue<v4d::data::Stream> actionQueue {};

V4D_MODULE_CLASS(V4D_Client) {
	
	V4D_MODULE_FUNC(void*, ModuleGetCustomPtr, int) {
		return &objects;
	}
	
	V4D_MODULE_FUNC(void, Init, std::shared_ptr<OutgoingConnection> _c, Scene* _s) {
		client = _c;
		scene = _s;
	}
	
	V4D_MODULE_FUNC(void, EnqueueAction, v4d::data::Stream& stream) {
		std::lock_guard lock(actionQueueMutex);
		actionQueue.emplace(stream);
	}
	
	V4D_MODULE_FUNC(void, SendActions, v4d::io::SocketPtr stream) {
		std::lock_guard lock(actionQueueMutex);
		while (actionQueue.size()) {
			stream->Begin();
				*stream << actionQueue.front();
				actionQueue.pop();
			stream->End();
		}
	}
	
	V4D_MODULE_FUNC(void, SendBursts, v4d::io::SocketPtr stream) {
		v4d::data::WriteOnlyStream tmpStream(256);
		std::lock_guard lock(objectsMutex);
		for (auto&[objID, obj] : objects) {
			if (obj->physicsControl) {
				obj->ReverseUpdateObjectInstanceTransform();
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
	
	V4D_MODULE_FUNC(void, ReceiveAction, v4d::io::SocketPtr stream) {
		auto action = stream->Read<app::networking::Action>();
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
					std::lock_guard lock(objectsMutex);
					LOG_DEBUG("Client ReceiveAction ADD_OBJECT for obj id " << id)
					
					auto obj = std::make_shared<NetworkGameObject>(moduleID, type, parent, id);
					obj->physicsControl = physicsControl;
					obj->SetAttributes(attributes);
					obj->iteration = iteration;
					obj->SetTransformFromNetwork(transform);
					
					auto* mod = V4D_Objects::GetModule(moduleID.String());
					if (mod) {
						if (mod->ReceiveStreamCustomObjectData) mod->ReceiveStreamCustomObjectData(obj, tmpStream1);
						if (mod->ReceiveStreamCustomTransformData) mod->ReceiveStreamCustomTransformData(obj, tmpStream2);
						if (mod->BuildObject) {
							mod->BuildObject(obj, scene);
							obj->UpdateObjectInstance();
							obj->UpdateObjectInstanceTransform();
						}
					}
					
					objects[id] = obj;
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
					std::lock_guard lock(objectsMutex);
					
					auto obj = objects.at(id);
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
				try {
					std::lock_guard lock(objectsMutex);
					
					auto obj = objects.at(id);
					
					auto* mod = V4D_Objects::GetModule(obj->moduleID.String());
					if (mod) {
						if (mod->DestroyObject) mod->DestroyObject(obj, scene);
					}
					
					obj->RemoveObjectInstance(scene);
					
					objects.erase(obj->id);
					
				} catch(std::exception& err) {
					LOG_ERROR("Client ReceiveAction REMOVE_OBJECT : " << err.what())
				}
			}break;
			case ASSIGN:{ // assign object to client for camera
				auto objectID = stream->Read<NetworkGameObject::Id>();
				try {
					std::lock_guard lock(objectsMutex);
					auto obj = objects.at(objectID);
					scene->cameraParent = obj->objectInstance;
					obj->objectInstance->rayTracingMaskRemoved |= GEOMETRY_ATTR_PRIMARY_VISIBLE;
				} catch (std::exception& err) {
					LOG_ERROR("Client ReceiveAction ASSIGN : " << err.what())
				}
			}break;
		}
	}
	
	V4D_MODULE_FUNC(void, ReceiveBurst, v4d::io::SocketPtr stream) {
		auto action = stream->Read<app::networking::Action>();
		switch (action) {
			case SYNC_OBJECT_TRANSFORM:
				auto id = stream->Read<uint32_t>();
				auto iteration = stream->Read<uint32_t>();
				auto transform = stream->Read<NetworkGameObjectTransform>();
				auto tmpStream = stream->ReadStream();
				try {
					std::lock_guard lock(objectsMutex);
					LOG_DEBUG("Client ReceiveBurst for obj id " << id)
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
			break;
		}
	}
	
};