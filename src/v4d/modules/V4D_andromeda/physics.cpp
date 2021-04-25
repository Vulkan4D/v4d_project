#include <v4d.h>
#include <V4D_Mod.h>
#include "utilities/io/Logger.h"

#include "v4d/game/Entity.h"
#include "v4d/game/physics.hh"
#include "v4d/game/Collider.hpp"
#include "v4d/game/ServerSideEntity.hpp"

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

std::unordered_map<uint64_t, std::vector<BroadphaseCollider>> cachedBroadphaseColliders {};
std::vector<Ray> cachedCollisionRays {};
uint randomSeed;
double avgDeltaTime = 1.0 / 200;

#pragma region Collision response

void RespondToCollision(ServerSideEntity::Ptr& entityA, ServerSideEntity::Ptr& entityB, CollisionInfo& collision) {
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

void RespondToCollisionWithTerrain(ServerSideEntity::Ptr& entity, TerrainCollisionInfo& collision) {
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

void SolveCollisionWithTerrain(ServerSideEntity::Ptr& entity, const Planet* const planet) {
	const glm::dvec3 normalizedPos = glm::normalize(entity->position);
	TerrainCollisionInfo collision { planet->GetID(), planet->GetTerrainTypeAtPos(normalizedPos) };
	auto terrainHeightMap = [&planet](const glm::dvec3& normalizedPos) -> double {
		return planet->GetTerrainHeightAtPos(normalizedPos);
	};
	for (const auto&[_, collider] : entity->colliders) {
		if (collider->TerrainCollision(terrainHeightMap, entity.get(), collision)) {
			RespondToCollisionWithTerrain(entity, collision);
		}
	}
}

#pragma endregion

V4D_MODULE_CLASS(V4D_Mod) {
	
	V4D_MODULE_FUNC(void, ServerPhysicsUpdate, double deltaTime) {
		avgDeltaTime = glm::mix(avgDeltaTime, deltaTime, 0.1);
		
		if (!ServerSideEntity::colliderCacheValid) {// Prepare data
			ServerSideEntity::colliderCacheValid = true;
			for (auto&[referenceFrame, colliders] : cachedBroadphaseColliders) {
				colliders.clear();
			}
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
						entity->colliderCacheIndex = cachedBroadphaseColliders[entity->referenceFrame].size();
						cachedBroadphaseColliders[entity->referenceFrame].emplace_back(
							rigidbody.position,
							rigidbody.boundingRadius,
							entity->GetID()
						);
					} else {
						entity->colliderCacheIndex = -1;
					}
				} else {
					entity->colliderCacheIndex = -1;
				}
			});
			// LOG("Generated collider cache")
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
								for (const auto&[id, colliderA] : entityA->colliders) {
									colliderA->GenerateCollisionRays(entityA.get(), id, cachedCollisionRays, randomSeed);
								}
								
								// Debug collision rays
								if (scene->camera.debugOptions & DEBUG_OPTION_PHYSICS) {
									for (auto& r : cachedCollisionRays) {
										mainRenderModule->DrawOverlayLineViewSpace(scene->camera.viewMatrix * glm::dvec4(r.origin, 1), scene->camera.viewMatrix * glm::dvec4(r.origin + r.direction * r.length, 1), glm::vec4{1}, 2.0);
									}
								}
								
							}
							
							CollisionInfo collision;
							if (auto entityB = ServerSideEntity::Get(colliders[cacheIndexB].id); entityB) {
								if (entityA->colliders.size() > 0 || entityB->colliders.size() > 0) {
									for (const auto&[_, colliderB] : entityB->colliders) {
										for (const auto& ray : cachedCollisionRays) {
											if (colliderB->RayCollision(ray, entityB.get(), collision)) {
												RespondToCollision(entityA, entityB, collision);
											}
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
					if (celestial && celestial->GetType() == CelestialType::Planet) {
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
		
		{// Integrate motion
			ServerSideEntity::rigidbodyComponents.ForEach([](Entity::Id id, Rigidbody& rigidbody){
				if (rigidbody.IsInitialized() && !rigidbody.IsKinematic() && !rigidbody.atRest) {
					// Linear integration
					rigidbody.linearAcceleration += rigidbody.invMass * rigidbody.force;
					rigidbody.linearVelocity += rigidbody.linearAcceleration * avgDeltaTime;
					rigidbody.position += rigidbody.linearVelocity * avgDeltaTime;
					// Angular integration
					if (!rigidbody.IsOrientationLocked()) {
						rigidbody.angularAcceleration += rigidbody.invInertiaTensorWorld * rigidbody.torque;
						rigidbody.angularVelocity += rigidbody.angularAcceleration * avgDeltaTime;
						rigidbody.orientation = glm::normalize(rigidbody.orientation + glm::dquat(0.0, rigidbody.angularVelocity * avgDeltaTime / 2.0) * rigidbody.orientation);
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
					if (entity->colliderCacheIndex != -1) {
						cachedBroadphaseColliders[entity->referenceFrame][entity->colliderCacheIndex].position = rigidbody.position;
					}
				}
			});
		}
		
	}

};
