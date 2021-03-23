#pragma once

#include <v4d.h>

struct LinearPhysics {
	glm::dvec3 force;
	glm::dvec3 linearAcceleration;
	glm::dvec3 linearVelocity;
	glm::dvec3 position;
};

struct AngularPhysics {
	glm::dvec3 torque;
	glm::dvec3 angularAcceleration;
	glm::dvec3 angularVelocity;
	glm::dquat orientation;
};

struct Renderable {
	glm::mat4 transform;
};

class Entity {
	V4D_ENTITY_DECLARE_CLASS(Entity)
	V4D_ENTITY_DECLARE_COMPONENT(Entity, LinearPhysics, linearPhysics)
	V4D_ENTITY_DECLARE_COMPONENT(Entity, AngularPhysics, angularPhysics)
	
	using Type = uint64_t;
	using Id = uint64_t;
	using Parent = uint64_t;
	using Extra = uint64_t; 
	using Attributes = uint64_t;
	using Iteration = uint64_t;
	
	v4d::modular::ModuleID moduleID;
	Type type;
	Id id;
	Parent parent;
	Extra extra;
	Iteration iteration;
	Attributes attributes;
	
};

/*

Server-side:
	Physics thread:
		Compute collider positions from Physics
		Loop through colliders, make collision candidates
		Loop through collision candidates, apply forces and torques to Physics
		Loop through Physics, apply forces to velocities and torques to angularVelocities, reset forces and torques
		Loop through Physics, apply velocities to positions and angularVelocities to orientations
		Loop through Constraints, adjust positions, velocities, angularVelocities, orientations
		Copy positions to Network
	Game thread:
		Module-specific updates
		Send data over network
	Burst Network thread:
		Send positions over Network

Client-side:
	Burst Network thread:
		Receive to Network
	Game thread:
		...
	Rendering thread:
		Interpolate from Network to Rendering
		Loop through Rendering, add BLAS instances

*/
