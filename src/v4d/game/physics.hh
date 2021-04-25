#pragma once

#include <v4d.h>
#include "Entity.h"

#define DOUBLE_EPSILON 0.000000001

#define COLLISION_REST_SPEED_THRESHOLD 0.04
#define COLLISION_REST_ANGULAR_SPEED_THRESHOLD 0.05
#define COLLISION_STATIC_FRICTION_SPEED_REDUCTION 0.5 // should be less than COLLISION_REST_SPEED_THRESHOLD / avgDeltaTime
#define COLLISION_STATIC_FRICTION_ANGULAR_SPEED_REDUCTION 0.8 // should be less than COLLISION_REST_ANGULAR_SPEED_THRESHOLD / avgDeltaTime

#define TERRAIN_COLLISION_REST_SPEED_THRESHOLD 0.04
#define TERRAIN_COLLISION_REST_ANGULAR_SPEED_THRESHOLD 0.05
#define TERRAIN_COLLISION_STATIC_FRICTION_SPEED_REDUCTION 0.5 // should be less than TERRAIN_COLLISION_REST_SPEED_THRESHOLD / avgDeltaTime
#define TERRAIN_COLLISION_STATIC_FRICTION_ANGULAR_SPEED_REDUCTION 0.8 // should be less than TERRAIN_COLLISION_REST_ANGULAR_SPEED_THRESHOLD / avgDeltaTime


struct BroadphaseCollider {
	glm::dvec3 position;
	double radius;
	Entity::Id id;
	BroadphaseCollider(glm::dvec3 position, double radius, Entity::Id id)
	 : position(position)
	 , radius(radius)
	 , id(id)
	{}
};

struct CollisionInfo {
	glm::vec3 normal; // normalize(B-A) in world space
	float penetration; // amount of penetration as positive number
	glm::dvec3 contactA; // hit position on entity A in world space
	glm::dvec3 contactB; // hit position on entity B in world space
	CollisionInfo() {}
};

struct TerrainType {
	double restitution = 0.4;
	double friction = 0.8;
	//...
};

struct TerrainCollisionInfo {
	uint64_t celestialIdInGalaxy;
	TerrainType terrainType;
	
	glm::vec3 normal; // upwards or terrain surface normal (world space)
	float penetration;
	glm::dvec3 contactTerrain; // world space
	glm::dvec3 contactB; // position in world space
	
	TerrainCollisionInfo(uint64_t celestialIdInGalaxy, TerrainType terrainType)
	 : celestialIdInGalaxy(celestialIdInGalaxy)
	 , terrainType(terrainType)
	{}
};

struct Ray {
	glm::dvec3 origin;
	glm::dvec3 direction;
	double length;
	uint64_t info;
	Ray(const glm::dvec3& origin, const glm::dvec3& direction, const double& length, const uint64_t& info = 0)
	: origin(origin)
	, direction(direction)
	, length(length)
	, info(info)
	{}
};
