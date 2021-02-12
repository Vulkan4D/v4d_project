#define _V4D_MODULE
#include <v4d.h>
#include "btBulletDynamicsCommon.h"
// #include "BulletCollision/CollisionShapes/btHeightfieldTerrainShape.h"
#include "../V4D_raytracing/camera_options.hh"

using namespace v4d::scene;
using namespace v4d::graphics;
using namespace v4d::graphics::vulkan;

v4d::graphics::Renderer* r = nullptr;
v4d::scene::Scene* scene = nullptr;

// #ifdef _DEBUG
	class DebugDrawer : public btIDebugDraw {
		int mode = DBG_DrawWireframe | DBG_DrawFeaturesText | DBG_DrawText | DBG_FastWireframe;
		
		void drawLine(const btVector3& from, const btVector3& to, const btVector3& color) override {
			glm::vec4 clipSpace1 = scene->camera.projectionMatrix * scene->camera.viewMatrix * glm::dvec4(from.x(), from.y(), from.z(), 1);
			glm::vec4 clipSpace2 = scene->camera.projectionMatrix * scene->camera.viewMatrix * glm::dvec4(to.x(), to.y(), to.z(), 1);
			clipSpace1 /= clipSpace1.w;
			clipSpace2 /= clipSpace2.w;
			if (clipSpace1.z < 0 || clipSpace2.z < 0) return;
			V4D_Mod::ForEachSortedModule([&](auto mod){
				if (mod->DrawOverlayLine) {
					mod->DrawOverlayLine(clipSpace1.x, clipSpace1.y, clipSpace2.x, clipSpace2.y, {color.x(), color.y(), color.z(), 1}, 2);
					mod = nullptr;
				}
			});
		}
		void drawContactPoint(const btVector3& PointOnB, const btVector3& normalOnB, btScalar distance, int lifeTime, const btVector3& color) override {
			
		}
		void reportErrorWarning(const char* warningString) override {
			
		}
		void draw3dText(const btVector3& location, const char* textString) override {
			glm::vec4 screenSpace = scene->camera.projectionMatrix * scene->camera.viewMatrix * glm::dvec4(location.x(), location.y(), location.z(), 1);
			screenSpace /= screenSpace.w;
			if (screenSpace.z < 0) return;
			V4D_Mod::ForEachSortedModule([&](auto mod){
				if (mod->DrawOverlayText) {
					mod->DrawOverlayText(textString, screenSpace.x, screenSpace.y, {1,1,1,1}, 20);
					mod = nullptr;
				}
			});
		}
		void setDebugMode(int debugMode) override {
			mode = debugMode;
		}
		int getDebugMode() const override {
			return mode;
		}
	} debugDrawer;
// #endif

btCollisionConfiguration* globalCollisionConfiguration = nullptr;
btDispatcher* globalPhysicsDispatcher = nullptr;
btBroadphaseInterface* globalOverlappingPairCache = nullptr;
btConstraintSolver* globalConstraintSolver = nullptr;
btDynamicsWorld* globalDynamicsWorld = nullptr;
btAlignedObjectArray<btCollisionShape*> globalCollisionShapes {};
btAlignedObjectArray<btTriangleMesh*> globalTriangleMeshes {};

glm::dvec3 BulletToGlm(const btVector3& v) { return glm::dvec3(v.getX(), v.getY(), v.getZ()); }
btVector3 GlmToBullet(const glm::dvec3& v) { return btVector3(v.x, v.y, v.z); }
glm::dquat BulletToGlm(const btQuaternion& q) { return glm::dquat(q.getW(), q.getX(), q.getY(), q.getZ()); }
btQuaternion GlmToBullet(const glm::dquat& q) { return btQuaternion(q.x, q.y, q.z, q.w); }
btMatrix3x3 GlmToBullet(const glm::dmat3& m) { return btMatrix3x3(m[0][0], m[1][0], m[2][0], m[0][1], m[1][1], m[2][1], m[0][2], m[1][2], m[2][2]); }
btTransform GlmToBullet(const glm::dmat4& m) {
	glm::dmat3 m3(m);
	return btTransform(GlmToBullet(m3), GlmToBullet(glm::dvec3(m[3][0], m[3][1], m[3][2])));
}
glm::dmat4 BulletToGlm(const btTransform& t) {
	glm::dmat4 m{};
	const btMatrix3x3& basis = t.getBasis();
	// rotation
	for (int r = 0; r < 3; r++) {
		for (int c = 0; c < 3; c++) {
			m[c][r] = basis[r][c];
		}
	}
	// traslation
	btVector3 origin = t.getOrigin();
	m[3][0] = origin.getX();
	m[3][1] = origin.getY();
	m[3][2] = origin.getZ();
	// unit scale
	m[0][3] = 0.0f;
	m[1][3] = 0.0f;
	m[2][3] = 0.0f;
	m[3][3] = 1.0f;
	return m;
}

struct PhysicsObject;
std::unordered_map<uint32_t, PhysicsObject*> physicsObjects {};
PhysicsObject* GetPhysicsObject(uint32_t index) {
	try {
		return physicsObjects.at(index);
	} catch(...) {
		return nullptr;
	}
}

void UpdateConstraintJointPhysics(btGeneric6DofSpring2Constraint* constraint, v4d::scene::PhysicsInfo* physics) {
	if (physics->jointIsDirty) {
		physics->jointIsDirty = false;
		
		constraint->setLinearLowerLimit(btVector3(physics->jointTranslationLimitsX.min, physics->jointTranslationLimitsY.min, physics->jointTranslationLimitsZ.min));
		constraint->setLinearUpperLimit(btVector3(physics->jointTranslationLimitsX.max, physics->jointTranslationLimitsY.max, physics->jointTranslationLimitsZ.max));
		constraint->setAngularLowerLimit(btVector3(physics->jointRotationLimitsX.min, physics->jointRotationLimitsY.min, physics->jointRotationLimitsZ.min));
		constraint->setAngularUpperLimit(btVector3(physics->jointRotationLimitsX.max, physics->jointRotationLimitsY.max, physics->jointRotationLimitsZ.max));
		
		if (physics->jointMotor && physics->jointTranslationLimitsX.min < physics->jointTranslationLimitsX.max) {
			constraint->setServo(0, true);
			constraint->setServoTarget(0, physics->jointTranslationTarget.x);
			constraint->setMaxMotorForce(0, physics->jointTranslationMaxForce.x);
			constraint->setTargetVelocity(0, physics->jointTranslationVelocity.x);
		} else {
			constraint->setServo(0, false);
			constraint->setTargetVelocity(0, 0);
		}
		
		if (physics->jointMotor && physics->jointTranslationLimitsY.min < physics->jointTranslationLimitsY.max) {
			constraint->setServo(1, true);
			constraint->setServoTarget(1, physics->jointTranslationTarget.y);
			constraint->setMaxMotorForce(1, physics->jointTranslationMaxForce.y);
			constraint->setTargetVelocity(1, physics->jointTranslationVelocity.y);
		} else {
			constraint->setServo(1, false);
			constraint->setTargetVelocity(1, 0);
		}
		
		if (physics->jointMotor && physics->jointTranslationLimitsZ.min < physics->jointTranslationLimitsZ.max) {
			constraint->setServo(2, true);
			constraint->setServoTarget(2, physics->jointTranslationTarget.z);
			constraint->setMaxMotorForce(2, physics->jointTranslationMaxForce.z);
			constraint->setTargetVelocity(2, physics->jointTranslationVelocity.z);
		} else {
			constraint->setServo(2, false);
			constraint->setTargetVelocity(2, 0);
		}
		
		if (physics->jointMotor && physics->jointRotationLimitsX.min < physics->jointRotationLimitsX.max) {
			constraint->setServo(3, true);
			constraint->setServoTarget(3, physics->jointRotationTarget.x);
			constraint->setMaxMotorForce(3, physics->jointRotationMaxForce.x);
			constraint->setTargetVelocity(3, physics->jointRotationVelocity.x);
		} else {
			constraint->setServo(3, false);
			constraint->setTargetVelocity(3, 0);
		}
		
		if (physics->jointMotor && physics->jointRotationLimitsY.min < physics->jointRotationLimitsY.max) {
			constraint->setServo(4, true);
			constraint->setServoTarget(4, physics->jointRotationTarget.y);
			constraint->setMaxMotorForce(4, physics->jointRotationMaxForce.y);
			constraint->setTargetVelocity(4, physics->jointRotationVelocity.y);
		} else {
			constraint->setServo(4, false);
			constraint->setTargetVelocity(4, 0);
		}
		
		if (physics->jointMotor && physics->jointRotationLimitsZ.min < physics->jointRotationLimitsZ.max) {
			constraint->setServo(5, true);
			constraint->setServoTarget(5, physics->jointRotationTarget.z);
			constraint->setMaxMotorForce(5, physics->jointRotationMaxForce.z);
			constraint->setTargetVelocity(5, physics->jointRotationVelocity.z);
		} else {
			constraint->setServo(5, false);
			constraint->setTargetVelocity(5, 0);
		}
	}
	
	if (!physics->jointMotor) {
		constraint->calculateTransforms();
		physics->jointTranslationTarget.x = constraint->getRelativePivotPosition(0);
		physics->jointTranslationTarget.y = constraint->getRelativePivotPosition(1);
		physics->jointTranslationTarget.z = constraint->getRelativePivotPosition(2);
		physics->jointRotationTarget.x = constraint->getAngle(0);
		physics->jointRotationTarget.y = constraint->getAngle(1);
		physics->jointRotationTarget.z = constraint->getAngle(2);
	}
}

struct PhysicsObject : btMotionState {
	std::weak_ptr<v4d::graphics::RenderableGeometryEntity> entityInstance;
	btTransform centerOfMassOffset {};
	btRigidBody* rigidbody = nullptr;
	btCollisionShape* collisionShape = nullptr;
	btGeneric6DofSpring2Constraint* constraint = nullptr;
	std::unordered_map<uint32_t, btGeneric6DofSpring2Constraint*> constrainedChildren {};
	PhysicsObject* parent = nullptr;
	
	PhysicsObject(std::shared_ptr<v4d::graphics::RenderableGeometryEntity> entity = nullptr) : entityInstance(entity) {
		centerOfMassOffset.setIdentity();
	}
	
	virtual void getWorldTransform(btTransform& centerOfMassWorldTrans) const override {
		auto entity = entityInstance.lock();if(!entity || entity->GetIndex()==-1)return;
		centerOfMassWorldTrans = GlmToBullet(entity->GetWorldTransform()) * centerOfMassOffset.inverse();
	}

	//Bullet only calls the update of worldtransform for active objects
	virtual void setWorldTransform(const btTransform& centerOfMassWorldTrans) override {
		auto entity = entityInstance.lock();if(!entity || entity->GetIndex()==-1)return;
		entity->SetWorldTransform(BulletToGlm(centerOfMassWorldTrans * centerOfMassOffset));
	}
	
	void Update() {
		auto entity = entityInstance.lock();if(!entity)return;
		auto physics = entity->physics.Lock();if(!physics)return;
		if (entity->GetIndex() == -1) return;

		// Wait until parent object's rigidbody exists before creating this rigidbody
		if (physics->jointParent != -1) {
			if (auto* _parent = GetPhysicsObject((uint32_t)physics->jointParent); !_parent || !_parent->rigidbody) {
				return;
			}
		}
		
		physics->physicsDirty = false;
		
		{// Set/Update Center Of Mass
			centerOfMassOffset.setIdentity();
			centerOfMassOffset.setOrigin(btVector3(physics->centerOfMass.x, physics->centerOfMass.y, physics->centerOfMass.z));
		}
		
		{// Create/Update Collision Shape
			if (physics->colliderDirty) {
				RemoveCollisionShape();
				physics->colliderDirty = false;
				// Add new collision shape
				if (physics->colliderType != v4d::scene::PhysicsInfo::ColliderType::NONE) {
					switch (physics->colliderType) {
						case v4d::scene::PhysicsInfo::ColliderType::SPHERE:
							collisionShape = new btSphereShape(btScalar(physics->boundingDistance));
						break;
						case v4d::scene::PhysicsInfo::ColliderType::BOX:
							collisionShape = new btBoxShape(btVector3(btScalar(physics->boundingBoxSize.x), btScalar(physics->boundingBoxSize.y), btScalar(physics->boundingBoxSize.z)));
						break;
						case v4d::scene::PhysicsInfo::ColliderType::MESH:{
							auto* mesh = new btTriangleMesh();
							globalTriangleMeshes.push_back(mesh);
							
							auto generateColliderMeshFunc = [&physics, &mesh](v4d::graphics::Mesh::VertexPosition* vertices, auto* indices, uint32_t indexCount){
								for (int i = 0; i < indexCount; i+=3) {
									btVector3 v0(vertices[indices[i]].x, vertices[indices[i]].y, vertices[indices[i]].z);
									btVector3 v1(vertices[indices[i+1]].x, vertices[indices[i+1]].y, vertices[indices[i+1]].z);
									btVector3 v2(vertices[indices[i+2]].x, vertices[indices[i+2]].y, vertices[indices[i+2]].z);
									mesh->addTriangle(v0, v1, v2, true);
									physics->boundingDistance = glm::max(physics->boundingDistance, glm::sqrt(float(v0.x()*v0.x() + v0.y()*v0.y() + v0.z()*v0.z())));
									physics->boundingDistance = glm::max(physics->boundingDistance, glm::sqrt(float(v1.x()*v1.x() + v1.y()*v1.y() + v1.z()*v1.z())));
									physics->boundingDistance = glm::max(physics->boundingDistance, glm::sqrt(float(v2.x()*v2.x() + v2.y()*v2.y() + v2.z()*v2.z())));
								}
							};
							
							// Generate collider mesh using custom physics mesh, or fallback on renderer components
							if (physics->colliderMeshVertices.size() > 0) {
								if (physics->colliderMeshIndices16.size() > 0) {
									generateColliderMeshFunc(physics->colliderMeshVertices.data(), physics->colliderMeshIndices16.data(), physics->colliderMeshIndices16.size());
								} else if (physics->colliderMeshIndices32.size() > 0) {
									generateColliderMeshFunc(physics->colliderMeshVertices.data(), physics->colliderMeshIndices32.data(), physics->colliderMeshIndices32.size());
								} else {
									LOG_ERROR("Empty colliderMeshVertices for generating the mesh collider")
									break;
								}
							} else if (entity->meshVertexPosition && entity->sharedGeometryData && entity->sharedGeometryData->geometries.size() > 0) {
								auto meshVertexPositions = entity->meshVertexPosition.Lock();
								if (meshVertexPositions && meshVertexPositions->data && entity->sharedGeometryData->geometries[0].vertexCount > 0) {
									if (physics->colliderMeshIndices16.size() > 0) {
										generateColliderMeshFunc(meshVertexPositions->data, physics->colliderMeshIndices16.data(), physics->colliderMeshIndices16.size());
									} else if (physics->colliderMeshIndices32.size() > 0) {
										generateColliderMeshFunc(meshVertexPositions->data, physics->colliderMeshIndices32.data(), physics->colliderMeshIndices32.size());
									} else if (entity->meshIndices16) {
										auto meshVertexIndices = entity->meshIndices16.Lock();
										if (meshVertexIndices && meshVertexIndices->data && entity->sharedGeometryData->geometries[0].indexCount > 0) {
											generateColliderMeshFunc(meshVertexPositions->data, meshVertexIndices->data, entity->sharedGeometryData->geometries[0].indexCount);
										} else {
											LOG_ERROR("Empty or Unallocated meshIndices16 for generating the mesh collider")
											break;
										}
									} else if (entity->meshIndices32) {
										auto meshVertexIndices = entity->meshIndices32.Lock();
										if (meshVertexIndices && meshVertexIndices->data && entity->sharedGeometryData->geometries[0].indexCount > 0) {
											generateColliderMeshFunc(meshVertexPositions->data, meshVertexIndices->data, entity->sharedGeometryData->geometries[0].indexCount);
										} else {
											LOG_ERROR("Empty or Unallocated meshIndices32 for generating the mesh collider")
											break;
										}
									} else {
										LOG_ERROR("Missing meshIndices component for mesh collider")
										break;
									}
								} else {
									LOG_ERROR("Empty or Unallocated meshVertexPosition for generating the mesh collider")
									break;
								}
							} else {
								LOG_ERROR("Missing meshVertexPosition component for mesh collider")
								break;
							}
							try {
								collisionShape = new btBvhTriangleMeshShape(mesh, true);
							} catch(...){
								LOG_ERROR("Exception occured in Bullet BVH generation from triangle mesh")
							}
						}break;
						case v4d::scene::PhysicsInfo::ColliderType::STATIC_PLANE:{
							collisionShape = new btStaticPlaneShape(btVector3(0, 0, 1), entity->GetWorldTransform()[3].z);
						}break;
						// case v4d::scene::PhysicsInfo::ColliderType::HEIGHTFIELD:
						// 	collisionShape = btHeightfieldTerrainShape();
						// break;
					}
					if (collisionShape) {
						globalCollisionShapes.push_back(collisionShape);
					}
				}
			}
		}

		{// Create/Update RigidBody
			RemoveRigidbody();
			
			if (collisionShape == nullptr) {
				LOG_ERROR("PhysicsObject has no collision shape")
				return;
			}
			
			btVector3 localInertia {0,0,0};
			btScalar mass = physics->mass;
			if (physics->rigidbodyType == v4d::scene::PhysicsInfo::RigidBodyType::DYNAMIC) {
				collisionShape->calculateLocalInertia(mass, localInertia);
			} else {
				mass = 0;
			}
			
			btRigidBody::btRigidBodyConstructionInfo rbInfo(mass, this, collisionShape, localInertia);
			rbInfo.m_restitution = physics->bounciness;
			rbInfo.m_friction = physics->friction;
			rbInfo.m_angularDamping = physics->angularDamping;
			
			rigidbody = new btRigidBody(rbInfo);
			rigidbody->setUserPointer(this);
			if (physics->rigidbodyType == v4d::scene::PhysicsInfo::RigidBodyType::KINEMATIC) {
				rigidbody->setCollisionFlags(rigidbody->getCollisionFlags() | btCollisionObject::CF_KINEMATIC_OBJECT);
				rigidbody->setActivationState(DISABLE_DEACTIVATION);
			}
			rigidbody->setAngularFactor(btVector3(physics->angularFactor.x, physics->angularFactor.y, physics->angularFactor.z));
			globalDynamicsWorld->addRigidBody(rigidbody);
			
			// Joints
			if (physics->jointParent != -1) {
				if (auto* _parent = GetPhysicsObject((uint32_t)physics->jointParent); _parent && _parent->rigidbody) {
					parent = _parent;
					
					constraint = new btGeneric6DofSpring2Constraint(*rigidbody, *_parent->rigidbody, GlmToBullet(physics->localJointPoint), GlmToBullet(physics->parentJointPoint));
					
					for (int i = 0; i < 6; ++i) {
						constraint->enableMotor(i, true);
						constraint->setParam(BT_CONSTRAINT_ERP, 0.8, i);
						constraint->setParam(BT_CONSTRAINT_CFM, 0.1, i);
					}
					
					physics->jointIsDirty = true;
					UpdateConstraintJointPhysics(constraint, physics.operator->());
					
					_parent->constrainedChildren[physics->uniqueId] = constraint;
					globalDynamicsWorld->addConstraint(constraint, true);
					rigidbody->setActivationState(DISABLE_DEACTIVATION);
					
					// // Continuous Collision Detection
					// rigidbody->setCcdMotionThreshold(0.0001f);
					// rigidbody->setCcdSweptSphereRadius(physics->boundingDistance);
				}
			}
		}
	}
	
	void RemoveCollisionShape() {
		if (collisionShape) {
			for (int i = 0; i < globalCollisionShapes.size(); ++i) {
				if (globalCollisionShapes[i] == collisionShape) {
					if (i != globalCollisionShapes.size()-1) globalCollisionShapes[i] = globalCollisionShapes[globalCollisionShapes.size()-1];
					globalCollisionShapes.pop_back();
					break;
				}
			}
			delete collisionShape;
			collisionShape = nullptr;
		}
	}
	
	void RemoveConstraint() {
		if (constraint) {
			if (parent) {
				for (auto& [childId, c] : parent->constrainedChildren) if (c == constraint) {
					c = nullptr;
					break;
				}
			}
			globalDynamicsWorld->removeConstraint(constraint);
			delete constraint;
			constraint = nullptr;
		}
	}
	
	void RemoveConstrainedChildren() {
		for (auto& [childId, c] : constrainedChildren) if (c) {
			auto* child = GetPhysicsObject(childId);
			if (child && child->constraint && child->constraint == c) {
				child->RemoveRigidbody();
				auto entity = child->entityInstance.lock();
				if(!entity) continue;
				auto physics = entity->physics.Lock();
				if (!physics) continue;
				physics->physicsDirty = true;
			}
			c = nullptr;
		}
	}
	
	void RemoveRigidbody() {
		RemoveConstrainedChildren();
		RemoveConstraint();
		if (rigidbody) {
			globalDynamicsWorld->removeRigidBody(rigidbody);
			delete rigidbody;
			rigidbody = nullptr;
		}
	}
	
	~PhysicsObject() {
		RemoveRigidbody();
		RemoveCollisionShape();
	}
	
};

// void NearCallback(btBroadphasePair& collisionPair, btCollisionDispatcher& dispatcher, const btDispatcherInfo& dispatchInfo)
// {
// 	btCollisionObject* colObj0 = (btCollisionObject*)collisionPair.m_pProxy0->m_clientObject;
// 	btCollisionObject* colObj1 = (btCollisionObject*)collisionPair.m_pProxy1->m_clientObject;

// 	if (dispatcher.needsCollision(colObj0, colObj1))
// 	{
// 		btCollisionObjectWrapper obj0Wrap(0, colObj0->getCollisionShape(), colObj0, colObj0->getWorldTransform(), -1, -1);
// 		btCollisionObjectWrapper obj1Wrap(0, colObj1->getCollisionShape(), colObj1, colObj1->getWorldTransform(), -1, -1);

// 		//dispatcher will keep algorithms persistent in the collision pair
// 		if (!collisionPair.m_algorithm)
// 		{
// 			collisionPair.m_algorithm = dispatcher.findAlgorithm(&obj0Wrap, &obj1Wrap, 0, BT_CONTACT_POINT_ALGORITHMS);
// 		}

// 		if (collisionPair.m_algorithm)
// 		{
// 			btManifoldResult contactPointResult(&obj0Wrap, &obj1Wrap);

// 			if (dispatchInfo.m_dispatchFunc == btDispatcherInfo::DISPATCH_DISCRETE)
// 			{
// 				//discrete collision detection query

// 				collisionPair.m_algorithm->processCollision(&obj0Wrap, &obj1Wrap, dispatchInfo, &contactPointResult);
// 			}
// 			else
// 			{
// 				//continuous collision detection query, time of impact (toi)
// 				btScalar toi = collisionPair.m_algorithm->calculateTimeOfImpact(colObj0, colObj1, dispatchInfo, &contactPointResult);
// 				if (dispatchInfo.m_timeOfImpact > toi)
// 					dispatchInfo.m_timeOfImpact = toi;
// 			}
// 		}

// 		// Custom stuff here
// 		btRigidBody* rb0 = dynamic_cast<btRigidBody*>(colObj0);
// 		btRigidBody* rb1 = dynamic_cast<btRigidBody*>(colObj1);
// 		if (rb0 && rb1) {
// 			PhysicsObject* obj0 = static_cast<PhysicsObject*>(rb0->getUserPointer());
// 			PhysicsObject* obj1 = static_cast<PhysicsObject*>(rb1->getUserPointer());
// 			auto entity0 = obj0->entityInstance.lock();
// 			auto entity1 = obj1->entityInstance.lock();
// 			if (entity0 && entity1) {
// 				// LOG(
// 				// 	"Collision between " << 
// 				// 	v4d::modular::ModuleID(entity0->entityInstanceInfo.moduleVen, entity0->entityInstanceInfo.moduleId).String() << ":" << entity0->entityInstanceInfo.objId
// 				// 	<< " and " <<
// 				// 	v4d::modular::ModuleID(entity1->entityInstanceInfo.moduleVen, entity1->entityInstanceInfo.moduleId).String() << ":" << entity1->entityInstanceInfo.objId
// 				// )
// 				auto physics0 = entity0->physics.Lock();
// 				auto physics1 = entity1->physics.Lock();
// 				if (physics0 && physics1) {
					
// 				}
// 			}
// 		}
// 	}
// }

void ContactStarted(btPersistentManifold* const& manifold) {
	const btRigidBody* rb0 = dynamic_cast<const btRigidBody*>(manifold->getBody0());
	const btRigidBody* rb1 = dynamic_cast<const btRigidBody*>(manifold->getBody1());
	if (rb0 && rb1) {
		PhysicsObject* obj0 = static_cast<PhysicsObject*>(rb0->getUserPointer());
		PhysicsObject* obj1 = static_cast<PhysicsObject*>(rb1->getUserPointer());
		auto entity0 = obj0->entityInstance.lock();
		auto entity1 = obj1->entityInstance.lock();
		if (entity0 && entity1) {
			// LOG(
			// 	"Collision Started between " << 
			// 	v4d::modular::ModuleID(entity0->entityInstanceInfo.moduleVen, entity0->entityInstanceInfo.moduleId).String() << ":" << entity0->entityInstanceInfo.objId
			// 	<< " and " <<
			// 	v4d::modular::ModuleID(entity1->entityInstanceInfo.moduleVen, entity1->entityInstanceInfo.moduleId).String() << ":" << entity1->entityInstanceInfo.objId
			// )
			auto physics0 = entity0->physics.Lock();
			auto physics1 = entity1->physics.Lock();
			if (physics0 && physics1) {
				++physics0->contacts;
				++physics1->contacts;
			}
		}
	}
}

void ContactEnded(btPersistentManifold* const& manifold) {
		const btRigidBody* rb0 = dynamic_cast<const btRigidBody*>(manifold->getBody0());
	const btRigidBody* rb1 = dynamic_cast<const btRigidBody*>(manifold->getBody1());
	if (rb0 && rb1) {
		PhysicsObject* obj0 = static_cast<PhysicsObject*>(rb0->getUserPointer());
		PhysicsObject* obj1 = static_cast<PhysicsObject*>(rb1->getUserPointer());
		auto entity0 = obj0->entityInstance.lock();
		auto entity1 = obj1->entityInstance.lock();
		if (entity0 && entity1) {
			// LOG(
			// 	"Collision Ended between " << 
			// 	v4d::modular::ModuleID(entity0->entityInstanceInfo.moduleVen, entity0->entityInstanceInfo.moduleId).String() << ":" << entity0->entityInstanceInfo.objId
			// 	<< " and " <<
			// 	v4d::modular::ModuleID(entity1->entityInstanceInfo.moduleVen, entity1->entityInstanceInfo.moduleId).String() << ":" << entity1->entityInstanceInfo.objId
			// )
			auto physics0 = entity0->physics.Lock();
			auto physics1 = entity1->physics.Lock();
			if (physics0 && physics1) {
				--physics0->contacts;
				--physics1->contacts;
			}
		}
	}
}

#define PHYSICS_REFRESH_COLLIDERS_OUTSIDE_OF_LOOP

V4D_MODULE_CLASS(V4D_Mod) {
	
	V4D_MODULE_FUNC(void, InitRenderer, v4d::graphics::Renderer* _r) {
		r = _r;
	}
	
	V4D_MODULE_FUNC(void, LoadScene, v4d::scene::Scene* _s) {
		scene = _s;
		
		globalCollisionConfiguration = new btDefaultCollisionConfiguration();
		globalPhysicsDispatcher = new btCollisionDispatcher(globalCollisionConfiguration);
		globalOverlappingPairCache = new btDbvtBroadphase();
		globalConstraintSolver = new btSequentialImpulseConstraintSolver();
		globalDynamicsWorld = new btDiscreteDynamicsWorld(globalPhysicsDispatcher, globalOverlappingPairCache, globalConstraintSolver, globalCollisionConfiguration);
		
		globalDynamicsWorld->setGravity(btVector3(scene->gravityVector.x, scene->gravityVector.y, scene->gravityVector.z));
		
		// #ifdef _DEBUG
			globalDynamicsWorld->setDebugDrawer(&debugDrawer);
		// #endif
		
		// ((btCollisionDispatcher*)globalPhysicsDispatcher)->setNearCallback(NearCallback);
		
		gContactStartedCallback = ContactStarted;
		gContactEndedCallback = ContactEnded;
		
		// globalPhysicsDispatcher->

	}
	
	V4D_MODULE_FUNC(void, UnloadScene) {
		for (auto&[i,obj] : physicsObjects) delete obj;
		physicsObjects.clear();
		
		for (int i = 0; i < globalCollisionShapes.size(); i++) {
			auto* o = globalCollisionShapes[i];
			globalCollisionShapes[i] = 0;
			delete o;
		}
		globalCollisionShapes.clear();
		
		for (int i = 0; i < globalTriangleMeshes.size(); i++) {
			auto* o = globalTriangleMeshes[i];
			globalTriangleMeshes[i] = 0;
			delete o;
		}
		globalTriangleMeshes.clear();
		
		delete globalDynamicsWorld;
		delete globalConstraintSolver;
		delete globalOverlappingPairCache;
		delete globalPhysicsDispatcher;
		delete globalCollisionConfiguration;
	}
	
	V4D_MODULE_FUNC(void, PhysicsUpdate, double deltaTime) {
		#ifdef PHYSICS_REFRESH_COLLIDERS_OUTSIDE_OF_LOOP
			static std::vector<PhysicsObject*> objectsToRefreshCollisionShape {};
			objectsToRefreshCollisionShape.clear();
		#endif
		
		// Set Gravity vector
		globalDynamicsWorld->setGravity(btVector3(scene->gravityVector.x, scene->gravityVector.y, scene->gravityVector.z));
		
		// Erase physics objects that have been destroyed
		for (auto it = physicsObjects.begin(); it != physicsObjects.end();) {
			auto&[i, physicsObj] = *it;
			auto e = physicsObj->entityInstance.lock();
			if (!e || e->GetIndex() == -1 || !e->physics) {
				goto Delete;
			}
			{
				auto physics = e->physics.Lock();
				if (!physics) goto Delete;
				if (physics->rigidbodyType == v4d::scene::PhysicsInfo::RigidBodyType::NONE) goto Delete;
				if (physics->colliderType == v4d::scene::PhysicsInfo::ColliderType::NONE) goto Delete;
			}
			Continue:
				++it;
				continue;
			Delete:
				delete physicsObj;
				it = physicsObjects.erase(it);
		}
		
		// Loop through all physics components within active entities
		v4d::graphics::RenderableGeometryEntity::physicsComponents.ForEach_LockEntities([](int32_t entityInstanceIndex, auto& physics){
			if (entityInstanceIndex == -1) return;
			
			// Fetch/Add physics object
			PhysicsObject* physicsObj;
			try {
				physicsObj = physicsObjects.at(physics.uniqueId);
			} catch(...) {
				auto entity = v4d::graphics::RenderableGeometryEntity::Get(entityInstanceIndex);
				if (!entity) return;
				assert(entity->GetIndex() == entityInstanceIndex);
				if (physics.rigidbodyType == v4d::scene::PhysicsInfo::RigidBodyType::NONE) return;
				if (physics.colliderType == v4d::scene::PhysicsInfo::ColliderType::NONE) return;
				physicsObj = (physicsObjects[physics.uniqueId] = new PhysicsObject(entity));
			}
			
			// Update physics objects
			if (physics.physicsDirty) {
				#ifdef PHYSICS_REFRESH_COLLIDERS_OUTSIDE_OF_LOOP
					objectsToRefreshCollisionShape.push_back(physicsObj);
				#else
					physicsObj->Update();
				#endif
			}
			
			// Realtime Update
			auto* rb = physicsObj->rigidbody;
			if (rb) {
				
				// Update rigidbody
				if (rb->getFriction() != physics.friction) 
					rb->setFriction(physics.friction);
				if (rb->getRestitution() != physics.bounciness) 
					rb->setRestitution(physics.bounciness);
				if (rb->getAngularFactor().x() != physics.angularFactor.x || rb->getAngularFactor().y() != physics.angularFactor.y || rb->getAngularFactor().z() != physics.angularFactor.z) 
					rb->setAngularFactor(btVector3(physics.angularFactor.x, physics.angularFactor.y, physics.angularFactor.z));
				// if (rb->getAngularDamping() != physics.angularDamping) 
				// 	rb->setAngularDamping(physics.angularDamping);
				
				// Update joint
				if (physics.jointParent != -1 && physicsObj->constraint) {
					UpdateConstraintJointPhysics(physicsObj->constraint, &physics);
				}
				
				// Apply forces
				if (physics.addedForce || physics.physicsForceImpulses.size() > 0) {
					if (physics.addedForce) {
						if (physics.forcePoint.length() == 0) {
							rb->applyCentralForce(btVector3(physics.forceDirection.x, physics.forceDirection.y, physics.forceDirection.z));
						} else {
							rb->applyForce(btVector3(physics.forceDirection.x, physics.forceDirection.y, physics.forceDirection.z), btVector3(physics.forcePoint.x, physics.forcePoint.y, physics.forcePoint.z));
						}
					}
					if (physics.physicsForceImpulses.size() > 0) {
						auto&[impulseDir, atPoint] = physics.physicsForceImpulses.front();
						if (atPoint.length() == 0) {
							rb->applyCentralImpulse(btVector3(impulseDir.x, impulseDir.y, impulseDir.z));
						} else {
							rb->applyImpulse(btVector3(impulseDir.x, impulseDir.y, impulseDir.z), btVector3(atPoint.x, atPoint.y, atPoint.z));
						}
						physics.physicsForceImpulses.pop();
					}
				}
				
			}
		});
		
		// Update physics objects
		#ifdef PHYSICS_REFRESH_COLLIDERS_OUTSIDE_OF_LOOP
			for (auto* physicsObj : objectsToRefreshCollisionShape) {
				physicsObj->Update();
			}
		#endif
		
		// Physics Simulation
		try {
			globalDynamicsWorld->stepSimulation(deltaTime);
		} catch(...){
			LOG_ERROR("Exception occured in Bullet Physics stepSimulation()")
		}
	}
	
	// #ifdef _DEBUG
		V4D_MODULE_FUNC(void, SecondaryRenderUpdate2, VkCommandBuffer commandBuffer) {
			if (globalDynamicsWorld && (scene->camera.debugOptions & DEBUG_OPTION_PHYSICS)) {
				globalDynamicsWorld->debugDrawWorld();
			}
		}
	// #endif
	
};
