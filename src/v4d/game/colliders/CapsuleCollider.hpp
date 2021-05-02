#pragma once 
#include "../Collider.hpp"

struct CapsuleCollider : Collider {

	float length;
	float radius;
	
	CapsuleCollider(const glm::dvec3& position, const glm::dmat3& rotation, float length, float radius)
	: Collider(position, rotation)
	, length(length)
	, radius(radius)
	{}

	bool RayIntersect(const Ray& ray, Entity* entity, double& t1, double& t2) {
		// const glm::dvec3 position = entity->position + glm::mat3_cast(entity->orientation) * this->position;
		// const glm::dmat3 rotation = glm::mat3_cast(entity->orientation) * this->rotation;
		
		// const glm::dvec3 localRayOrigin = glm::inverse(rotation) * (ray.origin - position);
		// const glm::dvec3 localRayDirection = glm::normalize(glm::inverse(rotation) * ray.direction);
		
		// const glm::dvec3 p = 
		
		
		// Ray-Marching?
		// from inside->out
		
		return false;
	}

	virtual bool RayCollision(const Ray& ray, Entity* entity, CollisionInfo& collision) override {
		// const glm::dvec3 position = entity->position + glm::mat3_cast(entity->orientation) * this->position;
		// const glm::dmat3 rotation = glm::mat3_cast(entity->orientation) * this->rotation;
		
		return false;
	}
	
	virtual bool TerrainCollision(const std::function<double(const glm::dvec3&)>& terrainHeightMap, Entity* entity, TerrainCollisionInfo& collision) override {
		const glm::dvec3 position = entity->position + glm::mat3_cast(entity->orientation) * this->position;
		const glm::dmat3 rotation = glm::mat3_cast(entity->orientation) * this->rotation;
		
		constexpr int NB_POINTS = 3;
		constexpr float NORMEPSILON = 0.01f; // offset to make the line along the capsule not perfectly straight to make it possible to calculate tangeants to get the normal
		
		glm::dvec3 points[NB_POINTS];
		glm::dvec3 normalizedPos[NB_POINTS];
		double terrainHeight[NB_POINTS];
		double altitudeAboveTerrain[NB_POINTS];
		
		normalizedPos[0] = glm::normalize(position);
		const glm::dvec3 downOffset = normalizedPos[0]*double(radius);
		points[0] = position - downOffset;
		points[1] = position + rotation * glm::dvec3(NORMEPSILON,0, length/2) - downOffset;
		points[2] = position + rotation * glm::dvec3(0, -NORMEPSILON, -length/2) - downOffset;
		normalizedPos[1] = glm::normalize(points[1]);
		normalizedPos[2] = glm::normalize(points[2]);
		
		double contactPoints = 0;
		for (int i = 0; i < NB_POINTS; ++i) {
			terrainHeight[i] = terrainHeightMap(normalizedPos[i]);
			altitudeAboveTerrain[i] = glm::length(points[i]) - terrainHeight[i];

			if (altitudeAboveTerrain[i] < 0) {
				++contactPoints;
				const double mixRatio = 1.0/contactPoints;
				
				collision.penetration = glm::mix(collision.penetration, float(-altitudeAboveTerrain[i]), mixRatio);
				collision.contactTerrain = glm::mix(collision.contactTerrain, points[i], mixRatio);
				collision.contactB = glm::mix(collision.contactB, points[i], mixRatio);
				
				// Compute terrain surface normal
				glm::dvec3 tangentXPos = glm::normalize(glm::cross(normalizedPos[i], glm::dvec3{0,1,0}));
				glm::dvec3 tangentYPos = glm::normalize(glm::cross(normalizedPos[i], tangentXPos));
				tangentXPos = glm::normalize(position + tangentXPos*double(radius));
				tangentYPos = glm::normalize(position + tangentYPos*double(radius));
				const glm::dvec3 posOnTerrain = normalizedPos[i] * terrainHeight[i];
				const glm::dvec3 tangentX = glm::normalize(tangentXPos*(terrainHeightMap(tangentXPos)) - posOnTerrain);
				const glm::dvec3 tangentY = glm::normalize(tangentYPos*(terrainHeightMap(tangentYPos)) - posOnTerrain);
				collision.normal = glm::mix(glm::dvec3(collision.normal), glm::normalize(glm::cross(tangentX, tangentY)), mixRatio);
			}
		}
		
		return contactPoints > 0;
	}
	
	virtual void GenerateCollisionRays(Entity* entity, const v4d::TextID& id, std::vector<Ray>& rays, uint& randomSeed) override {
		const glm::dvec3 position = entity->position + glm::mat3_cast(entity->orientation) * this->position;
		const glm::dmat3 rotation = glm::mat3_cast(entity->orientation) * this->rotation;
		
		const int RAYS_IN_CAPSULE_BODY = 1;
		for (int i = 0; i < RAYS_IN_CAPSULE_BODY; ++i) {
			const double randomHeight = (RandomFloat(randomSeed)-0.5) * length;
			const glm::vec2 randomDir = glm::normalize(RandomInUnitCircle(randomSeed)) * radius;
			const glm::dvec3 randomPointAtCenter = position + rotation * glm::dvec3(0, 0, randomHeight);
			const glm::dvec3 randomPoint = position + rotation * glm::dvec3(randomDir, randomHeight);
			rays.emplace_back(randomPoint, glm::normalize(randomPointAtCenter - randomPoint), radius, id);
		}
		const glm::dvec3 top = position + rotation * glm::dvec3(0, 0, length/2 + radius);
		const glm::dvec3 bottom = position + rotation * glm::dvec3(0, 0, -length/2 - radius);
		rays.emplace_back(top, glm::normalize(position - top), length/2 + radius, id);
		rays.emplace_back(bottom, glm::normalize(position - bottom), length/2 + radius, id);
	}

};
