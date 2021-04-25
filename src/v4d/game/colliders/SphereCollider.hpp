#pragma once 
#include "../Collider.hpp"

struct SphereCollider : Collider {
	
	float radius;
	
	SphereCollider(const glm::dvec3& position, float radius)
	: Collider(position)
	, radius(radius)
	{}
	
	SphereCollider(float radius)
	: Collider()
	, radius(radius)
	{}
	
	bool RayIntersect(const Ray& ray, Entity* entity, double& t1, double& t2) {
		const glm::dvec3 position = entity->position + glm::mat3_cast(entity->orientation) * this->position;
		// const glm::dmat3 rotation = glm::mat3_cast(entity->orientation) * this->rotation;
		
		const glm::dvec3 oc = ray.origin - position;
		const double a = glm::dot(ray.direction, ray.direction);
		const double b = glm::dot(oc, ray.direction);
		const double c = glm::dot(oc, oc) - radius*radius;
		const double discriminantSqr = b*b - a*c;
		if (discriminantSqr >= 0) {
			const double discriminant = glm::sqrt(discriminantSqr);
			t1 = (-b - discriminant) / a;
			t2 = (-b + discriminant) / a;
			if (t1 <= ray.length && t2 > 0) {
				return true;
			}
		}
		return false;
	}

	virtual bool RayCollision(const Ray& ray, Entity* entity, CollisionInfo& collision) override {
		double t1, t2;
		if (RayIntersect(ray, entity, t1, t2) && t2 < ray.length) {
			collision.penetration = float(t2);
			collision.contactA = ray.origin;
			collision.contactB = ray.origin + ray.direction * t2;
			collision.normal = -ray.direction;
			return true;
		}
		return false;
	}
	
	virtual bool TerrainCollision(const std::function<double(const glm::dvec3&)>& terrainHeightMap, Entity* entity, TerrainCollisionInfo& collision) override {
		const glm::dvec3 position = entity->position + glm::mat3_cast(entity->orientation) * this->position;
		// const glm::dmat3 rotation = glm::mat3_cast(entity->orientation) * this->rotation;
		
		const glm::dvec3 normalizedPos = glm::normalize(position);
		const double terrainHeight = terrainHeightMap(normalizedPos);
		const double altitudeAboveTerrain = glm::length(position) - radius - terrainHeight;
		
		if (altitudeAboveTerrain < 0) {
			glm::dvec3 tangentXPos = glm::normalize(glm::cross(normalizedPos, glm::dvec3{0,1,0}));
			glm::dvec3 tangentYPos = glm::normalize(glm::cross(normalizedPos, tangentXPos));
			tangentXPos = glm::normalize(position + tangentXPos*double(radius*0.5));
			tangentYPos = glm::normalize(position + tangentYPos*double(radius*0.5));
			const glm::dvec3 posOnTerrain = normalizedPos * terrainHeight;
			const glm::dvec3 tangentX = glm::normalize(tangentXPos*(terrainHeightMap(tangentXPos)) - posOnTerrain);
			const glm::dvec3 tangentY = glm::normalize(tangentYPos*(terrainHeightMap(tangentYPos)) - posOnTerrain);
			
			collision.normal = glm::normalize(glm::cross(tangentX, tangentY));
			collision.penetration = float(-altitudeAboveTerrain);
			collision.contactTerrain = position;
			collision.contactB = position - glm::dvec3(collision.normal * radius);
			
			return true;
		}
		return false;
	}
	
	virtual void GenerateCollisionRays(Entity* entity, const v4d::TextID& id, std::vector<Ray>& rays, uint& randomSeed) override {
		const glm::dvec3 position = entity->position + glm::mat3_cast(entity->orientation) * this->position;
		// const glm::dmat3 rotation = glm::mat3_cast(entity->orientation) * this->rotation;
		
		const int RAYS_PER_SPHERE = 1;
		for (int i = 0; i < RAYS_PER_SPHERE; ++i) {
			glm::dvec3 randomPoint = glm::normalize(RandomInUnitSphere(randomSeed));
			rays.emplace_back(position + randomPoint * double(radius), -randomPoint, radius, id);
		}
		// Add an additional ray at the very bottom
		glm::dvec3 bottomPoint = -glm::normalize(position);
		rays.emplace_back(position + bottomPoint * double(radius), -bottomPoint, radius, id);

	}

};
