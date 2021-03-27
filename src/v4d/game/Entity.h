#pragma once

#include <v4d.h>
#include <string_view>
#include "utilities/io/Logger.h"
#include "utilities/graphics/vulkan/Loader.h"
#include "utilities/data/EntityComponentSystem.hpp"

#include "utilities/graphics/RenderableGeometryEntity.h"

// struct Rigidbody {
// 	double mass;
// 	glm::dmat3 inertia;
// 	glm::dvec3 centerOfMass;
// };
// struct RigidbodyIntegration {
// 	// Initial Data
// 	double invMass;
// 	glm::dmat3 invInertia;
	
// 	// Integration Data
// 	glm::dvec3 force;
// 	glm::dvec3 torque;
// 	glm::dvec3 linearAcceleration;
// 	glm::dvec3 angularAcceleration;
// 	glm::dvec3 linearVelocity;
// 	glm::dvec3 angularVelocity;
	
// 	// Final Data
// 	glm::dvec3 position;
// 	glm::dquat orientation;
// };

// struct Collider {
// 	uint64_t id; //TODO static increment like physicsInfo
// 	uint64_t referenceFrame;
// 	glm::dmat4 offset;
// 	// shape...
// };
// struct ColliderBroadphaseIntegration {
// 	uint64_t referenceFrame;
// 	glm::dvec3 boundingBoxMin;
// 	glm::dvec3 boundingBoxMax;
// };

// struct Renderable {
// 	uint32_t id; //TODO static increment like physicsInfo
// 	glm::dmat4 offset;
// 	// geometries, ....
// };
// struct RenderableIntegration {
// 	glm::dmat4 worldTransform;
// 	glm::mat4 modelViewTransform;
// };



	// void NetworkGameObject::SetAttributes(Attributes attrs) {
	// 	for (size_t i = 0; i < attributesPtrs.size(); ++i) {
	// 		*attributesPtrs[i] = attrs & (1 << i);
	// 	}
	// }
	// NetworkGameObject::Attributes NetworkGameObject::GetAttributes() const {
	// 	Attributes attrs = 0;
	// 	for (size_t i = 0; i < attributesPtrs.size(); ++i) {
	// 		attrs |= (*attributesPtrs[i]? 1:0) << i;
	// 	}
	// 	return attrs;
	// }



struct Entity {
	using Id = v4d::data::EntityComponentSystem::EntityIndex_T;
	using Type = uint64_t;
	
	using ReferenceFrame = uint64_t;
	using ReferenceFrameExtra = uint64_t;
	using Position = glm::dvec3;
	using Orientation = glm::dquat;
	
	using Iteration = uint64_t;
	
	Entity() {};
	
	v4d::modular::ModuleID moduleID {0,0};
	Type type {0};
	
	ReferenceFrame referenceFrame {0};
	ReferenceFrameExtra referenceFrameExtra {0};
	Position position {0};
	Orientation orientation {1,0,0,0};
	
	inline void operator()(v4d::modular::ModuleID moduleID) {
		this->moduleID = moduleID;
	}
	inline void operator()(v4d::modular::ModuleID moduleID, Type type) {
		operator()(moduleID);
		this->type = type;
	}

	inline void operator()(v4d::modular::ModuleID moduleID, Type type, ReferenceFrame referenceFrame) {
		operator()(moduleID, type);
		this->referenceFrame = referenceFrame;
	}
	inline void operator()(v4d::modular::ModuleID moduleID, Type type, ReferenceFrame referenceFrame, ReferenceFrameExtra referenceFrameExtra) {
		operator()(moduleID, type, referenceFrame);
		this->referenceFrameExtra = referenceFrameExtra;
	}
	inline void operator()(v4d::modular::ModuleID moduleID, Type type, ReferenceFrame referenceFrame, Position position, Orientation orientation = {1,0,0,0}) {
		operator()(moduleID, type, referenceFrame);
		this->position = position;
		this->orientation = orientation;
	}
	inline void operator()(v4d::modular::ModuleID moduleID, Type type, ReferenceFrame referenceFrame, ReferenceFrameExtra referenceFrameExtra, Position position, Orientation orientation = {1,0,0,0}) {
		operator()(moduleID, type, referenceFrame, referenceFrameExtra);
		this->position = position;
		this->orientation = orientation;
	}
};

struct V4DGAME ServerSideEntity : Entity {
	V4D_ENTITY_DECLARE_CLASS_MAP(ServerSideEntity)
	
	// V4D_ENTITY_DECLARE_COMPONENT(Entity, Rigidbody, rigidbody)
	// V4D_ENTITY_DECLARE_COMPONENT(Entity, RigidbodyIntegration, rigidbodyIntegration)
	
	// V4D_ENTITY_DECLARE_COMPONENT_MAP(Entity, std::string_view, Collider, collider)
	// V4D_ENTITY_DECLARE_COMPONENT_MAP(Entity, std::string_view, ColliderBroadphaseIntegration, colliderBroadphaseIntegration)
	
	bool active = false;
	bool isDynamic = false;
	std::atomic<Iteration> iteration {0};
	std::unordered_map<uint64_t/*clientID*/, Iteration /*iteration*/> clientIterations {};

	Iteration Iterate() {
		return ++iteration;
	}

	void Activate() {
		active = true;
		Iterate();
	}
};

struct V4DGAME ClientSideEntity : Entity {
	V4D_ENTITY_DECLARE_CLASS_MAP(ClientSideEntity)
	
	// V4D_ENTITY_DECLARE_COMPONENT_MAP(Entity, std::string_view, Renderable, renderable)
	// V4D_ENTITY_DECLARE_COMPONENT_MAP(Entity, std::string_view, RenderableIntegration, renderableIntegration)
	
	Iteration iteration {0};

	bool posInit = false;
	Position targetPosition {0};
	Orientation targetOrientation {1,0,0,0};
	
	// Temporary
		v4d::graphics::RenderableGeometryEntity::WeakPtr renderableGeometryEntityInstance;
		void UpdateRenderable() {
			if (auto renderableEntity = renderableGeometryEntityInstance.lock(); renderableEntity) {
				renderableEntity->parentId = referenceFrame;
				renderableEntity->SetLocalTransform(glm::translate(glm::dmat4(1), position) * glm::mat4_cast(orientation));
			}
		}
	
	inline void operator()(v4d::modular::ModuleID moduleID, Type type, ReferenceFrame referenceFrame, Position position, Orientation orientation = {1,0,0,0}) {
		Entity::operator()(moduleID, type, referenceFrame, position, orientation);
		this->targetPosition = targetPosition;
		this->targetOrientation = targetOrientation;
		this->posInit = true;
	}
	inline void operator()(v4d::modular::ModuleID moduleID, Type type, ReferenceFrame referenceFrame, ReferenceFrameExtra referenceFrameExtra, Position position, Orientation orientation = {1,0,0,0}) {
		Entity::operator()(moduleID, type, referenceFrame, referenceFrameExtra, position, orientation);
		this->targetPosition = targetPosition;
		this->targetOrientation = targetOrientation;
		this->posInit = true;
	}
	inline void operator()(v4d::modular::ModuleID moduleID, Type type, ReferenceFrame referenceFrame, ReferenceFrameExtra referenceFrameExtra, Position position, Orientation orientation, Iteration iteration) {
		operator()(moduleID, type, referenceFrame, referenceFrameExtra, position, orientation);
		this->iteration = iteration;
	}
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
	
};
