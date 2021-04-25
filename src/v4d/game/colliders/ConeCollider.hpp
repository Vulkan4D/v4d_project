#pragma once 
#include "../Collider.hpp"

struct ConeCollider : Collider {
	
	float length;
	float radiusA;
	float radiusB;
	
	ConeCollider(const glm::dvec3& position, const glm::dmat3& rotation, float length, float radiusA, float radiusB)
	: Collider(position, rotation)
	, length(length)
	, radiusA(radiusA)
	, radiusB(radiusB)
	{}

	bool RayIntersect(const Ray& ray, Entity* entity, double& t1, double& t2) {
		// const glm::dvec3 position = entity->position + glm::mat3_cast(entity->orientation) * this->position;
		// const glm::dmat3 rotation = glm::mat3_cast(entity->orientation) * this->rotation;
		
		return false;
	}

	virtual bool RayCollision(const Ray& ray, Entity* entity, CollisionInfo& collision) override {
		// const glm::dvec3 position = entity->position + glm::mat3_cast(entity->orientation) * this->position;
		// const glm::dmat3 rotation = glm::mat3_cast(entity->orientation) * this->rotation;
		
		return false;
	}
	
	virtual bool TerrainCollision(const std::function<double(const glm::dvec3&)>& terrainHeightMap, Entity* entity, TerrainCollisionInfo& collision) override {
		// const glm::dvec3 position = entity->position + glm::mat3_cast(entity->orientation) * this->position;
		// const glm::dmat3 rotation = glm::mat3_cast(entity->orientation) * this->rotation;
		
		
		return false;
	}
	
	virtual void GenerateCollisionRays(Entity* entity, const v4d::TextID& id, std::vector<Ray>& rays, uint& randomSeed) override {
		// const glm::dvec3 position = entity->position + glm::mat3_cast(entity->orientation) * this->position;
		// const glm::dmat3 rotation = glm::mat3_cast(entity->orientation) * this->rotation;
		
		
	}

};
