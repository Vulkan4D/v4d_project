#pragma once 
#include "../Collider.hpp"

struct CylinderCollider : Collider {
	
	float length;
	float radius;
	
	CylinderCollider(const glm::dvec3& position, const glm::dmat3& rotation, float length, float radius)
	: Collider(position, rotation)
	, length(length)
	, radius(radius)
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
		const glm::dvec3 position = entity->position + glm::mat3_cast(entity->orientation) * this->position;
		const glm::dmat3 rotation = glm::mat3_cast(entity->orientation) * this->rotation;
		
		const int RAYS_IN_CYLINDER_BODY = 1;
		for (int i = 0; i < RAYS_IN_CYLINDER_BODY; ++i) {
			const double randomHeight = (RandomFloat(randomSeed)-0.5) * length * 0.8;
			const glm::vec2 randomDir = glm::normalize(RandomInUnitCircle(randomSeed)) * radius;
			const glm::dvec3 randomPointAtCenter = position + rotation * glm::dvec3(0, 0, randomHeight);
			const glm::dvec3 randomPoint = position + rotation * glm::dvec3(randomDir, randomHeight);
			rays.emplace_back(randomPoint, glm::normalize(randomPointAtCenter - randomPoint), radius, id);
		}
		const glm::dvec3 top = position + rotation * glm::dvec3(0, 0, length/2);
		const glm::dvec3 bottom = position + rotation * glm::dvec3(0, 0, -length/2);
		rays.emplace_back(top, glm::normalize(position - top), length/2, id);
		rays.emplace_back(bottom, glm::normalize(position - bottom), length/2, id);
	}

};
