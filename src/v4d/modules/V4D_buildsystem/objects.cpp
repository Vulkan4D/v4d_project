#define _V4D_MODULE
#include <v4d.h>
#include "common.hh"

using namespace v4d::scene;

namespace OBJECT_TYPE {
	const uint32_t Build = 0;
}

std::mutex objectMapsMutex;
std::unordered_map<NetworkGameObject::Id, std::mutex> objectMutexes {};
std::unordered_map<NetworkGameObject::Id, std::shared_ptr<Build>> builds {};
std::unordered_map<NetworkGameObject::Id, std::vector<Block>> buildBlocks {};


V4D_MODULE_CLASS(V4D_Objects) {
	
	V4D_MODULE_FUNC(void, CreateObject, v4d::scene::NetworkGameObjectPtr obj) {
		std::lock_guard lock1(objectMapsMutex);
		std::lock_guard lock2(objectMutexes[obj->id]);
		switch (obj->type) {
			case OBJECT_TYPE::Build:{
				builds[obj->id] = std::make_shared<Build>();
				buildBlocks[obj->id] = {};
			}break;
		}
	}
	V4D_MODULE_FUNC(void, DestroyObject, v4d::scene::NetworkGameObjectPtr obj, v4d::scene::Scene* scene) {
		std::lock_guard lock1(objectMapsMutex);
		{
			std::lock_guard lock2(objectMutexes[obj->id]);
			switch (obj->type) {
				case OBJECT_TYPE::Build:{
					try {builds.erase(obj->id);} catch(...){}
					try {buildBlocks.erase(obj->id);} catch(...){}
				}break;
			}
		}
		try {objectMutexes.erase(obj->id);} catch(...){}
	}
	
	V4D_MODULE_FUNC(void, SendStreamCustomObjectData, v4d::scene::NetworkGameObjectPtr obj, v4d::data::WriteOnlyStream& stream) {
		std::lock_guard lock1(objectMapsMutex);
		std::lock_guard lock2(objectMutexes[obj->id]);
		{// Data over network
			stream.Write(buildBlocks[obj->id]);
		}
	}
	V4D_MODULE_FUNC(void, ReceiveStreamCustomObjectData, v4d::scene::NetworkGameObjectPtr obj, v4d::data::ReadOnlyStream& stream) {
		std::lock_guard lock1(objectMapsMutex);
		std::lock_guard lock2(objectMutexes[obj->id]);
		{// Data over network
			buildBlocks[obj->id] = stream.Read<std::vector<Block>>();
		}
		try { // Update build in-game
			auto& build = builds.at(obj->id);
			if (build) {
				auto& blocks = buildBlocks.at(obj->id);
				build->SwapBlocksVector(blocks);
			}
		} catch(...){}
	}
	
	V4D_MODULE_FUNC(void, AddToScene, v4d::scene::NetworkGameObjectPtr obj, v4d::scene::Scene* scene) {
		std::lock_guard lock1(objectMapsMutex);
		std::lock_guard lock2(objectMutexes[obj->id]);
		switch (obj->type) {
			case OBJECT_TYPE::Build:{
				obj->objectInstance = builds[obj->id]->AddToScene(scene);
				obj->objectInstance->rigidbodyType = ObjectInstance::RigidBodyType::DYNAMIC;
			}break;
		}
	}
	
};
