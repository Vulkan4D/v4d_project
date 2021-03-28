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

struct PotentialCollision {
	Entity::Id a;
	Entity::Id b;
	glm::vec3 penetration;
	PotentialCollision(Entity::Id a, Entity::Id b, glm::vec3 penetration)
	 : a(a)
	 , b(b)
	 , penetration(penetration)
	{}
};

std::unordered_map<uint64_t, std::vector<BroadphaseCollider>> cachedBroadphaseColliders {};
std::vector<PotentialCollision> potentialCollisions {};

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
						const glm::vec3 d = colliders[i].position - colliders[j].position;
						const float contactDistanceSqrt = glm::dot(d,d) - radiusSum*radiusSum;
						if (contactDistanceSqrt < 0) {
							potentialCollisions.emplace_back(
								colliders[i].id, 
								colliders[j].id,
								glm::normalize(d) * glm::sqrt(-contactDistanceSqrt)
							);
						}
					}
				}
			}
		}
		
		{// Temporary basic collision response using broadphase bounding sphere
			for (const auto&[a,b,penetration] : potentialCollisions) {
				ServerSideEntity::Get(a)->rigidbody.Do([p = penetration](Rigidbody& rb){
					rb.linearVelocity = {0,0,0};
					rb.angularVelocity = {0,0,0};
					rb.position += p/2.0f;
				});
				ServerSideEntity::Get(b)->rigidbody.Do([p = penetration](Rigidbody& rb){
					rb.linearVelocity = {0,0,0};
					rb.angularVelocity = {0,0,0};
					rb.position -= p/2.0f;
				});
			}
		}
		
		// {// Narrow Phase Collision Detection
		// 	//...
		// }
		
		// {// Collision Response
		// 	//...
		// }
		
		{// Apply Gravity
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
