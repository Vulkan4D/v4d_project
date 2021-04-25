#pragma once 
#include "../Collider.hpp"

struct TriangleCollider : Collider {
	// triangle should be oriented with line 0-1 towards +Y, and 0-2 towards +X+Y. Up is +Z, triangle lays flat on Z=0. Center is the position of the center of the triangle mesh (relative to position or vertex0)
	
	float vertex1_y;
	glm::vec2 vertex2;
	glm::vec3 center;
	
	TriangleCollider(const glm::dvec3& position, const glm::dmat3& rotation, float vertex1_y, const glm::vec2& vertex2_xy, const glm::vec3& center)
	: Collider(position, rotation)
	, vertex1_y(vertex1_y)
	, vertex2(vertex2_xy)
	, center(center)
	{}

	TriangleCollider(glm::dvec3 vertex0, glm::dvec3 vertex1, glm::dvec3 vertex2, glm::dvec3 center)
	: Collider(vertex0)
	{
		const glm::dvec3 v1 = vertex1 - vertex0;
		const glm::dvec3 v2 = vertex2 - vertex0;
		this->rotation = glm::mat3_cast(glm::quatLookAt(glm::normalize(glm::cross(glm::normalize(v1), glm::normalize(v2))), glm::normalize(v1)));
		this->vertex1_y = float((glm::inverse(rotation) * v1).y);
		this->vertex2 = glm::inverse(rotation) * v2;
		this->center = glm::inverse(rotation) * (center - vertex0);
	}

	bool RayIntersect(const Ray& ray, Entity* entity, double& u, double& v, double& t, glm::dvec3& normal) {
		const glm::dvec3 position = entity->position + glm::mat3_cast(entity->orientation) * this->position;
		const glm::dmat3 rotation = glm::mat3_cast(entity->orientation) * this->rotation;
		
		const glm::dvec3 localRayOrigin = glm::inverse(rotation) * (ray.origin - position);
		const glm::dvec3 localRayDirection = glm::normalize(glm::inverse(rotation) * ray.direction);
		
		// const glm::dvec3 v0 {0,0,0};
		const glm::dvec3 v1 {0,vertex1_y,0};
		const glm::dvec3 v2 {vertex2,0};
		
		const glm::dvec3 pvec = glm::cross(localRayDirection, v2);
		const double det = glm::dot(v1, pvec);
		
		// if (glm::abs(det) < DOUBLE_EPSILON) return false; // ray is parallel to the triangle
		
		const double invDet = 1.0 / det;
		
		// Compute u
		u = glm::dot(localRayOrigin, pvec) * invDet;
		if (u < 0 || u > 1) return false;
		
		const glm::dvec3 qvec = glm::cross(localRayOrigin, v1);
		
		// Compute v
		v = glm::dot(localRayDirection, qvec) * invDet;
		if (v < 0 || u+v > 1) return false;
		
		// Compute t
		t = glm::dot(v2, qvec) * invDet;
		if (t < 0 || t > ray.length) return false;
		
		// Compute normal
		normal = glm::normalize(rotation * glm::dvec3(0,0,glm::sign(det)));
		
		// Make sure normal direction is opposite of triangle center direction
		if (glm::dot(normal, rotation * center) > 0) normal *= -1;
		
		return true;
	}

	virtual bool RayCollision(const Ray& ray, Entity* entity, CollisionInfo& collision) override {
		double u,v,t;
		glm::dvec3 normal;
		if (RayIntersect(ray, entity, u, v, t, normal)) {
			collision.penetration = float(t);
			collision.contactA = ray.origin;
			collision.contactB = ray.origin + ray.direction * t;
			// collision.normal = normal;
			collision.normal = -ray.direction;
			return true;
		}
		return false;
	}
	
	virtual bool TerrainCollision(const std::function<double(const glm::dvec3&)>& terrainHeightMap, Entity* entity, TerrainCollisionInfo& collision) override {
		const glm::dvec3 position = entity->position + glm::mat3_cast(entity->orientation) * this->position;
		const glm::dmat3 rotation = glm::mat3_cast(entity->orientation) * this->rotation;
		
		const glm::dvec3& p0 = position;
		const glm::dvec3 p1 = position + rotation * glm::dvec3{0,vertex1_y,0};
		const glm::dvec3 p2 = position + rotation * glm::dvec3{vertex2,0};
		
		const glm::dvec3 normalizedPos0 = glm::normalize(p0);
		const glm::dvec3 normalizedPos1 = glm::normalize(p1);
		const glm::dvec3 normalizedPos2 = glm::normalize(p2);
		const double terrainHeight0 = terrainHeightMap(normalizedPos0);
		const double terrainHeight1 = terrainHeightMap(normalizedPos1);
		const double terrainHeight2 = terrainHeightMap(normalizedPos2);
		const double altitudeAboveTerrain0 = glm::length(p0) - terrainHeight0;
		const double altitudeAboveTerrain1 = glm::length(p1) - terrainHeight1;
		const double altitudeAboveTerrain2 = glm::length(p2) - terrainHeight2;
		
		if (altitudeAboveTerrain0 < 0) {
			collision.penetration = float(-altitudeAboveTerrain0);
			collision.contactTerrain = p0;
			collision.contactB = p0;
			goto FoundIntersection;
		}
		
		if (altitudeAboveTerrain1 < 0) {
			collision.penetration = float(-altitudeAboveTerrain1);
			collision.contactTerrain = p1;
			collision.contactB = p1;
			goto FoundIntersection;
		}
		
		if (altitudeAboveTerrain2 < 0) {
			collision.penetration = float(-altitudeAboveTerrain2);
			collision.contactTerrain = p2;
			collision.contactB = p2;
			goto FoundIntersection;
		}
		
		// for (int i = 0; i < 4; ++i) {// Add a few Random points for good measure... //TODO maybe have a logic that makes it depend on the size of the triangles
		// 	const glm::dvec3 pRandom = position + rotation * glm::dvec3(RandomInTriangle(randomSeed, glm::vec2{0,vertex1_y}, vertex2), 0);
		// 	const glm::dvec3 normalizedPosRandom = glm::normalize(pRandom);
		// 	const double terrainHeightRandom = terrainHeightMap(normalizedPosRandom);
		// 	const double altitudeAboveTerrainRandom = glm::length(pRandom) - terrainHeightRandom;
		// 	if (altitudeAboveTerrainRandom < 0) {
		// 		collision.penetration = float(-altitudeAboveTerrainRandom);
		// 		collision.contactTerrain = pRandom;
		// 		collision.contactB = pRandom;
		// 		goto FoundIntersection;
		// 	}
		// }
		
		return false;
		
		FoundIntersection:
		
			// Compute terrain surface normal
			const glm::dvec3 posOnTerrain0 = normalizedPos0 * terrainHeight0;
			const glm::dvec3 posOnTerrain1 = normalizedPos1 * terrainHeight1;
			const glm::dvec3 posOnTerrain2 = normalizedPos2 * terrainHeight2;
			const glm::dvec3 tangentX = glm::normalize(posOnTerrain1 - posOnTerrain0);
			const glm::dvec3 tangentY = glm::normalize(posOnTerrain2 - posOnTerrain0);
			
			collision.normal = glm::normalize(glm::cross(tangentX, tangentY));
			if (glm::dot(collision.normal, glm::vec3(normalizedPos0)) < 0) collision.normal *= -1.0;
		
		return true;
	}
	
	virtual void GenerateCollisionRays(Entity* entity, const v4d::TextID& id, std::vector<Ray>& rays, uint& randomSeed) override {
		const glm::dvec3 position = entity->position + glm::mat3_cast(entity->orientation) * this->position;
		const glm::dmat3 rotation = glm::mat3_cast(entity->orientation) * this->rotation;
		
		// const glm::dvec3& p0 = position;
		// const glm::dvec3 p1 = position + rotation * glm::dvec3{0,vertex1_y,0};
		// const glm::dvec3 p2 = position + rotation * glm::dvec3{vertex2,0};
		
		constexpr int RAYS_PER_TRIANGLE = 1;
		const glm::dvec3 pCenter = position + rotation * glm::dvec3(center);
		for (int i = 0; i < RAYS_PER_TRIANGLE; ++i) {
			const glm::dvec3 pRandom = position + rotation * glm::dvec3(RandomInTriangle(randomSeed, glm::vec2{0,vertex1_y}, vertex2), 0);
			const glm::dvec3 vRay = pCenter - pRandom;
			rays.emplace_back(pRandom, glm::normalize(vRay), glm::length(vRay), id);
		}
	}

};
