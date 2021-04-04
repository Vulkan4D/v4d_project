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
	uint64_t referenceFrame;
	uint32_t a; // index of cachedBroadphaseColliders[referenceFrame][]
	uint32_t b;
	glm::vec3 normal; // normalize(B-A) in world space
	float penetration;
	glm::vec3 contactA; // position rotated in world space, relative to center of mass of the entity
	glm::vec3 contactB; // position rotated in world space, relative to center of mass of the entity
	Collision(uint64_t referenceFrame, uint32_t a, uint32_t b, glm::vec3 normal, float penetration, glm::vec3 contactA, glm::vec3 contactB)
	 : referenceFrame(referenceFrame)
	 , a(a), b(b)
	 , normal(normal)
	 , penetration(penetration)
	 , contactA(contactA), contactB(contactB)
	{}
};

struct TerrainCollision {
	uint64_t celestialIdInGalaxy;
	TerrainType terrainType;
	glm::vec3 normal; // upwards or terrain surface normal (world space)
	float penetration;
	glm::vec3 contactTerrain; // world space
	glm::vec3 contactB; // position rotated in world space, relative to center of mass of the entity
	TerrainCollision(uint64_t celestialIdInGalaxy, TerrainType terrainType, glm::vec3 normal, float penetration, glm::vec3 contactTerrain, glm::vec3 contactB)
	 : celestialIdInGalaxy(celestialIdInGalaxy)
	 , terrainType(terrainType)
	 , normal(normal)
	 , penetration(penetration)
	 , contactTerrain(contactTerrain), contactB(contactB)
	{}
};

std::unordered_map<uint64_t, std::vector<BroadphaseCollider>> cachedBroadphaseColliders {};
std::vector<Collision> potentialCollisions {};

#pragma region Shape-Ray intersections

struct Ray {
	glm::dvec3 origin;
	glm::dvec3 direction;
	double length;
};

uint randomSeed = 913450978;

bool SphereRayIntersect(const Collider& sphere, const Ray& ray, double& t1, double& t2) {
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
bool SphereRayIntersectExteriorSurface(const Collider& sphere, const Ray& ray, double& t, glm::dvec3& normal) {
	double t1, t2;
	if (SphereRayIntersect(sphere, ray, t1, t2)) {
		t = t1;
		normal = glm::normalize((ray.origin + ray.direction * t1) - sphere.position);
		return true;
	}
	return false;
}

bool TriangleRayIntersect(const Collider& triangle, const Ray& ray, double& u, double& v, double& t, glm::dvec3& normal) {
	const glm::dvec3 localRayOrigin = ray.origin - triangle.position;
	const glm::dvec3 localRayDirection = glm::inverse(triangle.rotation) * ray.direction;
	
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
	
	return true;
}

bool CylinderRayIntersect(const Collider& cylinder, const Ray& ray, double& t1, double& t2, glm::dvec3& normal) {
	//TODO
	return false;
}

bool RingRayIntersect(const Collider& ring, const Ray& ray, double& t1, double& t2, glm::dvec3& normal) {
	//TODO
	return false;
}

bool BoxRayIntersect(const Collider& box, const Ray& ray, double& t1, double& t2, glm::dvec3& normal) {
	//TODO
	return false;
}

#pragma endregion

#pragma region General colliders

// General intersection methods should set collision.normal to normalize(B-A) or the surface normal of A towards B, in world space, given dot(B-A, normal) > 0
// collision.penetration should be a positive number of the amount of penetration between the two colliders
// collision.contact[A/B] should be set rotated to world space positioned relative to center of collider (should be converted back to entity-relative in calling method)
// the calling method is responsible for giving us the world space position and rotation in Collider::position and Collider::rotation respectively for each individual colliders

bool SphereIntersect(Collision& collision, const Collider& a, const Collider& b) {
	const float radiusSum = a.sphere.radius + b.sphere.radius;
	const glm::vec3 d = b.position - a.position;
	if (glm::dot(d,d) < radiusSum*radiusSum) {
		collision.normal = glm::normalize(d);
		collision.penetration = radiusSum - glm::length(d);
		collision.contactA = +collision.normal * a.sphere.radius;
		collision.contactB = -collision.normal * b.sphere.radius;
		return true;
	}
	return false;
}

bool TriangleIntersect(Collision& collision, const Collider& a, const Collider& b) {
	//TODO
	return false;
}

bool CylinderIntersect(Collision& collision, const Collider& a, const Collider& b) {
	//TODO
	return false;
}

bool RingIntersect(Collision& collision, const Collider& a, const Collider& b) {
	//TODO
	return false;
}

bool BoxIntersect(Collision& collision, const Collider& a, const Collider& b) {
	//TODO
	return false;
}

bool TriangleSphereIntersect(Collision& collision, const Collider& aTriangle, const Collider& bSphere) {
	//TODO
	return false;
}

bool CylinderSphereIntersect(Collision& collision, const Collider& aCylinder, const Collider& bSphere) {
	//TODO
	return false;
}

bool RingSphereIntersect(Collision& collision, const Collider& aRing, const Collider& bSphere) {
	//TODO
	return false;
}

bool BoxSphereIntersect(Collision& collision, const Collider& aBox, const Collider& bSphere) {
	// const glm::vec3& boxHalfSize = aBox.box.halfSize;
	// const float& sphereRadius = bSphere.sphere.radius;
	
	// const glm::dmat3 boxInverseRotationMatrix = glm::inverse(aBox.rotation);
	// const glm::dvec3 relSpherePosition = boxInverseRotationMatrix * bSphere.position;
	// const glm::vec3 d = relSpherePosition - aBox.position;
	// if (glm::dot(d,d) < sphereRadius*sphereRadius) {
	// 	const glm::vec3 closestPointOnBox = glm::clamp(d, -boxHalfSize, +boxHalfSize);
	// 	collision.normal = glm::normalize(glm::mat3(aBox.rotation) * glm::normalize(d - closestPointOnBox));
	// 	collision.penetration = sphereRadius - glm::length(d);
	// 	collision.contactA = glm::normalize(glm::mat3(aBox.rotation) * glm::normalize(closestPointOnBox)) * length(closestPointOnBox);
	// 	collision.contactB = glm::normalize(glm::mat3(aBox.rotation) * -collision.normal) * sphereRadius;
	// 	return true;
	// }
	return false;
}

bool CylinderTriangleIntersect(Collision& collision, const Collider& aCylinder, const Collider& bTriangle) {
	//TODO
	return false;
}

bool RingTriangleIntersect(Collision& collision, const Collider& aRing, const Collider& bTriangle) {
	//TODO
	return false;
}

bool BoxTriangleIntersect(Collision& collision, const Collider& aBox, const Collider& bTriangle) {
	//TODO
	return false;
}

bool RingCylinderIntersect(Collision& collision, const Collider& aRing, const Collider& bCylinder) {
	//TODO
	return false;
}

bool BoxCylinderIntersect(Collision& collision, const Collider& aBox, const Collider& bCylinder) {
	//TODO
	return false;
}

bool BoxRingIntersect(Collision& collision, const Collider& aBox, const Collider& bRing) {
	//TODO
	return false;
}

#pragma endregion

#pragma region Terrain colliders

// Terrain intersection methods should set collision.normal to the terrain surface normal and collision.contactTerrain to the position on the planet, in world space
// collision.penetration should be a positive number of the amount of penetration between the collider and the terrain, typically the depth that it's penetrating in the ground
// collision.contactB should be set rotated to world space positioned relative to center of collider (should be converted back to entity-relative in calling method)
// the calling method is responsible for giving us the world space position and rotation in Collider::position and Collider::rotation respectively for each individual colliders

bool TerrainSphereIntersect(TerrainCollision& collision, const Collider& sphere, Planet* planet) {
	const glm::dvec3 normalizedPos = glm::normalize(sphere.position);
	const double terrainHeight = planet->GetTerrainHeightAtPos(normalizedPos);
	const double altitudeAboveTerrain = glm::length(sphere.position) - sphere.sphere.radius - terrainHeight;
	
	if (altitudeAboveTerrain < 0) {
		glm::dvec3 tangentXPos = glm::normalize(glm::cross(normalizedPos, {0,1,0}));
		glm::dvec3 tangentYPos = glm::normalize(glm::cross(normalizedPos, tangentXPos));
		tangentXPos = glm::normalize(sphere.position + tangentXPos*double(sphere.sphere.radius*0.5));
		tangentYPos = glm::normalize(sphere.position + tangentYPos*double(sphere.sphere.radius*0.5));
		const glm::dvec3 posOnTerrain = normalizedPos * terrainHeight;
		const glm::dvec3 tangentX = glm::normalize(tangentXPos*(planet->GetTerrainHeightAtPos(tangentXPos)) - posOnTerrain);
		const glm::dvec3 tangentY = glm::normalize(tangentYPos*(planet->GetTerrainHeightAtPos(tangentYPos)) - posOnTerrain);
		
		collision.normal = glm::normalize(glm::cross(tangentX, tangentY));
		collision.penetration = float(-altitudeAboveTerrain);
		collision.contactTerrain = sphere.position;
		collision.contactB = -collision.normal * float(sphere.sphere.radius);
		
		return true;
	}
	return false;
}

bool TerrainTriangleIntersect(TerrainCollision& collision, const Collider& triangle, Planet* planet) {
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
		collision.contactB = {0,0,0};
		goto FoundIntersection;
	}
	
	if (altitudeAboveTerrain1 < 0) {
		collision.penetration = float(-altitudeAboveTerrain1);
		collision.contactTerrain = p1;
		collision.contactB = p1 - p0;
		goto FoundIntersection;
	}
	
	if (altitudeAboveTerrain2 < 0) {
		collision.penetration = float(-altitudeAboveTerrain2);
		collision.contactTerrain = p2;
		collision.contactB = p2 - p0;
		goto FoundIntersection;
	}
	
	{// Add a Random point for good measure...
		const glm::dvec3 pRandom = triangle.position + triangle.rotation * glm::dvec3(RandomInTriangle(randomSeed, glm::vec2{0,triangle.triangle.vertex1_y}, triangle.triangle.vertex2), 0);
		const glm::dvec3 normalizedPosRandom = glm::normalize(pRandom);
		const double terrainHeightRandom = planet->GetTerrainHeightAtPos(normalizedPosRandom);
		const double altitudeAboveTerrainRandom = glm::length(pRandom) - terrainHeightRandom;
		
		if (altitudeAboveTerrainRandom < 0) {
			collision.penetration = float(-altitudeAboveTerrainRandom);
			collision.contactTerrain = pRandom;
			collision.contactB = pRandom - p0;
			goto FoundIntersection;
		}
	}
	
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

bool TerrainCylinderIntersect(TerrainCollision& collision, const Collider& cylinder, Planet* planet) {
	//TODO
	return false;
}

bool TerrainRingIntersect(TerrainCollision& collision, const Collider& cylinder, Planet* planet) {
	//TODO
	return false;
}

bool TerrainBoxIntersect(TerrainCollision& collision, const Collider& cylinder, Planet* planet) {
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
			
			// Projection method (separate the two bodies so that they don't penetrate anymore)
			const glm::dvec3 separation = glm::dvec3(collision.normal * collision.penetration) / (rbA->invMass + rbB->invMass);
			// Reposition rigidbody
			rbA->position -= separation * rbA->invMass;
			rbB->position += separation * rbB->invMass;
			
			// Impulse method (Adjust linear and angular velocities to simulate a bounce)
			const glm::dvec3 normal = collision.normal;
			const glm::dvec3 contactA = collision.contactA;
			const glm::dvec3 contactB = collision.contactB;
			const glm::dvec3 angularVelocityA = glm::cross(rbA->angularVelocity, contactA);
			const glm::dvec3 angularVelocityB = glm::cross(rbB->angularVelocity, contactB);
			const glm::dvec3 totalVelocityA = rbA->linearVelocity + angularVelocityA;
			const glm::dvec3 totalVelocityB = rbB->linearVelocity + angularVelocityB;
			const glm::dvec3 contactVelocity = totalVelocityB - totalVelocityA;
			const glm::dvec3 inertiaA = glm::cross(rbA->invInertiaMatrix * glm::cross(contactA, normal), contactA);
			const glm::dvec3 inertiaB = glm::cross(rbB->invInertiaMatrix * glm::cross(contactB, normal), contactB);
			contactSpeed = -glm::dot(contactVelocity, normal);
			const double J = contactSpeed * (rbA->restitution * rbB->restitution + 1.0) / (rbA->invMass + rbB->invMass + glm::dot(inertiaA + inertiaB, normal));
			const glm::dvec3 impulse = J * normal;
			
			// Apply impulses to rigidbody
			rbA->ApplyImpulse(-impulse, contactA);
			rbB->ApplyImpulse(+impulse, contactB);
			
			// Friction
			glm::dvec3 tangent = contactVelocity - normal * glm::dot(contactVelocity, normal);
			const double tangentLength = glm::length(tangent);
			if (tangentLength > 1e-6) {
				tangent /= tangentLength;
				const double frictionalMass = (rbA->invMass + rbB->invMass) + glm::dot(tangent, glm::cross(rbA->invInertiaMatrix * glm::cross(contactA, tangent), contactA) + glm::cross(rbB->invInertiaMatrix * glm::cross(contactB, tangent), contactB));
				if (frictionalMass > 0) {
					const double frictionCoeficient = rbA->friction * rbB->friction;
					const glm::dvec3 frictionImpulse = tangent * double(-glm::dot(contactVelocity, tangent) * frictionCoeficient / frictionalMass);
					// Apply impulses from friction
					rbA->ApplyImpulse(-frictionImpulse, contactA);
					rbB->ApplyImpulse(+frictionImpulse, contactB);
				}
			}
			
		}
	}
	
	// Trigger collision events on entities
	if (auto mod = V4D_Mod::GetModule(entityA->moduleID.String()); mod && mod->OnCollisionHit) {
		mod->OnCollisionHit(entityA->GetID(), entityA->type, glm::inverse(glm::mat3_cast(entityA->orientation)) * collision.contactA, glm::max(0.0, contactSpeed));
	}
	if (auto mod = V4D_Mod::GetModule(entityB->moduleID.String()); mod && mod->OnCollisionHit) {
		mod->OnCollisionHit(entityB->GetID(), entityB->type, glm::inverse(glm::mat3_cast(entityB->orientation)) * collision.contactB, glm::max(0.0, contactSpeed));
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
		const glm::dvec3 contactB = collision.contactB;
		const glm::dvec3 angularVelocityB = glm::cross(rb->angularVelocity, contactB);
		const glm::dvec3 totalVelocityB = rb->linearVelocity + angularVelocityB;
		const glm::dvec3 contactVelocity = totalVelocityB;
		const glm::dvec3 inertiaB = glm::cross(rb->invInertiaMatrix * glm::cross(contactB, normal), contactB);
		contactSpeed = -glm::dot(contactVelocity, normal);
		const double J = contactSpeed * (collision.terrainType.restitution * rb->restitution + 1.0) / (rb->invMass + glm::dot(inertiaB, normal));
		const glm::dvec3 impulse = J * normal;
		
		// Apply impulses to rigidbody
		rb->ApplyImpulse(+impulse, contactB);
		
		// Friction
		glm::dvec3 tangent = contactVelocity - normal * glm::dot(contactVelocity, normal);
		const double tangentLength = glm::length(tangent);
		if (tangentLength > 1e-6) {
			tangent /= tangentLength;
			const double frictionalMass = (rb->invMass) + glm::dot(tangent, glm::cross(rb->invInertiaMatrix * glm::cross(contactB, tangent), contactB));
			if (frictionalMass > 0) {
				const double frictionCoeficient = collision.terrainType.friction * rb->friction;
				const glm::dvec3 frictionImpulse = tangent * double(-glm::dot(contactVelocity, tangent) * frictionCoeficient / frictionalMass);
				// Apply impulses from friction
				rb->ApplyImpulse(+frictionImpulse, contactB);
			}
		}
		
	}
	
	// Trigger collision events on entities
	if (auto mod = V4D_Mod::GetModule(entity->moduleID.String()); mod && mod->OnCollisionHit) {
		mod->OnCollisionHit(entity->GetID(), entity->type, glm::inverse(glm::mat3_cast(entity->orientation)) * collision.contactB, glm::max(0.0, contactSpeed));
	}
}

#pragma endregion

#pragma region Narrow phase collision solvers

void SolveCollisionBetweenTwoRigidbodies(Collision& collision) {
	if (auto entityA = ServerSideEntity::Get(cachedBroadphaseColliders[collision.referenceFrame][collision.a].id); entityA) {
		if (auto entityB = ServerSideEntity::Get(cachedBroadphaseColliders[collision.referenceFrame][collision.b].id); entityB) {
			if (entityA->colliders.size() > 0 || entityB->colliders.size() > 0) {
				int aIndex = 0, bIndex = 0;
				Collider a {float(cachedBroadphaseColliders[collision.referenceFrame][collision.a].radius)};
				Collider b {float(cachedBroadphaseColliders[collision.referenceFrame][collision.b].radius)};
				
				do {
					if (entityA->colliders.size() > 0) {
						a = entityA->colliders[aIndex];
						// Transform colliders position and rotation to World space
						a.position += entityA->position;
						a.rotation = glm::mat3_cast(entityA->orientation) * a.rotation;
					}
					do {
						if (entityB->colliders.size() > 0) {
							b = entityB->colliders[bIndex];
							// Transform colliders position and rotation to World space
							b.position += entityB->position;
							b.rotation = glm::mat3_cast(entityB->orientation) * b.rotation;
						}
						
						switch (a.type | b.type) {
							default:
							case Collider::Type::SPHERE:
								if (SphereIntersect(collision, a, b)) goto CollisionResponse;
							break;
							case Collider::Type::TRIANGLE:
								if (TriangleIntersect(collision, a, b)) goto CollisionResponse;
							break;
							case Collider::Type::CYLINDER:
								if (CylinderIntersect(collision, a, b)) goto CollisionResponse;
							break;
							case Collider::Type::RING:
								if (RingIntersect(collision, a, b)) goto CollisionResponse;
							break;
							case Collider::Type::BOX:
								if (BoxIntersect(collision, a, b)) goto CollisionResponse;
							break;
							case Collider::Type::TRIANGLE | Collider::Type::SPHERE:
								if (a.type == Collider::Type::TRIANGLE) {
									if (TriangleSphereIntersect(collision, a, b)) goto CollisionResponse;
								} else {
									if (TriangleSphereIntersect(collision, b, a)) {
										collision.contactB = std::exchange(collision.contactA, collision.contactB);
										collision.normal *= -1;
										goto CollisionResponse;
									}
								}
							break;
							case Collider::Type::CYLINDER | Collider::Type::SPHERE:
								if (a.type == Collider::Type::CYLINDER) {
									if (CylinderSphereIntersect(collision, a, b)) goto CollisionResponse;
								} else {
									if (CylinderSphereIntersect(collision, b, a)) {
										collision.contactB = std::exchange(collision.contactA, collision.contactB);
										collision.normal *= -1;
										goto CollisionResponse;
									}
								}
							break;
							case Collider::Type::RING | Collider::Type::SPHERE:
								if (a.type == Collider::Type::RING) {
									if (RingSphereIntersect(collision, a, b)) goto CollisionResponse;
								} else {
									if (RingSphereIntersect(collision, b, a)) {
										collision.contactB = std::exchange(collision.contactA, collision.contactB);
										collision.normal *= -1;
										goto CollisionResponse;
									}
								}
							break;
							case Collider::Type::BOX | Collider::Type::SPHERE:
								if (a.type == Collider::Type::BOX) {
									if (BoxSphereIntersect(collision, a, b)) goto CollisionResponse;
								} else {
									if (BoxSphereIntersect(collision, b, a)) {
										collision.contactB = std::exchange(collision.contactA, collision.contactB);
										collision.normal *= -1;
										goto CollisionResponse;
									}
								}
							break;
							case Collider::Type::CYLINDER | Collider::Type::TRIANGLE:
								if (a.type == Collider::Type::CYLINDER) {
									if (CylinderTriangleIntersect(collision, a, b)) goto CollisionResponse;
								} else {
									if (CylinderTriangleIntersect(collision, b, a)) {
										collision.contactB = std::exchange(collision.contactA, collision.contactB);
										collision.normal *= -1;
										goto CollisionResponse;
									}
								}
							break;
							case Collider::Type::RING | Collider::Type::TRIANGLE:
								if (a.type == Collider::Type::RING) {
									if (RingTriangleIntersect(collision, a, b)) goto CollisionResponse;
								} else {
									if (RingTriangleIntersect(collision, b, a)) {
										collision.contactB = std::exchange(collision.contactA, collision.contactB);
										collision.normal *= -1;
										goto CollisionResponse;
									}
								}
							break;
							case Collider::Type::BOX | Collider::Type::TRIANGLE:
								if (a.type == Collider::Type::BOX) {
									if (BoxTriangleIntersect(collision, a, b)) goto CollisionResponse;
								} else {
									if (BoxTriangleIntersect(collision, b, a)) {
										collision.contactB = std::exchange(collision.contactA, collision.contactB);
										collision.normal *= -1;
										goto CollisionResponse;
									}
								}
							break;
							case Collider::Type::RING | Collider::Type::CYLINDER:
								if (a.type == Collider::Type::RING) {
									if (RingCylinderIntersect(collision, a, b)) goto CollisionResponse;
								} else {
									if (RingCylinderIntersect(collision, b, a)) {
										collision.contactB = std::exchange(collision.contactA, collision.contactB);
										collision.normal *= -1;
										goto CollisionResponse;
									}
								}
							break;
							case Collider::Type::BOX | Collider::Type::CYLINDER:
								if (a.type == Collider::Type::BOX) {
									if (BoxCylinderIntersect(collision, a, b)) goto CollisionResponse;
								} else {
									if (BoxCylinderIntersect(collision, b, a)) {
										collision.contactB = std::exchange(collision.contactA, collision.contactB);
										collision.normal *= -1;
										goto CollisionResponse;
									}
								}
							break;
							case Collider::Type::BOX | Collider::Type::RING:
								if (a.type == Collider::Type::BOX) {
									if (BoxRingIntersect(collision, a, b)) goto CollisionResponse;
								} else {
									if (BoxRingIntersect(collision, b, a)) {
										collision.contactB = std::exchange(collision.contactA, collision.contactB);
										collision.normal *= -1;
										goto CollisionResponse;
									}
								}
							break;
						}
						
						continue;
						
						CollisionResponse:
							// Transform colliders contact point from collider-relative to entity-relative
							if (entityA->colliders.size() > 0) collision.contactA += entityA->colliders[aIndex].position;
							if (entityB->colliders.size() > 0) collision.contactB += entityB->colliders[bIndex].position;
							// Respond to the colision
							RespondToCollision(entityA, entityB, collision);
							return;
							
					} while(++aIndex < entityA->colliders.size());
				} while(++bIndex < entityB->colliders.size());
			} else {
				RespondToCollision(entityA, entityB, collision);
			}
		}
	}
}

void SolveCollisionWithTerrain(ServerSideEntity::Ptr& entity, Planet* planet) {
	glm::dvec3 normalizedPos = glm::normalize(entity->position);
	TerrainCollision collision { planet->GetID(), planet->GetTerrainTypeAtPos(normalizedPos), normalizedPos, 0.001/* 1 mm */, entity->position, -normalizedPos };
	
	if (entity->colliders.size() > 0) {
		for (Collider collider : entity->colliders) {
			// Transform colliders position and rotation to World space
			collider.position += entity->position;
			collider.rotation = glm::mat3_cast(entity->orientation) * collider.rotation;
			
			switch (collider.type) {
				default:
				case Collider::Type::SPHERE:
					if (TerrainSphereIntersect(collision, collider, planet))
						goto CollisionResponse;
				break;
				case Collider::Type::TRIANGLE:
					if (TerrainTriangleIntersect(collision, collider, planet))
						goto CollisionResponse;
				break;
				case Collider::Type::CYLINDER:
					if (TerrainCylinderIntersect(collision, collider, planet))
						goto CollisionResponse;
				break;
				case Collider::Type::RING:
					if (TerrainRingIntersect(collision, collider, planet))
						goto CollisionResponse;
				break;
				case Collider::Type::BOX:
					if (TerrainBoxIntersect(collision, collider, planet))
						goto CollisionResponse;
				break;
			}
			
			continue;
			
			CollisionResponse:
				// Transform colliders contact point from collider-relative to entity-relative
				collision.contactB += collider.position - entity->position;
				// Respond to the colision
				RespondToCollisionWithTerrain(entity, collision);
				return;
		}
	} else {
		RespondToCollisionWithTerrain(entity, collision);
	}
}

#pragma endregion

V4D_MODULE_CLASS(V4D_Mod) {
	
	V4D_MODULE_FUNC(void, ServerPhysicsUpdate, double deltaTime) {
		
		{// Prepare data
			ServerSideEntity::rigidbodyComponents.ForEach_Entity([](ServerSideEntity::Ptr& entity, Rigidbody& rigidbody){
				rigidbody.position = entity->position;
				rigidbody.orientation = entity->orientation;
				if (rigidbody.boundingRadius > 0) {
					cachedBroadphaseColliders[entity->referenceFrame].emplace_back(
						rigidbody.position,
						rigidbody.boundingRadius,
						entity->GetID()
					);
					
					
					// // Debug Colliders
					// for (auto& collider : entity->colliders) {
					// 	switch (collider.type) {
					// 		case Collider::Type::TRIANGLE:{
					// 			const glm::dvec3& p0 = collider.position + entity->position;
					// 			const glm::dvec3 p1 = p0 + (glm::mat3_cast(entity->orientation) * collider.rotation) * glm::dvec3{0,collider.triangle.vertex1_y,0};
					// 			const glm::dvec3 p2 = p0 + (glm::mat3_cast(entity->orientation) * collider.rotation) * glm::dvec3{collider.triangle.vertex2,0};
					// 			mainRenderModule->DrawOverlayLineViewSpace(scene->camera.viewMatrix * glm::dvec4(p0, 1), scene->camera.viewMatrix * glm::dvec4(p1, 1), glm::vec4{1}, 2.0);
					// 			mainRenderModule->DrawOverlayLineViewSpace(scene->camera.viewMatrix * glm::dvec4(p0, 1), scene->camera.viewMatrix * glm::dvec4(p2, 1), glm::vec4{1}, 2.0);
					// 			mainRenderModule->DrawOverlayLineViewSpace(scene->camera.viewMatrix * glm::dvec4(p1, 1), scene->camera.viewMatrix * glm::dvec4(p2, 1), glm::vec4{1}, 2.0);
					// 		}break;
					// 	}
					// }
					
					
				}
			});
		}
		
		{// Broad Phase Collision Detection
			for (const auto&[referenceFrame, colliders] : cachedBroadphaseColliders) {
				for (size_t i = 0; i < colliders.size() - 1; ++i) {
					for (size_t j = i+1; j < colliders.size(); ++j) {
						const float radiusSum = colliders[i].radius + colliders[j].radius;
						const glm::vec3 d = colliders[j].position - colliders[i].position;
						if (glm::dot(d,d) < radiusSum*radiusSum) {
							const glm::dvec3 normal = glm::normalize(d);
							potentialCollisions.emplace_back(
								referenceFrame,
								i, j,
								normal,
								radiusSum - glm::length(d),
								+normal*colliders[i].radius,
								-normal*colliders[j].radius
							);
						}
					}
				}
			}
		}
		
		{// Narrow-phase Collision detection and response
			std::for_each(potentialCollisions.begin(), potentialCollisions.end(), SolveCollisionBetweenTwoRigidbodies);
		}
		
		{// Apply Gravity and Collision with terrain
			for (const auto&[referenceFrame, colliders] : cachedBroadphaseColliders) {
				const GalacticPosition galacticPosition(referenceFrame);
				if (galacticPosition.IsCelestial()) {
					auto celestial = GalaxyGenerator::GetCelestial(galacticPosition);
					if (celestial->GetType() == CelestialType::Planet) {
						Planet* planet = (Planet*)celestial.get();
						if (PlanetTerrain::generatorFunction) {
							for (auto collider : colliders) {
								const double distanceFromPlanetCenter = glm::length(collider.position);
								if (distanceFromPlanetCenter > 0) {
									if (auto entity = ServerSideEntity::Get(collider.id); entity) {
										glm::dvec3 normalizedPos = glm::normalize(collider.position);
										{// Gravity
											if (auto rb = entity->rigidbody.Lock(); rb) {
												rb->linearAcceleration += -normalizedPos * planet->GetGravityAcceleration(distanceFromPlanetCenter);
											}
										}
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
		
		{// Apply aerodynamics
			//...
		}
		
		{// Apply Constraints
			//...
		}
		
		{// Integrate motion
			ServerSideEntity::rigidbodyComponents.ForEach([deltaTime](Entity::Id id, Rigidbody& rigidbody){
				// Linear integration
				rigidbody.linearAcceleration += rigidbody.invMass * rigidbody.force;
				rigidbody.linearVelocity += rigidbody.linearAcceleration * deltaTime;
				rigidbody.position += rigidbody.linearVelocity * deltaTime;
				// Angular integration
				rigidbody.angularAcceleration += rigidbody.invInertiaMatrix * rigidbody.torque;
				rigidbody.angularVelocity += rigidbody.angularAcceleration * deltaTime;
				rigidbody.orientation = glm::normalize(rigidbody.orientation + glm::dquat(0.0, rigidbody.angularVelocity * deltaTime / 2.0) * rigidbody.orientation);
			});
		}
		
		{// Update entity
			ServerSideEntity::rigidbodyComponents.ForEach_Entity([](ServerSideEntity::Ptr& entity, Rigidbody& rigidbody){
				entity->position = rigidbody.position;
				entity->orientation = rigidbody.orientation;
				// Clear forces
				rigidbody.force = {0,0,0};
				rigidbody.linearAcceleration = {0,0,0};
				rigidbody.torque = {0,0,0};
				rigidbody.angularAcceleration = {0,0,0};
			});
		}
		
		{// Clear cached data
			potentialCollisions.clear();
			for (auto&[referenceFrame, colliders] : cachedBroadphaseColliders) {
				colliders.clear();
			}
		}
		
	}

};
