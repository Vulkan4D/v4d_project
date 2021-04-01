#include <v4d.h>
#include <V4D_Mod.h>
#include "v4d/game/Entity.h"
#include "utilities/io/Logger.h"

#include <vector>
#include <unordered_map>

// #include "GalaxyGenerator.h"
// #include "Celestial.h"
// #include "StarSystem.h"
// #include "celestials/Planet.h"
// #include "TerrainGeneratorLib.h"

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
	glm::vec3 normal; // normalize(B-A)
	float penetration;
	glm::vec3 contactA;
	glm::vec3 contactB;
	Collision(uint64_t referenceFrame, uint32_t a, uint32_t b, glm::vec3 normal, float penetration, glm::vec3 contactA, glm::vec3 contactB)
	 : referenceFrame(referenceFrame)
	 , a(a), b(b)
	 , normal(normal)
	 , penetration(penetration)
	 , contactA(contactA), contactB(contactB)
	{}
};

std::unordered_map<uint64_t, std::vector<BroadphaseCollider>> cachedBroadphaseColliders {};
std::vector<Collision> potentialCollisions {};

bool SphereSphereIntersect(Collision& collision, const Collider& a, const Collider& b) {
	return true;
}

bool BoxBoxIntersect(Collision& collision, const Collider& a, const Collider& b) {
	return true;
}

bool BoxSphereIntersect(Collision& collision, const Collider& box, const Collider& sphere) {
	return true;
}

void SolveCollision(Collision& collision) {
	if (auto entityA = ServerSideEntity::Get(cachedBroadphaseColliders[collision.referenceFrame][collision.a].id); entityA) {
		if (auto entityB = ServerSideEntity::Get(cachedBroadphaseColliders[collision.referenceFrame][collision.b].id); entityB) {
			
			////////////////////////////////////////////////////
			// Narrow phase collision detection
			
			if (entityA->colliders.size() > 0 || entityB->colliders.size() > 0) {
				collision.penetration = 0;
				int aIndex = 0, bIndex = 0;
				Collider a {glm::dmat4{1}, {cachedBroadphaseColliders[collision.referenceFrame][collision.a].radius,0,0}, Collider::Type::SPHERE};
				Collider b {glm::dmat4{1}, {cachedBroadphaseColliders[collision.referenceFrame][collision.b].radius,0,0}, Collider::Type::SPHERE};
				do {
					if (entityA->colliders.size() > 0) {
						a = entityA->colliders[aIndex];
						// a.offset = ....
					}
					do {
						if (entityB->colliders.size() > 0) {
							b = entityB->colliders[bIndex];
							// b.offset = ....
						}
						
						switch (a.type | b.type) {
							default:
							case Collider::Type::SPHERE:
								if (SphereSphereIntersect(collision, a, b)) goto CollisionResponse;
							break;
							case Collider::Type::BOX:
								if (BoxBoxIntersect(collision, a, b)) goto CollisionResponse;
							break;
							case Collider::Type::BOX | Collider::Type::SPHERE:
								if (BoxSphereIntersect(collision, a.type==Collider::Type::BOX? a : b, a.type==Collider::Type::SPHERE? b : a)) goto CollisionResponse;
							break;
						}
						
					} while(++aIndex < entityA->colliders.size());
				} while(++bIndex < entityB->colliders.size());
			}
			
			// No collision
			if (collision.penetration == 0) return;
			
			////////////////////////////////////////////////////
			// Collision Response
			
			CollisionResponse:{
			
				double contactSpeed {0};
				
				// This should be the only place where we lock two rigidbodies simultaneously. Doing it on any other thread could result in a deadlock.
				if (auto rbA = entityA->rigidbody.Lock(); rbA) {
					if (auto rbB = entityB->rigidbody.Lock(); rbB) {
						
						// Projection method (separate the two bodies so that they don't penetrate anymore)
						const glm::dvec3 separation = glm::dvec3(collision.normal * collision.penetration) / (rbA->invMass + rbB->invMass);
						// Reposition rigidbody
						rbA->position += separation * rbA->invMass;
						rbB->position -= separation * rbB->invMass;
						
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
					mod->OnCollisionHit(entityA->GetID(), entityA->type, collision.contactA, glm::max(0.0, contactSpeed));
				}
				if (auto mod = V4D_Mod::GetModule(entityB->moduleID.String()); mod && mod->OnCollisionHit) {
					mod->OnCollisionHit(entityB->GetID(), entityB->type, collision.contactB, glm::max(0.0, contactSpeed));
				}
			}
			
		}
	}
}

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
				}
			});
		}
		
		{// Broad Phase Collision Detection
			for (const auto&[referenceFrame, colliders] : cachedBroadphaseColliders) {
				for (size_t i = 0; i < colliders.size() - 1; ++i) {
					for (size_t j = i+1; j < colliders.size(); ++j) {
						const float radiusSum = colliders[i].radius + colliders[j].radius;
						const glm::vec3 d = colliders[j].position - colliders[i].position;
						const float contactDistanceSqr = glm::dot(d,d) - radiusSum*radiusSum;
						if (contactDistanceSqr < 0) {
							const glm::dvec3 normal = glm::normalize(d);
							potentialCollisions.emplace_back(
								referenceFrame,
								i, j,
								normal,
								(glm::length(d) - radiusSum),
								+normal*colliders[i].radius,
								-normal*colliders[j].radius
							);
						}
					}
				}
			}
		}
		
		{// Narrow-phase Collision detection and response
			std::for_each(potentialCollisions.begin(), potentialCollisions.end(), SolveCollision);
		}
		
		{// Apply Gravity
			//...   rigidbody.linearAcceleration += m/s2
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
