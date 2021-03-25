#pragma once

#include <v4d.h>

struct Rigidbody {
	// Linear
	double invMass;
	glm::dvec3 force;
	glm::dvec3 linearAcceleration;
	glm::dvec3 linearVelocity;
	glm::dvec3 position;
	
	// Angular
	glm::dmat3 invInertia;
	glm::dvec3 torque;
	glm::dvec3 angularAcceleration;
	glm::dvec3 angularVelocity;
	glm::dquat orientation;
};

struct Renderable {
	glm::dmat4 worldTransform;
	glm::mat4 modelViewTransform;
};

class Entity {
	V4D_ENTITY_DECLARE_CLASS(Entity)
	V4D_ENTITY_DECLARE_COMPONENT(Entity, Rigidbody, rigidbody)
	
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
