#pragma once

#include "Entity.h"
#include "physics.hh"

struct Collider {
	
	glm::dvec3 position;
	glm::dmat3 rotation;
	
	Collider(const glm::dvec3& position = {0,0,0}, const glm::dmat3& rotation = glm::dmat3{1})
	: position(position)
	, rotation(rotation)
	{}
	
	virtual ~Collider() = default;
	
	virtual bool RayCollision(const Ray&, Entity*, CollisionInfo&) = 0;
	virtual bool TerrainCollision(const std::function<double(const glm::dvec3&)>&, Entity*, TerrainCollisionInfo&) = 0;
	virtual void GenerateCollisionRays(Entity*, const v4d::TextID&, std::vector<Ray>&, uint& randomSeed) = 0;
	
};

#include "random.hh"

#include "colliders/BoxCollider.hpp"
#include "colliders/SphereCollider.hpp"
#include "colliders/CapsuleCollider.hpp"
#include "colliders/CylinderCollider.hpp"
#include "colliders/RingCollider.hpp"
#include "colliders/TriangleCollider.hpp"
#include "colliders/ConeCollider.hpp"
