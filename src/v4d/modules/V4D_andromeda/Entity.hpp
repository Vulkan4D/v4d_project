#pragma once

#include <v4d.h>

struct Rigidbody {
	double mass;
	glm::dmat3 inertia;
	glm::dvec3 centerOfMass;
};
struct RigidbodyIntegration {
	// Initial Data
	double invMass;
	glm::dmat3 invInertia;
	
	// Integration Data
	glm::dvec3 force;
	glm::dvec3 torque;
	glm::dvec3 linearAcceleration;
	glm::dvec3 angularAcceleration;
	glm::dvec3 linearVelocity;
	glm::dvec3 angularVelocity;
	
	// Final Data
	glm::dvec3 position;
	glm::dquat orientation;
};

struct Collider {
	uint64_t id; //TODO static increment like physicsInfo
	uint64_t referenceFrame;
	glm::dmat4 offset;
	// shape...
};
struct ColliderBroadphaseIntegration {
	uint64_t referenceFrame;
	glm::dvec3 boundingBoxMin;
	glm::dvec3 boundingBoxMax;
};

struct NetworkTransformIntegration {
	glm::dvec3 targetPosition;
	glm::dquat targetOrientation;
	glm::dvec3 currentPosition;
	glm::dquat currentOrientation;
	glm::dmat4 worldTransform;
	float interpolation;
};

struct Renderable {
	uint32_t id; //TODO static increment like physicsInfo
	glm::dmat4 offset;
	// geometries, ....
};
struct RenderableIntegration {
	glm::dmat4 worldTransform;
	glm::mat4 modelViewTransform;
};

class Entity {
	V4D_ENTITY_DECLARE_CLASS(Entity)
	
	V4D_ENTITY_DECLARE_COMPONENT(Entity, Rigidbody, rigidbody)
	V4D_ENTITY_DECLARE_COMPONENT(Entity, RigidbodyIntegration, rigidbodyIntegration)
	
	V4D_ENTITY_DECLARE_COMPONENT_MAP(Entity, std::string_view, Collider, collider)
	V4D_ENTITY_DECLARE_COMPONENT_MAP(Entity, std::string_view, ColliderBroadphaseIntegration, colliderBroadphaseIntegration)
	
	V4D_ENTITY_DECLARE_COMPONENT(Entity, NetworkTransformIntegration, networkTransformIntegration)
	
	V4D_ENTITY_DECLARE_COMPONENT_MAP(Entity, std::string_view, Renderable, renderable)
	V4D_ENTITY_DECLARE_COMPONENT_MAP(Entity, std::string_view, RenderableIntegration, renderableIntegration)
	
	using Type = uint16_t;
	using Attributes = uint32_t;
	using Iteration = uint32_t;
	using Id = uint64_t;
	using Parent = uint64_t;
	using Extra = uint64_t; 
	
	v4d::modular::ModuleID moduleID;
	Type type;
	Id id;
	Parent parent;
	Extra extra;
	Iteration iteration;
	Attributes attributes;
	
};
