#pragma once 
#include "../Collider.hpp"

struct BoxCollider : Collider {
	
	glm::vec3 halfSize;
	
	BoxCollider(const glm::dvec3& position, const glm::dmat3& rotation, const glm::vec3& halfSize)
	: Collider(position, rotation)
	, halfSize(halfSize)
	{}
	
	bool RayIntersect(const Ray& ray, Entity* entity, double& t1, double& t2) {
		const glm::dvec3 position = entity->position + glm::mat3_cast(entity->orientation) * this->position;
		const glm::dmat3 rotation = glm::mat3_cast(entity->orientation) * this->rotation;
		
		const glm::dvec3 localRayOrigin = glm::inverse(rotation) * (ray.origin - position);
		const glm::dvec3 localRayDirection = glm::normalize(glm::inverse(rotation) * ray.direction);
		
		const glm::dvec3 aabb_min = -halfSize;
		const glm::dvec3 aabb_max = +halfSize;
		const glm::dvec3 invDir = 1.0 / localRayDirection;
		const glm::dvec3 tbot   = invDir * (aabb_min - localRayOrigin);
		const glm::dvec3 ttop   = invDir * (aabb_max - localRayOrigin);
		const glm::dvec3 tmin   = glm::min(ttop, tbot);
		const glm::dvec3 tmax   = glm::max(ttop, tbot);
		t1 = glm::max(tmin.x, glm::max(tmin.y, tmin.z));
		t2 = glm::min(tmax.x, glm::min(tmax.y, tmax.z));
		
		return t1 < ray.length && t2 > 0 && t2 > t1;
	}

	virtual bool RayCollision(const Ray& ray, Entity* entity, CollisionInfo& collision) override {
		double t1, t2;
		if (RayIntersect(ray, entity, t1, t2) && t1 < 0 && t2 < ray.length) {
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
		const glm::dmat3 rotation = glm::mat3_cast(entity->orientation) * this->rotation;
		
		constexpr int MIN_TEST_VERTICES = 4; // test at least the first 4 lowest vertices of the box
		constexpr int MAX_TEST_VERTICES = 6; // test up to 6 lowest vertices of the box
		
		std::array points {
			position + rotation * (glm::dvec3{+1,+1,+1} * glm::dvec3(halfSize)),
			position + rotation * (glm::dvec3{+1,+1,-1} * glm::dvec3(halfSize)),
			position + rotation * (glm::dvec3{+1,-1,-1} * glm::dvec3(halfSize)),
			position + rotation * (glm::dvec3{-1,-1,-1} * glm::dvec3(halfSize)),
			position + rotation * (glm::dvec3{-1,-1,+1} * glm::dvec3(halfSize)),
			position + rotation * (glm::dvec3{-1,+1,+1} * glm::dvec3(halfSize)),
			position + rotation * (glm::dvec3{+1,-1,+1} * glm::dvec3(halfSize)),
			position + rotation * (glm::dvec3{-1,+1,-1} * glm::dvec3(halfSize)),
		};
		
		// Sort box vertices by lowest
		std::sort(points.begin(), points.end(), [](const glm::dvec3& a, const glm::dvec3& b){
			return glm::dot(a,a) < glm::dot(b,b);
		});
		
		glm::dvec3 normalizedPos[MAX_TEST_VERTICES];
		double terrainHeight[MAX_TEST_VERTICES];
		double altitudeAboveTerrain[MAX_TEST_VERTICES];
		double contactPoints = 0;
		
		for (int i = 0; i < MAX_TEST_VERTICES; ++i) {
			normalizedPos[i] = glm::normalize(points[i]);
			terrainHeight[i] = terrainHeightMap(normalizedPos[i]);
			altitudeAboveTerrain[i] = glm::length(points[i]) - terrainHeight[i];

			if (altitudeAboveTerrain[i] < 0) {
				++contactPoints;
				const double mixRatio = 1.0/contactPoints;
				
				collision.penetration = glm::mix(collision.penetration, float(-altitudeAboveTerrain[i]), mixRatio);
				collision.contactTerrain = glm::mix(collision.contactTerrain, points[i], mixRatio);
				collision.contactB = glm::mix(collision.contactB, points[i], mixRatio);
				
				if (contactPoints == MIN_TEST_VERTICES) break; // good enough!
			}
		}
		
		if (contactPoints > 0) {
			// Compute terrain surface normal
			const glm::dvec3 posOnTerrain0 = normalizedPos[0] * terrainHeight[0];
			const glm::dvec3 posOnTerrain1 = normalizedPos[1] * terrainHeight[1];
			const glm::dvec3 posOnTerrain2 = normalizedPos[2] * terrainHeight[2];
			const glm::dvec3 tangentX = glm::normalize(posOnTerrain1 - posOnTerrain0);
			const glm::dvec3 tangentY = glm::normalize(posOnTerrain2 - posOnTerrain0);
			collision.normal = glm::normalize(glm::cross(tangentX, tangentY));
			if (glm::dot(collision.normal, glm::vec3(normalizedPos[0])) < 0) collision.normal *= -1.0;
		}
		
		return contactPoints > 0;
	}
	
	virtual void GenerateCollisionRays(Entity* entity, const v4d::TextID& id, std::vector<Ray>& rays, uint& randomSeed) override {
		const glm::dvec3 position = entity->position + glm::mat3_cast(entity->orientation) * this->position;
		const glm::dmat3 rotation = glm::mat3_cast(entity->orientation) * this->rotation;
		
		// Launch rays stochastically
		constexpr int RAYS_PER_FACE = 1;
		constexpr bool AXIS_ALIGNED_RAYS = true;
		auto func = [this, &rays, &position, &rotation, &id](const glm::dvec3& point, const glm::dvec3& dir){
			const glm::dvec3 p = position + rotation * (point * glm::dvec3(halfSize));
			if constexpr(AXIS_ALIGNED_RAYS) {
				rays.emplace_back(p, rotation * -dir, glm::length(dir * glm::dvec3(halfSize)), id);
			} else {
				rays.emplace_back(p, -glm::normalize(p-position), glm::length(p-position), id);
			}
		};
		for (int i = 0; i < RAYS_PER_FACE; ++i) {
			func(glm::dvec3{+1, RandomFloat(randomSeed)*1.8-0.9, RandomFloat(randomSeed)*1.8-0.9}, {+1,0,0});// +x
			func(glm::dvec3{-1, RandomFloat(randomSeed)*1.8-0.9, RandomFloat(randomSeed)*1.8-0.9}, {-1,0,0});// -x
			func(glm::dvec3{RandomFloat(randomSeed)*1.8-0.9, +1, RandomFloat(randomSeed)*1.8-0.9}, {0,+1,0});// +y
			func(glm::dvec3{RandomFloat(randomSeed)*1.8-0.9, -1, RandomFloat(randomSeed)*1.8-0.9}, {0,-1,0});// -y
			func(glm::dvec3{RandomFloat(randomSeed)*1.8-0.9, RandomFloat(randomSeed)*1.8-0.9, +1}, {0,0,+1});// +z
			func(glm::dvec3{RandomFloat(randomSeed)*1.8-0.9, RandomFloat(randomSeed)*1.8-0.9, -1}, {0,0,-1});// -z
		}
		
		// // Launch rays in corners and center of faces
		// const std::array points {
		// 	// // Corner vertices
		// 	// position + rotation * (glm::dvec3{+1,+1,+1} * glm::dvec3(halfSize)),
		// 	// position + rotation * (glm::dvec3{+1,+1,-1} * glm::dvec3(halfSize)),
		// 	// position + rotation * (glm::dvec3{+1,-1,-1} * glm::dvec3(halfSize)),
		// 	// position + rotation * (glm::dvec3{-1,-1,-1} * glm::dvec3(halfSize)),
		// 	// position + rotation * (glm::dvec3{-1,-1,+1} * glm::dvec3(halfSize)),
		// 	// position + rotation * (glm::dvec3{-1,+1,+1} * glm::dvec3(halfSize)),
		// 	// position + rotation * (glm::dvec3{+1,-1,+1} * glm::dvec3(halfSize)),
		// 	// position + rotation * (glm::dvec3{-1,+1,-1} * glm::dvec3(halfSize)),
		// 	// Face Centers
		// 	position + rotation * (glm::dvec3{+1,0,0} * glm::dvec3(halfSize)),
		// 	position + rotation * (glm::dvec3{-1,0,0} * glm::dvec3(halfSize)),
		// 	position + rotation * (glm::dvec3{0,+1,0} * glm::dvec3(halfSize)),
		// 	position + rotation * (glm::dvec3{0,-1,0} * glm::dvec3(halfSize)),
		// 	position + rotation * (glm::dvec3{0,0,+1} * glm::dvec3(halfSize)),
		// 	position + rotation * (glm::dvec3{0,0,-1} * glm::dvec3(halfSize)),
		// };
		// for (const glm::dvec3& p : points) {
		// 	rays.emplace_back(p, -glm::normalize(p-position), glm::length(p-position));
		// }
		
	}

};
