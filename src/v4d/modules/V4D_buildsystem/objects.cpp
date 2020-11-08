#define _V4D_MODULE
#include <v4d.h>
#include "common.hh"

using namespace v4d::scene;

CachedData cachedData {};

V4D_MODULE_CLASS(V4D_Objects) {
	
	V4D_MODULE_FUNC(void*, ModuleGetCustomPtr, int) {
		return &cachedData;
	}
	
	// Client-Only
	V4D_MODULE_FUNC(void, CreateObject, v4d::scene::NetworkGameObjectPtr obj) {
		std::lock_guard lock(cachedData.objectMapsMutex);
		switch (obj->type) {
			case OBJECT_TYPE::Build:{
				cachedData.builds[obj->id] = std::make_shared<Build>(obj->id);
				cachedData.buildBlocks[obj->id] = {};
			}break;
		}
	}
	V4D_MODULE_FUNC(void, DestroyObject, v4d::scene::NetworkGameObjectPtr obj, v4d::scene::Scene* scene) {
		std::lock_guard lock(cachedData.objectMapsMutex);
		switch (obj->type) {
			case OBJECT_TYPE::Build:{
				try {cachedData.builds.erase(obj->id);} catch(...){}
				try {cachedData.buildBlocks.erase(obj->id);} catch(...){}
			}break;
		}
	}
	V4D_MODULE_FUNC(void, AddToScene, v4d::scene::NetworkGameObjectPtr obj, v4d::scene::Scene* scene) {
		std::lock_guard lock(cachedData.objectMapsMutex);
		switch (obj->type) {
			case OBJECT_TYPE::Build:{
				obj->objectInstance = cachedData.builds[obj->id]->AddToScene(scene);
				obj->objectInstance->AssignToModule(THIS_MODULE, obj->id);
				obj->objectInstance->rigidbodyType = ObjectInstance::RigidBodyType::DYNAMIC;
			}break;
		}
	}
	
	
	// Server-Only
	V4D_MODULE_FUNC(void, SendStreamCustomObjectData, v4d::scene::NetworkGameObjectPtr obj, v4d::data::WriteOnlyStream& stream) {
		std::lock_guard lock1(cachedData.serverObjectMapsMutex);
		{// Data over network
			stream.Write(cachedData.serverBuildBlocks[obj->id]);
		}
	}
	
	// Client-Only
	V4D_MODULE_FUNC(void, ReceiveStreamCustomObjectData, v4d::scene::NetworkGameObjectPtr obj, v4d::data::ReadOnlyStream& stream) {
		std::lock_guard lock(cachedData.objectMapsMutex);
		{// Data over network
			cachedData.buildBlocks[obj->id] = stream.Read<std::vector<Block>>();
		}
		try { // Update build in-game
			auto& build = cachedData.builds.at(obj->id);
			LOG("Client Reveived build")
			if (build) {
				auto& blocks = cachedData.buildBlocks.at(obj->id);
				build->SwapBlocksVector(blocks);
			}
		} catch(...){}
	}
	
};
