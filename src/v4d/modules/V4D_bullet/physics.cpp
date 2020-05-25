#define _V4D_MODULE
#include <v4d.h>

#include "btBulletDynamicsCommon.h"

btCollisionConfiguration* globalCollisionConfiguration = nullptr;
btDispatcher* globalPhysicsDispatcher = nullptr;
btBroadphaseInterface* globalOverlappingPairCache = nullptr;
btConstraintSolver* globalConstraintSolver = nullptr;
btDynamicsWorld* globalDynamicsWorld = nullptr;
btAlignedObjectArray<btCollisionShape*> globalCollisionShapes {};
btAlignedObjectArray<btTriangleMesh*> globalTriangleMeshes {};


glm::dvec3 BulletToGlm(const btVector3& v) { return glm::dvec3(v.getX(), v.getY(), v.getZ()); }
btVector3 GlmToBullet(const glm::dvec3& v) { return btVector3(v.x, v.y, v.z); }
glm::quat BulletToGlm(const btQuaternion& q) { return glm::quat(q.getW(), q.getX(), q.getY(), q.getZ()); }
btQuaternion GlmToBullet(const glm::quat& q) { return btQuaternion(q.x, q.y, q.z, q.w); }
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

struct PhysicsObject : btMotionState {
	v4d::graphics::ObjectInstance* objectInstance = nullptr;
	btTransform centerOfMassOffset {};
	btRigidBody* rigidbody = nullptr;
	bool isGroupedCollisionShape = false;
	btCollisionShape* groupedCollisionShape = nullptr;
	std::vector<btCollisionShape*> localCollisionShapes {};
	
	PhysicsObject(v4d::graphics::ObjectInstance* obj = nullptr) : objectInstance(obj) {
		if (obj) RefreshCenterOfMass();
	}
	
	void RefreshCenterOfMass() {
		centerOfMassOffset.setIdentity();
		centerOfMassOffset.setOrigin(btVector3(objectInstance->centerOfMass.x, objectInstance->centerOfMass.y, objectInstance->centerOfMass.z));
	}
	
	virtual void getWorldTransform(btTransform& centerOfMassWorldTrans) const override {
		std::lock_guard physicsLock(objectInstance->physicsMutex);
		centerOfMassWorldTrans = GlmToBullet(objectInstance->GetWorldTransform()) * centerOfMassOffset.inverse();
	}

	//Bullet only calls the update of worldtransform for active objects
	virtual void setWorldTransform(const btTransform& centerOfMassWorldTrans) override {
		std::lock_guard physicsLock(objectInstance->physicsMutex);
		objectInstance->SetWorldTransform(BulletToGlm(centerOfMassWorldTrans * centerOfMassOffset));
	}
	
	void RefreshCollisionShape() {
		// Delete grouped collision shape
		if (groupedCollisionShape && isGroupedCollisionShape) {
			for (int i = 0; i < globalCollisionShapes.size(); ++i) {
				if (globalCollisionShapes[i] == groupedCollisionShape) {
					if (i != globalCollisionShapes.size()-1) globalCollisionShapes[i] = globalCollisionShapes[globalCollisionShapes.size()-1];
					globalCollisionShapes.pop_back();
					break;
				}
			}
			delete groupedCollisionShape;
		}
		groupedCollisionShape = nullptr;
		
		// calculate geometries shapes
		for (auto& geometryInstance : objectInstance->GetGeometries()) {
			auto geom = geometryInstance.geometry;
			btCollisionShape* collisionShape = (btCollisionShape*)geom->colliderShapeObject;
			if (geom->colliderDirty) {
				// Delete old collision shape
				if (collisionShape) {
					for (int i = 0; i < globalCollisionShapes.size(); ++i) {
						if (globalCollisionShapes[i] == collisionShape) {
							if (i != globalCollisionShapes.size()-1) globalCollisionShapes[i] = globalCollisionShapes[globalCollisionShapes.size()-1];
							globalCollisionShapes.pop_back();
							break;
						}
					}
					for (int i = 0; i < localCollisionShapes.size(); ++i) {
						if (localCollisionShapes[i] == collisionShape) {
							if (i != localCollisionShapes.size()-1) localCollisionShapes[i] = localCollisionShapes[localCollisionShapes.size()-1];
							localCollisionShapes.pop_back();
							break;
						}
					}
					delete collisionShape;
					collisionShape = nullptr;
				}
				// Add new collision shape
				if (geom->colliderType != v4d::graphics::Geometry::ColliderType::NONE) {
					switch (geom->colliderType) {
						case v4d::graphics::Geometry::ColliderType::SPHERE:
							collisionShape = new btSphereShape(btScalar(geom->boundingDistance));
						break;
						case v4d::graphics::Geometry::ColliderType::BOX:
							collisionShape = new btBoxShape(btVector3(btScalar(geom->boundingBoxSize.x), btScalar(geom->boundingBoxSize.y), btScalar(geom->boundingBoxSize.z)));
						break;
						case v4d::graphics::Geometry::ColliderType::TRIANGLE_MESH:
							{
								auto* mesh = new btTriangleMesh();
								globalTriangleMeshes.push_back(mesh);
								auto* indices = geom->GetIndexPtr(0);
								auto* vertices = geom->GetVertexPtr(0);
								for (int i = 0; i < geom->indexCount; i+=3) {
									btVector3 v0(vertices[indices[i]].pos.x, vertices[indices[i]].pos.y, vertices[indices[i]].pos.z);
									btVector3 v1(vertices[indices[i+1]].pos.x, vertices[indices[i+1]].pos.y, vertices[indices[i+1]].pos.z);
									btVector3 v2(vertices[indices[i+2]].pos.x, vertices[indices[i+2]].pos.y, vertices[indices[i+2]].pos.z);
									mesh->addTriangle(v0, v1, v2, true);
								}
								collisionShape = new btBvhTriangleMeshShape(mesh, true);
							}
						break;
						case v4d::graphics::Geometry::ColliderType::STATIC_PLANE:
							collisionShape = new btStaticPlaneShape(btVector3(0, 0, 1), objectInstance->GetWorldTransform()[3].z);
						break;
						// case v4d::graphics::Geometry::ColliderType::HEIGHTFIELD:
						// 	collisionShape = 
						// break;
					}
					if (collisionShape) {
						globalCollisionShapes.push_back(collisionShape);
						localCollisionShapes.push_back(collisionShape);
						geom->colliderShapeObject = collisionShape;
					}
				}
				// Assign collision shape
				geom->colliderShapeObject = collisionShape;
				geom->colliderDirty = false;
			}
		}
		
		if (localCollisionShapes.size() == 1) {
			groupedCollisionShape = localCollisionShapes[0];
			isGroupedCollisionShape = false;
		} else if (localCollisionShapes.size() > 1) {
			//TODO generate one group collision shape that shall contain all sub-geometries
			LOG_WARN("groupedCollisionShape Not implemented yet")
			isGroupedCollisionShape = true;
		} else if (isGroupedCollisionShape) {
			isGroupedCollisionShape = false;
		}
	}
	
	void RefreshRigidbody() {
		objectInstance->LockPhysics();
			RemoveRigidbody();
			if (groupedCollisionShape == nullptr) {
				LOG_ERROR("PhysicsObject has no collision shape")
				objectInstance->UnlockPhysics();
				return;
			}
			btVector3 localInertia {0,0,0};
			btScalar mass = objectInstance->mass;
			if (objectInstance->rigidbodyType == v4d::graphics::ObjectInstance::RigidBodyType::DYNAMIC) {
				// groupedCollisionShape->calculateLocalInertia(mass, localInertia);
			} else {
				mass = 0;
			}
			btRigidBody::btRigidBodyConstructionInfo rbInfo(mass, this, groupedCollisionShape, localInertia);
			
			//TODO set additional stuff in rbInfo
			
			rigidbody = new btRigidBody(rbInfo);
			rigidbody->setUserPointer(this);
			if (objectInstance->rigidbodyType == v4d::graphics::ObjectInstance::RigidBodyType::KINEMATIC) {
				rigidbody->setCollisionFlags(rigidbody->getCollisionFlags() | btCollisionObject::CF_KINEMATIC_OBJECT);
				rigidbody->setActivationState(DISABLE_DEACTIVATION);
			}
			globalDynamicsWorld->addRigidBody(rigidbody);
		objectInstance->UnlockPhysics();
	}
	
	void Refresh() {
		RefreshCenterOfMass();
		RefreshCollisionShape();
		RefreshRigidbody();
	}
	
	void RemoveRigidbody() {
		if (rigidbody) {
			globalDynamicsWorld->removeRigidBody(rigidbody);
			delete rigidbody;
			rigidbody = nullptr;
		}
	}
	
};

v4d::graphics::Renderer* r = nullptr;
v4d::graphics::Scene* scene = nullptr;
V4D_Renderer* rendererModule = nullptr;

std::unordered_map<v4d::graphics::ObjectInstance*, PhysicsObject> physicsObjects {};
std::queue<v4d::graphics::ObjectInstance*> objectsToRefresh {};

void RemovePhysicsObject(v4d::graphics::ObjectInstance* obj) {
	obj->LockPhysics();
		physicsObjects[obj].RemoveRigidbody();
		physicsObjects.erase(obj);
		obj->physicsObject = nullptr;
		obj->removePhysicsCallback = nullptr;
	obj->UnlockPhysics();
}

void AddPhysicsObject(v4d::graphics::ObjectInstance* obj) {
	physicsObjects[obj] = obj;
	obj->physicsObject = &physicsObjects[obj];
	obj->removePhysicsCallback = RemovePhysicsObject;
}

void UpdatePhysicsObject(v4d::graphics::ObjectInstance* obj) {
	try {
		physicsObjects.at(obj);
	} catch(...) {
		LOG_ERROR("physics object does not exist for this objectInstance")
		return;
	}
	physicsObjects[obj].Refresh();
}

extern "C" {
	
	bool ModuleIsPrimary() {
		return true;
	}
	
	void ModuleLoad() {
		// #ifdef _DEBUG
			rendererModule = V4D_Renderer::LoadModule(THIS_MODULE);
		// #endif
	}
	
	void Init(v4d::graphics::Renderer* _r, v4d::graphics::Scene* _s) {
		r = _r;
		scene = _s;
	}
	
	void LoadScene() {
		globalCollisionConfiguration = new btDefaultCollisionConfiguration();
		globalPhysicsDispatcher = new btCollisionDispatcher(globalCollisionConfiguration);
		globalOverlappingPairCache = new btDbvtBroadphase();
		globalConstraintSolver = new btSequentialImpulseConstraintSolver();
		globalDynamicsWorld = new btDiscreteDynamicsWorld(globalPhysicsDispatcher, globalOverlappingPairCache, globalConstraintSolver, globalCollisionConfiguration);
		
		globalDynamicsWorld->setGravity(btVector3(0, 0, -10));
		
		// #ifdef _DEBUG
			if (rendererModule) {
				globalDynamicsWorld->setDebugDrawer((btIDebugDraw*)rendererModule->ModuleGetCustomPtr(0));
				rendererModule->ModuleSetCustomPtr(0, globalDynamicsWorld);
			}
		// #endif
	}
	
	void UnloadScene() {
		
		for (int i = 0; i < globalCollisionShapes.size(); i++) {
			auto* obj = globalCollisionShapes[i];
			globalCollisionShapes[i] = 0;
			delete obj;
		}
		globalCollisionShapes.clear();
		
		for (int i = 0; i < globalTriangleMeshes.size(); i++) {
			auto* obj = globalTriangleMeshes[i];
			globalTriangleMeshes[i] = 0;
			delete obj;
		}
		globalTriangleMeshes.clear();
		
		delete globalDynamicsWorld;
		delete globalConstraintSolver;
		delete globalOverlappingPairCache;
		delete globalPhysicsDispatcher;
		delete globalCollisionConfiguration;
	}
	
	void RunUi() {
		
	}
	
	void StepSimulation(double deltaTime) {
		// Refresh info from scene objects
		scene->Lock();
			for (auto* obj : scene->objectInstances) if (obj->IsActive() || obj->physicsDirty) {
				obj->LockPhysics();
					if (obj->IsActive() && !obj->physicsObject && obj->rigidbodyType != v4d::graphics::ObjectInstance::RigidBodyType::NONE) {
						AddPhysicsObject(obj);
					}
					if (obj->physicsDirty) {
						if (obj->rigidbodyType == v4d::graphics::ObjectInstance::RigidBodyType::NONE) {
							if (obj->physicsObject) {
								RemovePhysicsObject(obj);
							}
						} else {
							objectsToRefresh.emplace(obj);
						}
						obj->physicsDirty = false;
					}
					if ((obj->addedForce || obj->physicsForceImpulses.size() > 0) && obj->physicsObject) {
						std::lock_guard lock(obj->physicsMutex);
						auto* rb = ((PhysicsObject*)obj->physicsObject)->rigidbody;
						if (rb) {
							if (obj->addedForce) {
								if (obj->forcePoint.length() == 0) {
									rb->applyCentralForce(btVector3(obj->forceDirection.x, obj->forceDirection.y, obj->forceDirection.z));
								} else {
									rb->applyForce(btVector3(obj->forceDirection.x, obj->forceDirection.y, obj->forceDirection.z), btVector3(obj->forcePoint.x, obj->forcePoint.y, obj->forcePoint.z));
								}
							}
							if (obj->physicsForceImpulses.size() > 0) {
								auto&[impulseDir, atPoint] = obj->physicsForceImpulses.front();
								if (atPoint.length() == 0) {
									rb->applyCentralImpulse(btVector3(impulseDir.x, impulseDir.y, impulseDir.z));
								} else {
									rb->applyImpulse(btVector3(impulseDir.x, impulseDir.y, impulseDir.z), btVector3(atPoint.x, atPoint.y, atPoint.z));
								}
								obj->physicsForceImpulses.pop();
							}
						}
					}
				obj->UnlockPhysics();
			}
		scene->Unlock();
		
		// Generate/Update collision shapes
		while (!objectsToRefresh.empty()) {
			UpdatePhysicsObject(objectsToRefresh.front());
			objectsToRefresh.pop();
		}
		
		// Physics Simulation
		globalDynamicsWorld->stepSimulation(deltaTime);
	}
	
	bool RayCastClosest(v4d::graphics::Scene::RayCastHit* hit, glm::dvec3 origin, glm::dvec3 target, uint32_t mask) {
		if (!hit) {
			LOG_ERROR("RayCastClosest: The given hit pointer is null")
			return false;
		}
		if (globalDynamicsWorld) {
			btVector3 from = GlmToBullet(origin);
			btVector3 to = GlmToBullet(target);
			btCollisionWorld::ClosestRayResultCallback rayResult(from, to);
			rayResult.m_collisionFilterMask = mask;
			globalDynamicsWorld->rayTest(from, to, rayResult);
			if (rayResult.hasHit()) {
				*hit = {
					((PhysicsObject*)rayResult.m_collisionObject->getUserPointer())->objectInstance,
					BulletToGlm(rayResult.m_hitPointWorld),
					BulletToGlm(rayResult.m_hitNormalWorld)
				};
				return true;
			}
		}
		return false;
	}
	
	int RayCastAll(std::vector<v4d::graphics::Scene::RayCastHit>* hits, glm::dvec3 origin, glm::dvec3 target, uint32_t mask) {
		if (!hits) {
			LOG_ERROR("RayCastAll: The given hits pointer is null")
			return 0;
		}
		if (globalDynamicsWorld) {
			btVector3 from = GlmToBullet(origin);
			btVector3 to = GlmToBullet(target);
			btCollisionWorld::AllHitsRayResultCallback rayResult(from, to);
			rayResult.m_collisionFilterMask = mask;
			globalDynamicsWorld->rayTest(from, to, rayResult);
			if (rayResult.hasHit()) {
				int nbHits = rayResult.m_collisionObjects.size();
				hits->reserve(nbHits);
				for (int i = 0; i < nbHits; ++i) {
					hits->emplace_back(
						((PhysicsObject*)rayResult.m_collisionObjects[i]->getUserPointer())->objectInstance,
						BulletToGlm(rayResult.m_hitPointWorld[i]),
						BulletToGlm(rayResult.m_hitNormalWorld[i])
					);
				}
				return nbHits;
			}
		}
		return 0;
	}
	
}
