#include <v4d.h>
#include <V4D_Mod.h>
#include "v4d/game/Entity.h"

#include "utilities/io/Logger.h"

#include "common.hh"
#include "actions.hh"
#include "../V4D_flycam/common.hh"

using namespace v4d::scene;
using namespace v4d::networking;
using namespace networking::action;
using namespace v4d::modular;

std::shared_ptr<ListeningServer> server = nullptr;
std::shared_ptr<OutgoingConnection> client = nullptr;
Scene* scene = nullptr;
v4d::graphics::Renderer* r = nullptr;

std::recursive_mutex serverActionQueueMutex;
std::unordered_map<uint64_t /* clientID */, std::queue<v4d::data::Stream>> serverActionQueuePerClient {};

inline const size_t BURST_SYNC_POSITIONS_PLUS_ROTATIONS_GROUP_SIZE = 14;
inline const size_t BURST_SYNC_POSITIONS_GROUP_SIZE = 16; // max 28;
inline const size_t BURST_SYNC_ROTATIONS_GROUP_SIZE = 16; // max 24;
inline const size_t BURST_SYNC_CACHE_INITIAL_VECTOR_SIZE = 32;
inline const double BURST_SYNC_MAX_DISTANCE = 10'000; // 10 km

struct NearbyEntity {
	int32_t id;
	glm::vec3 position;
	glm::quat orientation;
	double distance;
	NearbyEntity(Entity::Id id, Entity::Position position, Entity::Orientation orientation, double distance)
	: id(int32_t(id))
	, position(position)
	, orientation(orientation)
	, distance(distance)
	{}
};

float interpolationSpeed = 15.0;

V4D_MODULE_CLASS(V4D_Mod) {
	
	#pragma region Init
	
	V4D_MODULE_FUNC(int, OrderIndex) {
		return -1000;
	}
	
	V4D_MODULE_FUNC(void, InitServer, std::shared_ptr<ListeningServer> _srv) {
		server = _srv;
	}
	
	V4D_MODULE_FUNC(void, InitClient, std::shared_ptr<OutgoingConnection> _c) {
		client = _c;
	}
	
	V4D_MODULE_FUNC(void, InitRenderer, v4d::graphics::Renderer* _r) {
		r = _r;
	}
	
	V4D_MODULE_FUNC(void, LoadScene, Scene* _s) {
		scene = _s;
	}
	
	V4D_MODULE_FUNC(void, UnloadScene) {
		ServerSidePlayer::ClearAll();
		ServerSideEntity::ClearAll();
		ClientSideEntity::ClearAll();
	}
	
	#pragma endregion
	
	V4D_MODULE_FUNC(void, SlowLoopUpdate, double deltaTime) {
		ServerSideEntity::ForEach([](ServerSideEntity::Ptr entity){
			if (!entity->active && entity->iteration > 0 && entity->clientIterations.size() == 0) {
				entity->Destroy();
			}
		});
		ServerSideEntity::CleanupOnThisThread();
		ClientSideEntity::CleanupOnThisThread();
		ServerSidePlayer::CleanupOnThisThread();
	}
	
	#pragma region Rendeering
	
	V4D_MODULE_FUNC(void, RenderFrame_BeforeUpdate) {
		// Smoothly Interpolate position and orientation
		double delta = r->deltaTime * interpolationSpeed;
		ClientSideEntity::ForEach([&delta](ClientSideEntity::Ptr entity) {
			if (entity->posInit) {
				entity->position = glm::mix(entity->position, entity->targetPosition, delta);
				entity->orientation = glm::slerp(entity->orientation, entity->targetOrientation, delta);
			} else {
				entity->position = entity->targetPosition;
				entity->orientation = entity->targetOrientation;
				entity->posInit = true;
			}
			
			entity->UpdateRenderable();
		});
	}
	
	V4D_MODULE_FUNC(void, DrawUi2) {
		#ifdef _ENABLE_IMGUI
			ImGui::SliderFloat("Network interpolation smooth speed", &interpolationSpeed, 0.0f, 30.0f);
		#endif
	}
	
	#pragma endregion
	
	#pragma region Server
	
	V4D_MODULE_FUNC(void, ServerSendActions, v4d::io::SocketPtr stream, IncomingClientPtr client) {
		v4d::data::WriteOnlyStream tmpStream(CUSTOM_ENTITY_DATA_INITIAL_STREAM_SIZE);
		
		auto serverSideLock = ServerSideEntity::GetLock();
		
		ServerSideEntity::ForEach([&stream, &client, &tmpStream](ServerSideEntity::Ptr entity){
			Entity::Iteration clientIteration;
			if (entity->active) {
				try {
					clientIteration = entity->clientIterations.at(client->id);
				} catch (...) {
					clientIteration = entity->clientIterations[client->id] = 0;
				}
				if (entity->iteration > clientIteration) {
					stream->Begin();
						if (clientIteration == 0) {
							// LOG_DEBUG("Server SendAction ADD_ENTITY for obj id " << obj->id << ", client " << client->id)
							// Add
							*stream << ADD_ENTITY;
							*stream << entity->moduleID.vendor;
							*stream << entity->moduleID.module;
							*stream << entity->type;
						} else {
							// LOG_DEBUG("Server SendAction UPDATE_ENTITY for obj id " << obj->id << ", client " << client->id)
							// Update
							*stream << UPDATE_ENTITY;
						}
						*stream << entity->referenceFrame;
						*stream << entity->GetID();
						*stream << entity->referenceFrameExtra;
						*stream << entity->iteration;
						*stream << entity->position;
						*stream << entity->orientation;
						
						auto mod = V4D_Mod::GetModule(entity->moduleID.String());
						
						tmpStream.ClearWriteBuffer();
						if (mod && mod->StreamSendEntityData) mod->StreamSendEntityData(entity->GetID(), entity->type, tmpStream);
						stream->WriteStream(tmpStream);
						
						tmpStream.ClearWriteBuffer();
						if (mod && mod->StreamSendEntityTransformData) mod->StreamSendEntityTransformData(entity->GetID(), entity->type, tmpStream);
						stream->WriteStream(tmpStream);
						
					stream->End();
					entity->clientIterations[client->id] = entity->iteration;
				}
			} else {
				try {
					clientIteration = entity->clientIterations.at(client->id);
					v4d::data::WriteOnlyStream removeStream(8);
						removeStream << REMOVE_ENTITY;
						removeStream << entity->GetID();
					serverActionQueuePerClient[client->id].emplace(removeStream);
					entity->clientIterations.erase(client->id);
				} catch(std::out_of_range) {}// NO ERROR HERE, it's normal that this entity may have already been removed for this client
			}
		});
		
		auto& actionQueue = serverActionQueuePerClient[client->id];
		while (actionQueue.size()) {
			// LOG_DEBUG("Server SendActionFromQueue for client " << client->id)
			stream->Begin();
				stream->EmplaceStream(actionQueue.front());
			stream->End();
			actionQueue.pop();
		}
	}
	
	V4D_MODULE_FUNC(void, ServerSendBursts, v4d::io::SocketPtr stream, IncomingClientPtr client, uint64_t frame) {
		glm::dvec3 basePosition;
		uint64_t referenceFrame;
		if (ServerSidePlayer::Ptr player = ServerSidePlayer::Get(client->id); player) {
			if (ServerSideEntity::Ptr playerEntity = player->GetServerSideEntity(); playerEntity) {
				basePosition = playerEntity->position;
				referenceFrame = playerEntity->referenceFrame;
				goto SendBursts;
			}
		}
		return;
		
		SendBursts:
		
			// Fill cache vector and sort it by distance (closest first)
			// Also filter them by active, same reference frame as user, and less than 10 km away from user
			std::vector<NearbyEntity> nearbyEntities {};
			nearbyEntities.reserve(BURST_SYNC_CACHE_INITIAL_VECTOR_SIZE);
			ServerSideEntity::ForEach([&basePosition, &referenceFrame, &nearbyEntities](ServerSideEntity::Ptr entity){
				if (entity->active && entity->isDynamic && entity->referenceFrame == referenceFrame) {
					double distance = glm::length(entity->position - basePosition);
					if (distance < BURST_SYNC_MAX_DISTANCE) {
						nearbyEntities.emplace_back(
							entity->GetID(),
							entity->position - basePosition,
							entity->orientation,
							distance
						);
					}
				}
			});
			std::sort(nearbyEntities.begin(), nearbyEntities.end(), [](const auto& a, const auto& b){
				//TODO maybe take into account the bounding radius too so that when the player is inside a very big craft it can be considered to be closer than another object which the center is closer
				return a.distance < b.distance;
			});
			
			
			{// Sync positions in groups of base N. First group of N is synced every frame, next group of N*2 is synced every 2 frames, next group of N*4 is synced every 4 frames, and so on...
				int frameDivider = 1;
				size_t start = 0;
				size_t end = 0;
				while (nearbyEntities.size() > end) {
					start = end;
					end = std::min(nearbyEntities.size(), start + BURST_SYNC_POSITIONS_GROUP_SIZE * frameDivider);
					
					stream->Begin();
						*stream << SYNC_GROUPED_ENTITIES_POSITIONS;
						
						*stream << glm::dvec3(basePosition);
						for (size_t n = start; n < end; ++n) if (n % frameDivider == frame % frameDivider) {
							*stream << nearbyEntities[n].id;
							*stream << nearbyEntities[n].position;
						}
						*stream << (int32_t)-1;
						
					stream->End();
					
					frameDivider *= 2;
				}
			}
			
			{// Sync rotations in groups of base N, half as often as positions. First group of N*2 is synced every 2 frames, next group of N*4 is synced every 4 frames, next group of N*8 is synced every 8 frames, and so on...
				int frameDivider = 2;
				size_t start = 0;
				size_t end = 0;
				while (nearbyEntities.size() > end) {
					start = end;
					end = std::min(nearbyEntities.size(), start + BURST_SYNC_ROTATIONS_GROUP_SIZE * frameDivider);
					
					stream->Begin();
						*stream << SYNC_GROUPED_ENTITIES_ROTATIONS;
						
						for (size_t n = start; n < end; ++n) if (n % frameDivider == frame % frameDivider) {
							*stream << nearbyEntities[n].id;
							*stream << nearbyEntities[n].orientation;
						}
						*stream << (int32_t)-1;
						
					stream->End();
					
					frameDivider *= 2;
				}
			}
			
		
		// ServerSideEntity::ForEach([&stream, &client](ServerSideEntity::Ptr entity){
		// 	v4d::data::WriteOnlyStream tmpStream(CUSTOM_ENTITY_TRANSFORM_DATA_MAX_STREAM_SIZE);
		// 	if (entity->active && entity->isDynamic) {
		// 		if (entity->iteration == entity->clientIterations[client->id]) {
		// 			stream->Begin();
		// 				*stream << SYNC_ENTITY_TRANSFORM;
		// 				*stream << entity->GetID();
		// 				*stream << entity->iteration;
		// 				*stream << entity->position;
		// 				*stream << entity->orientation;
						
		// 				auto mod = V4D_Mod::GetModule(entity->moduleID.String());
						
		// 				tmpStream.ClearWriteBuffer();
		// 				if (mod && mod->StreamSendEntityTransformData) mod->StreamSendEntityTransformData(entity->GetID(), entity->type, tmpStream);
		// 				DEBUG_ASSERT_WARN(tmpStream.GetWriteBufferSize() <= CUSTOM_ENTITY_TRANSFORM_DATA_MAX_STREAM_SIZE, "V4D_Server::SendBursts for module '" << mod->ModuleName() << "', CustomTransformData for Object type " << entity->type << " stream size was " << tmpStream.GetWriteBufferSize() << " bytes, but should be at most " << CUSTOM_ENTITY_TRANSFORM_DATA_MAX_STREAM_SIZE << " bytes")
		// 				stream->WriteStream(tmpStream); // must send stream even the module did not define the function to write to it, because at the other end we are trying to read it anyways
						
		// 			stream->End();
		// 		}
		// 	}
		// });
	}
	
	#pragma endregion
	
	#pragma region Client
	
	V4D_MODULE_FUNC(void, ClientReceiveAction, v4d::io::SocketPtr stream) {
		auto action = stream->Read<Action>();
		switch (action) {
			case ADD_ENTITY:{
				auto _vendor = stream->Read<typeof ModuleID::vendor>();
				auto _module = stream->Read<typeof ModuleID::module>();
				ModuleID moduleID(_vendor, _module);
				auto type = stream->Read<Entity::Type>();
				auto referenceFrame = stream->Read<Entity::ReferenceFrame>();
				auto id = stream->Read<Entity::Id>();
				auto referenceFrameExtra = stream->Read<Entity::ReferenceFrameExtra>();
				auto iteration = stream->Read<Entity::Iteration>();
				auto position = stream->Read<Entity::Position>();
				auto orientation = stream->Read<Entity::Orientation>();
				auto tmpStream1 = stream->ReadStream();
				auto tmpStream2 = stream->ReadStream();
				if (moduleID.IsValid()) {
					// LOG_DEBUG("Client ReceiveAction ADD_ENTITY for obj id " << id)
					
					ClientSideEntity::Ptr entity = ClientSideEntity::Create(id, moduleID, type, referenceFrame, referenceFrameExtra, position, orientation, iteration);
					
					auto* mod = V4D_Mod::GetModule(moduleID.String());
					if (mod) {
						if (mod->CreateEntity) mod->CreateEntity(id, entity->type);
						if (mod->StreamReceiveEntityData) mod->StreamReceiveEntityData(id, entity->type, tmpStream1);
						if (mod->StreamReceiveEntityTransformData) mod->StreamReceiveEntityTransformData(id, entity->type, tmpStream2);
					} else {
						LOG_ERROR("Client ReceiveAction ADD_ENTITY : module " << moduleID.String() << " is not loaded")
					}
				
				} else {
					LOG_ERROR("Client ReceiveAction ADD_ENTITY : moduleID is not valid")
				}
			}break;
			case UPDATE_ENTITY:{
				auto referenceFrame = stream->Read<Entity::ReferenceFrame>();
				auto id = stream->Read<Entity::Id>();
				auto referenceFrameExtra = stream->Read<Entity::ReferenceFrameExtra>();
				auto iteration = stream->Read<Entity::Iteration>();
				auto position = stream->Read<Entity::Position>();
				auto orientation = stream->Read<Entity::Orientation>();
				auto tmpStream1 = stream->ReadStream();
				auto tmpStream2 = stream->ReadStream();
				
				// LOG_DEBUG("Client ReceiveAction UPDATE_ENTITY for obj id " << id)
				
				ClientSideEntity::Ptr entity = ClientSideEntity::Get(id);
				if (entity) {
					entity->referenceFrame = referenceFrame;
					entity->referenceFrameExtra = referenceFrameExtra;
					entity->iteration = iteration;
					entity->targetPosition = position;
					entity->targetOrientation = orientation;
					
					auto* mod = V4D_Mod::GetModule(entity->moduleID.String());
					if (mod) {
						if (mod->StreamReceiveEntityData) mod->StreamReceiveEntityData(id, entity->type, tmpStream1);
						if (mod->StreamReceiveEntityTransformData) mod->StreamReceiveEntityTransformData(id, entity->type, tmpStream2);
					}
				}
			}break;
			case REMOVE_ENTITY:{
				auto id = stream->Read<Entity::Id>();
				
				// LOG_DEBUG("Client ReceiveAction REMOVE_ENTITY for obj id " << id)
				
				ClientSideEntity::Ptr entity = ClientSideEntity::Get(id);
				if (entity) {
					auto* mod = V4D_Mod::GetModule(entity->moduleID.String());
					if (mod && mod->DestroyEntity) mod->DestroyEntity(id, entity->type);
					entity->DestroyRenderable();
					entity->Destroy();
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
			// case SYNC_ENTITY_TRANSFORM:{
			// 	auto id = stream->Read<Entity::Id>();
			// 	auto iteration = stream->Read<Entity::Iteration>();
			// 	auto position = stream->Read<Entity::Position>();
			// 	auto orientation = stream->Read<Entity::Orientation>();
			// 	auto tmpStream = stream->ReadStream();
			// 	if (ClientSideEntity::Ptr entity = ClientSideEntity::Get(id); entity) {
			// 		// LOG_DEBUG("Client ReceiveBurst for obj id " << id)
			// 		if (entity->iteration == iteration) {
			// 			entity->targetPosition = position;
			// 			entity->targetOrientation = orientation;
						
			// 			auto* mod = V4D_Mod::GetModule(entity->moduleID.String());
			// 			if (mod && mod->StreamReceiveEntityTransformData) mod->StreamReceiveEntityTransformData(id, entity->type, tmpStream);
			// 		}
			// 	}
			// }break;
			
			case SYNC_GROUPED_ENTITIES_POSITIONS_ROTATIONS:{
				glm::dvec3 basePosition = stream->Read<glm::dvec3>();
				auto entitiesLock = ClientSideEntity::GetLock();
				for (;;) {
					auto id = Entity::Id(stream->Read<int32_t>());
					if (id < 0) break;
					auto position = Entity::Position(stream->Read<glm::vec3>());
					auto orientation = Entity::Orientation(stream->Read<glm::quat>());
					if (ClientSideEntity::Ptr entity = ClientSideEntity::Get(id); entity) {
						entity->targetPosition = position + basePosition;
						entity->targetOrientation = orientation;
					}
				}
			}break;
			
			case SYNC_GROUPED_ENTITIES_POSITIONS:{
				glm::dvec3 basePosition = stream->Read<glm::dvec3>();
				auto entitiesLock = ClientSideEntity::GetLock();
				for (;;) {
					auto id = Entity::Id(stream->Read<int32_t>());
					if (id < 0) break;
					auto position = Entity::Position(stream->Read<glm::vec3>());
					if (ClientSideEntity::Ptr entity = ClientSideEntity::Get(id); entity) {
						entity->targetPosition = position + basePosition;
					}
				}
			}break;
			
			case SYNC_GROUPED_ENTITIES_ROTATIONS:{
				auto entitiesLock = ClientSideEntity::GetLock();
				for (;;) {
					auto id = Entity::Id(stream->Read<int32_t>());
					if (id < 0) break;
					auto orientation = Entity::Orientation(stream->Read<glm::quat>());
					if (ClientSideEntity::Ptr entity = ClientSideEntity::Get(id); entity) {
						entity->targetOrientation = orientation;
					}
				}
			}break;
			
			default: 
				LOG_ERROR("Client ReceiveBurst UNRECOGNIZED MODULE ACTION " << std::to_string((int)action))
			break;
		}
	}
	
	#pragma endregion
	
};
