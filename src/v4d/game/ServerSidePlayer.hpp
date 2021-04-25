#pragma once

#include <v4d.h>

#include "ServerSideEntity.hpp"

struct EntitySubscription {
	ServerSideEntity::WeakPtr entity;
	Entity::Iteration iteration;
	EntitySubscription() : iteration(0) {}
	EntitySubscription(ServerSideEntity::Ptr& entity, Entity::Iteration iteration = 0)
	: entity(entity)
	, iteration(iteration)
	{}
};

struct V4DGAME ServerSidePlayer {
	V4D_ENTITY_DECLARE_CLASS_MAP(ServerSidePlayer)
	
	// GetID() is client_id
	Entity::ReferenceFrame referenceFrame {0};
	Entity::ReferenceFrameExtra referenceFrameExtra {0};
	Entity::Id parentEntityId {-1};
	
	ServerSideEntity::Ptr GetServerSideEntity() const {
		if (parentEntityId < 0) return nullptr;
		return ServerSideEntity::Get(parentEntityId);
	}
	
	// Entity Subscriptions
private: mutable std::mutex subscriptionsMutex;
public:
	std::unordered_map<Entity::Id, EntitySubscription> entitySubscriptions {};
	std::unordered_map<Entity::Id, ServerSideEntity::WeakPtr> dynamicEntitySubscriptions {};
	std::unique_lock<std::mutex> GetSubscriptionLock() const {
		return std::unique_lock<std::mutex>{subscriptionsMutex};
	}
};
