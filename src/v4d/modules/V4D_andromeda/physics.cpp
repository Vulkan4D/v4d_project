#include <v4d.h>
#include <V4D_Mod.h>
#include "v4d/game/Entity.h"
#include "utilities/io/Logger.h"

#include <vector>
#include <unordered_map>

#include "GalaxyGenerator.h"
#include "Celestial.h"
#include "StarSystem.h"
#include "celestials/Planet.h"
#include "TerrainGeneratorLib.h"
#include "noise_functions.hpp"

extern V4D_Mod* mainRenderModule;
extern v4d::scene::Scene* scene;

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

struct Collision {
	glm::vec3 normal; // normalize(B-A) in world space
	float penetration; // amount of penetration as positive number
	glm::dvec3 contactA; // hit position on entity A in world space
	glm::dvec3 contactB; // hit position on entity B in world space
	Collision() {}
};

struct TerrainCollision {
	uint64_t celestialIdInGalaxy;
	TerrainType terrainType;
	
	glm::vec3 normal; // upwards or terrain surface normal (world space)
	float penetration;
	glm::dvec3 contactTerrain; // world space
	glm::dvec3 contactB; // position in world space
	
	TerrainCollision(uint64_t celestialIdInGalaxy, TerrainType terrainType)
	 : celestialIdInGalaxy(celestialIdInGalaxy)
	 , terrainType(terrainType)
	{}
};

std::unordered_map<uint64_t, std::vector<BroadphaseCollider>> cachedBroadphaseColliders {};
uint randomSeed;
double avgDeltaTime = 1.0 / 200;

#pragma region Ray-Shape intersections/collisions

struct Ray {
	glm::dvec3 origin;
	glm::dvec3 direction;
	double length;
	Ray(glm::dvec3 origin, glm::dvec3 direction, double length)
	: origin(origin)
	, direction(direction)
	, length(length)
	{}
};

bool RaySphereIntersect(const Ray& ray, const Collider& sphere, double& t1, double& t2) {
	const glm::dvec3 oc = ray.origin - sphere.position;
	const double a = glm::dot(ray.direction, ray.direction);
	const double b = glm::dot(oc, ray.direction);
	const double c = glm::dot(oc, oc) - sphere.sphere.radius*sphere.sphere.radius;
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
bool RaySphereCollision(const Ray& ray, const Collider& collider, Collision& collision) {
	double t1, t2;
	if (RaySphereIntersect(ray, collider, t1, t2) && t2 < ray.length) {
		collision.penetration = t2;
		collision.contactA = ray.origin;
		collision.contactB = ray.origin + ray.direction * t2;
		collision.normal = -ray.direction;
		return true;
	}
	return false;
}

bool RayTriangleIntersect(const Ray& ray, const Collider& triangle, double& u, double& v, double& t, glm::dvec3& normal) {
	const glm::dvec3 localRayOrigin = glm::inverse(triangle.rotation) * (ray.origin - triangle.position);
	const glm::dvec3 localRayDirection = glm::normalize(glm::inverse(triangle.rotation) * ray.direction);
	
	// const glm::dvec3 v0 {0,0,0};
	const glm::dvec3 v1 {0,triangle.triangle.vertex1_y,0};
	const glm::dvec3 v2 {triangle.triangle.vertex2,0};
	
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
	normal = glm::normalize(triangle.rotation * glm::dvec3(0,0,glm::sign(det)));
	
	// Make sure normal direction is opposite of triangle center direction
	if (glm::dot(normal, triangle.rotation * triangle.triangle.center) > 0) normal *= -1;
	
	return true;
}
bool RayTriangleCollision(const Ray& ray, const Collider& collider, Collision& collision) {
	double u,v,t;
	glm::dvec3 normal;
	if (RayTriangleIntersect(ray, collider, u, v, t, normal)) {
		collision.penetration = t;
		collision.contactA = ray.origin;
		collision.contactB = ray.origin + ray.direction * t;
		// collision.normal = normal;
		collision.normal = -ray.direction;
		return true;
	}
	return false;
}

bool RayBoxIntersect(const Ray& ray, const Collider& box, double& t1, double& t2) {
	const glm::dvec3 localRayOrigin = glm::inverse(box.rotation) * (ray.origin - box.position);
	const glm::dvec3 localRayDirection = glm::normalize(glm::inverse(box.rotation) * ray.direction);
	
	const glm::dvec3 aabb_min = -box.box.halfSize;
	const glm::dvec3 aabb_max = +box.box.halfSize;
	const glm::dvec3 invDir = 1.0 / localRayDirection;
	const glm::dvec3 tbot   = invDir * (aabb_min - localRayOrigin);
	const glm::dvec3 ttop   = invDir * (aabb_max - localRayOrigin);
	const glm::dvec3 tmin   = glm::min(ttop, tbot);
	const glm::dvec3 tmax   = glm::max(ttop, tbot);
	t1 = glm::max(tmin.x, glm::max(tmin.y, tmin.z));
	t2 = glm::min(tmax.x, glm::min(tmax.y, tmax.z));
	
	return t1 < ray.length && t2 > 0 && t2 > t1;
}
bool RayBoxCollision(const Ray& ray, const Collider& collider, Collision& collision) {
	double t1, t2;
	if (RayBoxIntersect(ray, collider, t1, t2) && t1 < 0 && t2 < ray.length) {
		collision.penetration = t2;
		collision.contactA = ray.origin;
		collision.contactB = ray.origin + ray.direction * t2;
		collision.normal = -ray.direction;
		return true;
	}
	return false;
}

bool RayCylinderIntersect(const Ray& ray, const Collider& cylinder, double& t1, double& t2, glm::dvec3& normal) {
	//TODO
	return false;
}
bool RayCylinderCollision(const Ray& ray, const Collider& collider, Collision& collision) {
	//TODO
	return false;
}

bool RayRingIntersect(const Ray& ray, const Collider& ring, double& t1, double& t2, glm::dvec3& normal) {
	//TODO
	return false;
}
bool RayRingCollision(const Ray& ray, const Collider& collider, Collision& collision) {
	//TODO
	return false;
}

#pragma endregion

#pragma region Collision response

void RespondToCollision(ServerSideEntity::Ptr& entityA, ServerSideEntity::Ptr& entityB, Collision& collision) {
	double contactSpeed {0};
	
	// This should be the only place where we lock two rigidbodies simultaneously. Doing it on any other thread could result in a deadlock.
	if (auto rbA = entityA->rigidbody.Lock(); rbA) {
		if (auto rbB = entityB->rigidbody.Lock(); rbB) {
			
			if (rbA->atRest && rbB->atRest) return;
			
			// Projection method (separate the two bodies so that they don't penetrate anymore)
			const glm::dvec3 separation = glm::dvec3(collision.normal * collision.penetration) / (rbA->invMass + rbB->invMass);
			// Reposition rigidbody
			rbA->position -= separation * rbA->invMass;
			rbB->position += separation * rbB->invMass;
			
			// Impulse method (Adjust linear and angular velocities to simulate a bounce)
			const glm::dvec3 normal = collision.normal;
			const glm::dvec3 contactA = collision.contactA - rbA->position;
			const glm::dvec3 contactB = collision.contactB - rbB->position;
			const glm::dvec3 angularVelocityA = glm::cross(rbA->angularVelocity, contactA);
			const glm::dvec3 angularVelocityB = glm::cross(rbB->angularVelocity, contactB);
			const glm::dvec3 totalVelocityA = rbA->linearVelocity + angularVelocityA;
			const glm::dvec3 totalVelocityB = rbB->linearVelocity + angularVelocityB;
			const glm::dvec3 contactVelocity = totalVelocityB - totalVelocityA;
			const glm::dvec3 inertiaA = glm::cross(rbA->invInertiaTensorWorld * glm::cross(contactA, normal), contactA);
			const glm::dvec3 inertiaB = glm::cross(rbB->invInertiaTensorWorld * glm::cross(contactB, normal), contactB);
			contactSpeed = -glm::dot(contactVelocity, normal);
			const double J = contactSpeed * (rbA->restitution * rbB->restitution + 1.0) / (rbA->invMass + rbB->invMass + glm::dot(inertiaA + inertiaB, normal));
			const glm::dvec3 impulse = J * normal;
			
			// Apply impulses to rigidbody
			rbA->ApplyImpulse(-impulse, contactA);
			rbB->ApplyImpulse(+impulse, contactB);
			
			// Dynamic Friction
			const double frictionCoeficient = rbA->friction * rbB->friction;
			glm::dvec3 tangent = contactVelocity - normal * glm::dot(contactVelocity, normal);
			const double tangentLength = glm::length(tangent);
			if (tangentLength > 1e-6) {
				tangent /= tangentLength;
				const double frictionalMass = (rbA->invMass + rbB->invMass) + glm::dot(tangent, glm::cross(rbA->invInertiaTensorWorld * glm::cross(contactA, tangent), contactA) + glm::cross(rbB->invInertiaTensorWorld * glm::cross(contactB, tangent), contactB));
				if (frictionalMass > 0) {
					const glm::dvec3 frictionImpulse = tangent * double(-glm::dot(contactVelocity, tangent) * frictionCoeficient / frictionalMass);
					// Apply impulses from friction
					rbA->ApplyImpulse(-frictionImpulse, contactA);
					rbB->ApplyImpulse(+frictionImpulse, contactB);
				}
			}
			
			// Static Friction
			if (glm::length(rbA->linearVelocity) > COLLISION_REST_SPEED_THRESHOLD || glm::length(rbA->angularVelocity) > COLLISION_REST_ANGULAR_SPEED_THRESHOLD) {
				rbA->linearVelocity -= glm::normalize(rbA->linearVelocity) * COLLISION_STATIC_FRICTION_SPEED_REDUCTION * avgDeltaTime * frictionCoeficient;
				rbA->angularVelocity -= glm::normalize(rbA->angularVelocity) * COLLISION_STATIC_FRICTION_ANGULAR_SPEED_REDUCTION * avgDeltaTime * frictionCoeficient;
			} else {
				rbA->linearVelocity = {0,0,0};
				rbA->angularVelocity = {0,0,0};
				rbA->atRest = true;
			}
			if (glm::length(rbB->linearVelocity) > COLLISION_REST_SPEED_THRESHOLD || glm::length(rbB->angularVelocity) > COLLISION_REST_ANGULAR_SPEED_THRESHOLD) {
				rbB->linearVelocity -= glm::normalize(rbB->linearVelocity) * COLLISION_STATIC_FRICTION_SPEED_REDUCTION * avgDeltaTime * frictionCoeficient;
				rbB->angularVelocity -= glm::normalize(rbB->angularVelocity) * COLLISION_STATIC_FRICTION_ANGULAR_SPEED_REDUCTION * avgDeltaTime * frictionCoeficient;
			} else {
				rbB->linearVelocity = {0,0,0};
				rbB->angularVelocity = {0,0,0};
				rbB->atRest = true;
			}
			
			
		}
	}
	
	// Trigger collision events on entities
	if (auto mod = V4D_Mod::GetModule(entityA->moduleID.String()); mod && mod->OnCollisionHit) {
		mod->OnCollisionHit(entityA->GetID(), entityA->type, glm::inverse(glm::mat3_cast(entityA->orientation)) * (collision.contactA - entityA->position), glm::max(0.0, contactSpeed));
	}
	if (auto mod = V4D_Mod::GetModule(entityB->moduleID.String()); mod && mod->OnCollisionHit) {
		mod->OnCollisionHit(entityB->GetID(), entityB->type, glm::inverse(glm::mat3_cast(entityB->orientation)) * (collision.contactB - entityB->position), glm::max(0.0, contactSpeed));
	}
}

void RespondToCollisionWithTerrain(ServerSideEntity::Ptr& entity, TerrainCollision& collision) {
	double contactSpeed {0};
	
	// This should be the only place where we lock two rigidbodies simultaneously. Doing it on any other thread could result in a deadlock.
	if (auto rb = entity->rigidbody.Lock(); rb) {

		// Projection method (separate the two bodies so that they don't penetrate anymore)
		const glm::dvec3 separation = glm::dvec3(collision.normal * collision.penetration) / rb->invMass;
		// Reposition rigidbody
		rb->position += separation * rb->invMass;
		
		// Impulse method (Adjust linear and angular velocities to simulate a bounce)
		const glm::dvec3 normal = collision.normal;
		const glm::dvec3 contactB = collision.contactB - rb->position;
		const glm::dvec3 angularVelocityB = glm::cross(rb->angularVelocity, contactB);
		const glm::dvec3 totalVelocityB = rb->linearVelocity + angularVelocityB;
		const glm::dvec3 contactVelocity = totalVelocityB;
		const glm::dvec3 inertiaB = glm::cross(rb->invInertiaTensorWorld * glm::cross(contactB, normal), contactB);
		contactSpeed = -glm::dot(contactVelocity, normal);
		const double J = contactSpeed * (collision.terrainType.restitution * rb->restitution + 1.0) / (rb->invMass + glm::dot(inertiaB, normal));
		const glm::dvec3 impulse = J * normal;
		
		// Apply impulses to rigidbody
		rb->ApplyImpulse(+impulse, contactB);
		
		// Dynamic Friction
		glm::dvec3 tangent = contactVelocity - normal * glm::dot(contactVelocity, normal);
		const double tangentLength = glm::length(tangent);
		if (tangentLength > 1e-6) {
			tangent /= tangentLength;
			const double frictionalMass = (rb->invMass) + glm::dot(tangent, glm::cross(rb->invInertiaTensorWorld * glm::cross(contactB, tangent), contactB));
			if (frictionalMass > 0) {
				const double frictionCoeficient = collision.terrainType.friction * rb->friction;
				const glm::dvec3 frictionImpulse = tangent * double(-glm::dot(contactVelocity, tangent) * frictionCoeficient / frictionalMass);
				// Apply impulses from friction
				rb->ApplyImpulse(+frictionImpulse, contactB);
			}
		}
		
		// Static Friction
		if (glm::length(rb->linearVelocity) > TERRAIN_COLLISION_REST_SPEED_THRESHOLD || glm::length(rb->angularVelocity) > TERRAIN_COLLISION_REST_ANGULAR_SPEED_THRESHOLD) {
			rb->linearVelocity -= glm::normalize(rb->linearVelocity) * TERRAIN_COLLISION_STATIC_FRICTION_SPEED_REDUCTION * avgDeltaTime * collision.terrainType.friction;
			rb->angularVelocity -= glm::normalize(rb->angularVelocity) * TERRAIN_COLLISION_STATIC_FRICTION_ANGULAR_SPEED_REDUCTION * avgDeltaTime * collision.terrainType.friction;
		} else {
			rb->linearVelocity = {0,0,0};
			rb->angularVelocity = {0,0,0};
			rb->atRest = true;
		}
		
	}
	
	// Trigger collision events on entities
	if (auto mod = V4D_Mod::GetModule(entity->moduleID.String()); mod && mod->OnCollisionHit) {
		mod->OnCollisionHit(entity->GetID(), entity->type, glm::inverse(glm::mat3_cast(entity->orientation)) * (collision.contactB - entity->position), glm::max(0.0, contactSpeed));
	}
}

#pragma endregion

#pragma region Terrain collision solver

// Terrain intersection methods should set collision.normal to the terrain surface normal and collision.contactTerrain to the position on the planet, in world space
// collision.penetration should be a positive number of the amount of penetration between the collider and the terrain, typically the depth that it's penetrating in the ground
// collision.contactB should be set to the contact point on the collider, but in world space
// the calling method is responsible for giving us the world space position and rotation in Collider::position and Collider::rotation respectively for each individual colliders

bool TerrainSphereCollision(const Planet* const planet, const Collider& sphere, TerrainCollision& collision) {
	const glm::dvec3 normalizedPos = glm::normalize(sphere.position);
	const double terrainHeight = planet->GetTerrainHeightAtPos(normalizedPos);
	const double altitudeAboveTerrain = glm::length(sphere.position) - sphere.sphere.radius - terrainHeight;
	
	if (altitudeAboveTerrain < 0) {
		glm::dvec3 tangentXPos = glm::normalize(glm::cross(normalizedPos, glm::dvec3{0,1,0}));
		glm::dvec3 tangentYPos = glm::normalize(glm::cross(normalizedPos, tangentXPos));
		tangentXPos = glm::normalize(sphere.position + tangentXPos*double(sphere.sphere.radius*0.5));
		tangentYPos = glm::normalize(sphere.position + tangentYPos*double(sphere.sphere.radius*0.5));
		const glm::dvec3 posOnTerrain = normalizedPos * terrainHeight;
		const glm::dvec3 tangentX = glm::normalize(tangentXPos*(planet->GetTerrainHeightAtPos(tangentXPos)) - posOnTerrain);
		const glm::dvec3 tangentY = glm::normalize(tangentYPos*(planet->GetTerrainHeightAtPos(tangentYPos)) - posOnTerrain);
		
		collision.normal = glm::normalize(glm::cross(tangentX, tangentY));
		collision.penetration = float(-altitudeAboveTerrain);
		collision.contactTerrain = sphere.position;
		collision.contactB = sphere.position - glm::dvec3(collision.normal * sphere.sphere.radius);
		
		return true;
	}
	return false;
}

bool TerrainTriangleCollision(const Planet* const planet, const Collider& triangle, TerrainCollision& collision) {
	const glm::dvec3& p0 = triangle.position;
	const glm::dvec3 p1 = triangle.position + triangle.rotation * glm::dvec3{0,triangle.triangle.vertex1_y,0};
	const glm::dvec3 p2 = triangle.position + triangle.rotation * glm::dvec3{triangle.triangle.vertex2,0};
	
	const glm::dvec3 normalizedPos0 = glm::normalize(p0);
	const glm::dvec3 normalizedPos1 = glm::normalize(p1);
	const glm::dvec3 normalizedPos2 = glm::normalize(p2);
	const double terrainHeight0 = planet->GetTerrainHeightAtPos(normalizedPos0);
	const double terrainHeight1 = planet->GetTerrainHeightAtPos(normalizedPos1);
	const double terrainHeight2 = planet->GetTerrainHeightAtPos(normalizedPos2);
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
	// 	const glm::dvec3 pRandom = triangle.position + triangle.rotation * glm::dvec3(RandomInTriangle(randomSeed, glm::vec2{0,triangle.triangle.vertex1_y}, triangle.triangle.vertex2), 0);
	// 	const glm::dvec3 normalizedPosRandom = glm::normalize(pRandom);
	// 	const double terrainHeightRandom = planet->GetTerrainHeightAtPos(normalizedPosRandom);
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

bool TerrainBoxCollision(const Planet* const planet, const Collider& box, TerrainCollision& collision, ServerSideEntity::Ptr& entity) {
	constexpr int MIN_TEST_VERTICES = 4; // test at least the first 4 lowest vertices of the box
	constexpr int MAX_TEST_VERTICES = 6; // test up to 6 lowest vertices of the box
	
	std::vector<glm::dvec3> points {
		box.position + box.rotation * (glm::dvec3{+1,+1,+1} * glm::dvec3(box.box.halfSize)),
		box.position + box.rotation * (glm::dvec3{+1,+1,-1} * glm::dvec3(box.box.halfSize)),
		box.position + box.rotation * (glm::dvec3{+1,-1,-1} * glm::dvec3(box.box.halfSize)),
		box.position + box.rotation * (glm::dvec3{-1,-1,-1} * glm::dvec3(box.box.halfSize)),
		box.position + box.rotation * (glm::dvec3{-1,-1,+1} * glm::dvec3(box.box.halfSize)),
		box.position + box.rotation * (glm::dvec3{-1,+1,+1} * glm::dvec3(box.box.halfSize)),
		box.position + box.rotation * (glm::dvec3{+1,-1,+1} * glm::dvec3(box.box.halfSize)),
		box.position + box.rotation * (glm::dvec3{-1,+1,-1} * glm::dvec3(box.box.halfSize)),
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
		terrainHeight[i] = planet->GetTerrainHeightAtPos(normalizedPos[i]);
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

bool TerrainCylinderCollision(const Planet* const planet, const Collider& cylinder, TerrainCollision& collision) {
	//TODO
	return false;
}

bool TerrainRingCollision(const Planet* const planet, const Collider& ring, TerrainCollision& collision) {
	//TODO
	return false;
}

void SolveCollisionWithTerrain(ServerSideEntity::Ptr& entity, const Planet* const planet) {
	const glm::dvec3 normalizedPos = glm::normalize(entity->position);
	TerrainCollision collision { planet->GetID(), planet->GetTerrainTypeAtPos(normalizedPos) };
	
	for (Collider collider : entity->colliders) {
		// Transform colliders position and rotation to World space
		collider.position = entity->position + glm::mat3_cast(entity->orientation) * collider.position;
		collider.rotation = glm::mat3_cast(entity->orientation) * collider.rotation;
		
		switch (collider.type) {
			default:
			case Collider::Type::SPHERE:
				if (TerrainSphereCollision(planet, collider, collision)) {
					RespondToCollisionWithTerrain(entity, collision);
				}
			break;
			case Collider::Type::TRIANGLE:
				if (TerrainTriangleCollision(planet, collider, collision)) {
					RespondToCollisionWithTerrain(entity, collision);
				}
			break;
			case Collider::Type::BOX:
				if (TerrainBoxCollision(planet, collider, collision, entity)) {
					RespondToCollisionWithTerrain(entity, collision);
				}
			break;
			case Collider::Type::CYLINDER:
				if (TerrainCylinderCollision(planet, collider, collision)) {
					RespondToCollisionWithTerrain(entity, collision);
				}
			break;
			case Collider::Type::RING:
				if (TerrainRingCollision(planet, collider, collision)) {
					RespondToCollisionWithTerrain(entity, collision);
				}
			break;
		}
	}
}

#pragma endregion

#pragma region Generate collision rays

std::vector<Ray> cachedCollisionRays {};

void GenerateSphereCollisionRays(const Collider& collider) {
	const int RAYS_PER_SPHERE = 1;
	for (int i = 0; i < RAYS_PER_SPHERE; ++i) {
		glm::dvec3 randomPoint = glm::normalize(RandomInUnitSphere(randomSeed));
		cachedCollisionRays.emplace_back(collider.position + randomPoint * double(collider.sphere.radius), -randomPoint, collider.sphere.radius);
	}
	// Add an additional ray at the very bottom
	glm::dvec3 bottomPoint = -glm::normalize(collider.position);
	cachedCollisionRays.emplace_back(collider.position + bottomPoint * double(collider.sphere.radius), -bottomPoint, collider.sphere.radius);
}

void GenerateTriangleCollisionRays(const Collider& collider) {
	// const glm::dvec3& p0 = collider.position;
	// const glm::dvec3 p1 = collider.position + collider.rotation * glm::dvec3{0,collider.triangle.vertex1_y,0};
	// const glm::dvec3 p2 = collider.position + collider.rotation * glm::dvec3{collider.triangle.vertex2,0};
	
	constexpr int RAYS_PER_TRIANGLE = 1;
	const glm::dvec3 pCenter = collider.position + collider.rotation * glm::dvec3(collider.triangle.center);
	for (int i = 0; i < RAYS_PER_TRIANGLE; ++i) {
		const glm::dvec3 pRandom = collider.position + collider.rotation * glm::dvec3(RandomInTriangle(randomSeed, glm::vec2{0,collider.triangle.vertex1_y}, collider.triangle.vertex2), 0);
		const glm::dvec3 vRay = pCenter - pRandom;
		cachedCollisionRays.emplace_back(pRandom, glm::normalize(vRay), glm::length(vRay));
	}
}

void GenerateBoxCollisionRays(const Collider& collider) {
	
	// Launch rays stochastically
	constexpr int RAYS_PER_FACE = 1;
	constexpr bool AXIS_ALIGNED_RAYS = true;
	auto func = [&collider](const glm::dvec3& point, const glm::dvec3& dir){
		const glm::dvec3 p = collider.position + collider.rotation * (point * glm::dvec3(collider.box.halfSize));
		if constexpr(AXIS_ALIGNED_RAYS) {
			cachedCollisionRays.emplace_back(p, collider.rotation * -dir, glm::length(dir * glm::dvec3(collider.box.halfSize)));
		} else {
			cachedCollisionRays.emplace_back(p, -glm::normalize(p-collider.position), glm::length(p-collider.position));
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
	// 	// collider.position + collider.rotation * (glm::dvec3{+1,+1,+1} * glm::dvec3(collider.box.halfSize)),
	// 	// collider.position + collider.rotation * (glm::dvec3{+1,+1,-1} * glm::dvec3(collider.box.halfSize)),
	// 	// collider.position + collider.rotation * (glm::dvec3{+1,-1,-1} * glm::dvec3(collider.box.halfSize)),
	// 	// collider.position + collider.rotation * (glm::dvec3{-1,-1,-1} * glm::dvec3(collider.box.halfSize)),
	// 	// collider.position + collider.rotation * (glm::dvec3{-1,-1,+1} * glm::dvec3(collider.box.halfSize)),
	// 	// collider.position + collider.rotation * (glm::dvec3{-1,+1,+1} * glm::dvec3(collider.box.halfSize)),
	// 	// collider.position + collider.rotation * (glm::dvec3{+1,-1,+1} * glm::dvec3(collider.box.halfSize)),
	// 	// collider.position + collider.rotation * (glm::dvec3{-1,+1,-1} * glm::dvec3(collider.box.halfSize)),
	// 	// Face Centers
	// 	collider.position + collider.rotation * (glm::dvec3{+1,0,0} * glm::dvec3(collider.box.halfSize)),
	// 	collider.position + collider.rotation * (glm::dvec3{-1,0,0} * glm::dvec3(collider.box.halfSize)),
	// 	collider.position + collider.rotation * (glm::dvec3{0,+1,0} * glm::dvec3(collider.box.halfSize)),
	// 	collider.position + collider.rotation * (glm::dvec3{0,-1,0} * glm::dvec3(collider.box.halfSize)),
	// 	collider.position + collider.rotation * (glm::dvec3{0,0,+1} * glm::dvec3(collider.box.halfSize)),
	// 	collider.position + collider.rotation * (glm::dvec3{0,0,-1} * glm::dvec3(collider.box.halfSize)),
	// };
	// for (const glm::dvec3& p : points) {
	// 	cachedCollisionRays.emplace_back(p, -glm::normalize(p-collider.position), glm::length(p-collider.position));
	// }
	
}

void GenerateCylinderCollisionRays(const Collider& collider) {
	//TODO
}

void GenerateRingCollisionRays(const Collider& collider) {
	//TODO
}

#pragma endregion

V4D_MODULE_CLASS(V4D_Mod) {
	
	V4D_MODULE_FUNC(void, ServerPhysicsUpdate, double deltaTime) {
		avgDeltaTime = glm::mix(avgDeltaTime, deltaTime, 0.1);

		//TODO keep cache valid until an entity was added, removed or its active status changed, add colliderCacheIndex field in entity so that we can update the cached position in cachedBroadphaseColliders at the end of the physics frame
		
		{// Prepare data
			ServerSideEntity::rigidbodyComponents.ForEach_Entity([](ServerSideEntity::Ptr& entity, Rigidbody& rigidbody) {
				if (entity->IsActive()) {
					if (!rigidbody.IsInitialized()) {
						if (rigidbody.IsKinematic()) {
							rigidbody.invMass = 0;
							rigidbody.invInertiaTensorWorld = glm::dmat3{0};
						} else {
							rigidbody.position = entity->position;
							rigidbody.orientation = entity->orientation;
							rigidbody.ComputeInvInertiaTensorWorld();
						}
						rigidbody.SetInitialized();
					}
					
					if (rigidbody.boundingRadius > 0) {
						cachedBroadphaseColliders[entity->referenceFrame].emplace_back(
							rigidbody.position,
							rigidbody.boundingRadius,
							entity->GetID()
						);
					}
					
				}
			});
		}
		
		{// Collision Detection
			for (const auto&[referenceFrame, colliders] : cachedBroadphaseColliders) {
				for (size_t cacheIndexA = 0; cacheIndexA < colliders.size(); ++cacheIndexA) {
					
					ServerSideEntity::Ptr entityA = nullptr;
					
					for (size_t cacheIndexB = 0; cacheIndexB < colliders.size(); ++cacheIndexB) if (cacheIndexA != cacheIndexB) {
						const float radiusSum = colliders[cacheIndexA].radius + colliders[cacheIndexB].radius;
						const glm::vec3 d = colliders[cacheIndexB].position - colliders[cacheIndexA].position;
						if (glm::dot(d,d) < radiusSum*radiusSum) {
							// const glm::dvec3 normal = glm::normalize(d);
							
							if (!entityA) {
								entityA = ServerSideEntity::Get(colliders[cacheIndexA].id);
								if (!entityA) goto NextEntityA;
								
								// Loop through entityA's colliders to generate a list of collision rays
								cachedCollisionRays.clear();
								for (auto colliderA : entityA->colliders) {
									// Transform collider info world space
									colliderA.position = entityA->position + glm::mat3_cast(entityA->orientation) * colliderA.position;
									colliderA.rotation = glm::mat3_cast(entityA->orientation) * colliderA.rotation;
									// Generate collision rays depending on type of collider
									switch (colliderA.type) {
										default:
										case Collider::Type::SPHERE:
											GenerateSphereCollisionRays(colliderA);
										break;
										case Collider::Type::TRIANGLE:
											GenerateTriangleCollisionRays(colliderA);
										break;
										case Collider::Type::BOX:
											GenerateBoxCollisionRays(colliderA);
										break;
										case Collider::Type::CYLINDER:
											GenerateCylinderCollisionRays(colliderA);
										break;
										case Collider::Type::RING:
											GenerateRingCollisionRays(colliderA);
										break;
									}
								}
								
								// Debug collision rays
								if (scene->camera.debugOptions & DEBUG_OPTION_PHYSICS) {
									for (auto& r : cachedCollisionRays) {
										mainRenderModule->DrawOverlayLineViewSpace(scene->camera.viewMatrix * glm::dvec4(r.origin, 1), scene->camera.viewMatrix * glm::dvec4(r.origin + r.direction * r.length, 1), glm::vec4{1}, 2.0);
									}
								}
								
							}
							
							Collision collision;
							if (auto entityB = ServerSideEntity::Get(colliders[cacheIndexB].id); entityB) {
								if (entityA->colliders.size() > 0 || entityB->colliders.size() > 0) {
									for (auto colliderB : entityB->colliders) {
										// Transform collider to World space
										colliderB.position = entityB->position + glm::mat3_cast(entityB->orientation) * colliderB.position;
										colliderB.rotation = glm::mat3_cast(entityB->orientation) * colliderB.rotation;
										
										switch (colliderB.type) {
											default:
											case Collider::Type::SPHERE:
												for (const auto& ray : cachedCollisionRays) {
													if (RaySphereCollision(ray, colliderB, collision)) {
														RespondToCollision(entityA, entityB, collision);
													}
												}
											break;
											case Collider::Type::TRIANGLE:
												for (const auto& ray : cachedCollisionRays) {
													if (RayTriangleCollision(ray, colliderB, collision)) {
														RespondToCollision(entityA, entityB, collision);
													}
												}
											break;
											case Collider::Type::BOX:
												for (const auto& ray : cachedCollisionRays) {
													if (RayBoxCollision(ray, colliderB, collision)) {
														RespondToCollision(entityA, entityB, collision);
													}
												}
											break;
											case Collider::Type::CYLINDER:
												for (const auto& ray : cachedCollisionRays) {
													if (RayCylinderCollision(ray, colliderB, collision)) {
														RespondToCollision(entityA, entityB, collision);
													}
												}
											break;
											case Collider::Type::RING:
												for (const auto& ray : cachedCollisionRays) {
													if (RayRingCollision(ray, colliderB, collision)) {
														RespondToCollision(entityA, entityB, collision);
													}
												}
											break;
										}
									}
								}
							}
							
						}
					}
					NextEntityA: (void)0;
				}
			}
		}
		
		if (PlanetTerrain::generatorFunction) {// Apply Gravity and Collision with terrain
			for (const auto&[referenceFrame, colliders] : cachedBroadphaseColliders) {
				const GalacticPosition galacticPosition(referenceFrame);
				if (galacticPosition.IsCelestial()) {
					const auto celestial = GalaxyGenerator::GetCelestial(galacticPosition);
					if (celestial->GetType() == CelestialType::Planet) {
						const Planet* planet = (Planet*)celestial.get();
						for (auto collider : colliders) {
							const double distanceFromPlanetCenter = glm::length(collider.position);
							if (distanceFromPlanetCenter > 0) {
								if (auto entity = ServerSideEntity::Get(collider.id); entity) {
									const glm::dvec3 normalizedPos = glm::normalize(collider.position);
									if (auto rb = entity->rigidbody.Lock(); rb) {
										if (rb->atRest) continue;
										// Gravity
										if (entity->moduleID != v4d::modular::ModuleID(THIS_MODULE) || entity->type != 0) {
											rb->linearAcceleration += -normalizedPos * planet->GetGravityAcceleration(distanceFromPlanetCenter);
										}
										// Air resistance & aerodynamics
										if (distanceFromPlanetCenter < planet->GetAtmosphereRadius()) {
											//TODO
										}
									}
									// Collisions with terrain
									if (distanceFromPlanetCenter < planet->GetTerrainRadius() + planet->GetTerrainHeightVariation()) {
										constexpr double radiusMarginFactor = 2.0; // added margins to catch collisions on very steep slopes
										if (distanceFromPlanetCenter - collider.radius*radiusMarginFactor - planet->GetTerrainHeightAtPos(normalizedPos) < 0) {
											SolveCollisionWithTerrain(entity, planet);
										}
									}
								}
							}
						}
					}
				}
			}
		}
		
		{// Apply Constraints
			//...
		}
		
		{// Integrate motion
			ServerSideEntity::rigidbodyComponents.ForEach([deltaTime](Entity::Id id, Rigidbody& rigidbody){
				if (rigidbody.IsInitialized() && !rigidbody.IsKinematic() && !rigidbody.atRest) {
					// Linear integration
					rigidbody.linearAcceleration += rigidbody.invMass * rigidbody.force;
					rigidbody.linearVelocity += rigidbody.linearAcceleration * deltaTime;
					rigidbody.position += rigidbody.linearVelocity * deltaTime;
					// Angular integration
					if (!rigidbody.IsOrientationLocked()) {
						rigidbody.angularAcceleration += rigidbody.invInertiaTensorWorld * rigidbody.torque;
						rigidbody.angularVelocity += rigidbody.angularAcceleration * deltaTime;
						rigidbody.orientation = glm::normalize(rigidbody.orientation + glm::dquat(0.0, rigidbody.angularVelocity * deltaTime / 2.0) * rigidbody.orientation);
						rigidbody.ComputeInvInertiaTensorWorld();
					}
				}
			});
		}
		
		{// Update entity
			ServerSideEntity::rigidbodyComponents.ForEach_Entity([](ServerSideEntity::Ptr& entity, Rigidbody& rigidbody){
				if (rigidbody.IsInitialized() && !rigidbody.IsKinematic()) {
					entity->position = rigidbody.position;
					if (!rigidbody.IsOrientationLocked()) entity->orientation = rigidbody.orientation;
					// Clear forces
					rigidbody.force = {0,0,0};
					rigidbody.linearAcceleration = {0,0,0};
					rigidbody.torque = {0,0,0};
					rigidbody.angularAcceleration = {0,0,0};
				}
			});
		}
		
		{// Clear cached data
			for (auto&[referenceFrame, colliders] : cachedBroadphaseColliders) {
				colliders.clear();
			}
		}
		
	}

};
