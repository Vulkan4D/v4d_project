#define _V4D_MODULE
#include <v4d.h>
#include "btBulletDynamicsCommon.h"
#include "../V4D_hybrid/camera_options.hh"

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

struct PhysicsObject : btMotionState {
	v4d::scene::ObjectInstanceWeakPtr objectInstance;
	btTransform centerOfMassOffset {};
	btRigidBody* rigidbody = nullptr;
	bool isGroupedCollisionShape = false;
	btCollisionShape* groupedCollisionShape = nullptr;
	std::vector<btCollisionShape*> localCollisionShapes {};
	
	PhysicsObject(v4d::scene::ObjectInstancePtr obj = nullptr) : objectInstance(obj) {
		if (obj) RefreshCenterOfMass();
	}
	
	void RefreshCenterOfMass() {
		auto obj = objectInstance.lock();if(!obj)return;
		centerOfMassOffset.setIdentity();
		centerOfMassOffset.setOrigin(btVector3(obj->centerOfMass.x, obj->centerOfMass.y, obj->centerOfMass.z));
	}
	
	virtual void getWorldTransform(btTransform& centerOfMassWorldTrans) const override {
		auto obj = objectInstance.lock();if(!obj)return;
		centerOfMassWorldTrans = GlmToBullet(obj->GetWorldTransform()) * centerOfMassOffset.inverse();
	}

	//Bullet only calls the update of worldtransform for active objects
	virtual void setWorldTransform(const btTransform& centerOfMassWorldTrans) override {
		auto obj = objectInstance.lock();if(!obj)return;
		obj->SetWorldTransform(BulletToGlm(centerOfMassWorldTrans * centerOfMassOffset));
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
		auto obj = objectInstance.lock();if(!obj)return;
		for (auto& geometryInstance : obj->GetGeometries()) {
			auto geom = geometryInstance.geometry;
			btCollisionShape* collisionShape = (btCollisionShape*)geom->colliderShapeObject;
			if (geom->colliderDirty || (!geom->active && collisionShape)) {
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
				if (geom->active && geom->colliderType != v4d::scene::Geometry::ColliderType::NONE) {
					switch (geom->colliderType) {
						case v4d::scene::Geometry::ColliderType::SPHERE:
							collisionShape = new btSphereShape(btScalar(geom->boundingDistance));
						break;
						case v4d::scene::Geometry::ColliderType::BOX:
							collisionShape = new btBoxShape(btVector3(btScalar(geom->boundingBoxSize.x), btScalar(geom->boundingBoxSize.y), btScalar(geom->boundingBoxSize.z)));
						break;
						case v4d::scene::Geometry::ColliderType::TRIANGLE_MESH:
							{
								int indexCount;
								v4d::scene::Geometry::IndexBuffer_T* indices;
								if (geom->simplifiedMeshIndices.size()) {
									indexCount = geom->simplifiedMeshIndices.size();
									indices = geom->simplifiedMeshIndices.data();
								} else {
									indexCount = geom->indexCount;
									indices = geom->GetIndexPtr(0);
								}
								auto* mesh = new btTriangleMesh();
								globalTriangleMeshes.push_back(mesh);
								auto* vertices = geom->GetVertexPtr(0);
								for (int i = 0; i < indexCount; i+=3) {
									btVector3 v0(vertices[indices[i]].pos.x, vertices[indices[i]].pos.y, vertices[indices[i]].pos.z);
									btVector3 v1(vertices[indices[i+1]].pos.x, vertices[indices[i+1]].pos.y, vertices[indices[i+1]].pos.z);
									btVector3 v2(vertices[indices[i+2]].pos.x, vertices[indices[i+2]].pos.y, vertices[indices[i+2]].pos.z);
									mesh->addTriangle(v0, v1, v2, true);
								}
								collisionShape = new btBvhTriangleMeshShape(mesh, true);
							}
						break;
						case v4d::scene::Geometry::ColliderType::STATIC_PLANE:
							collisionShape = new btStaticPlaneShape(btVector3(0, 0, 1), obj->GetWorldTransform()[3].z);
						break;
						// case v4d::scene::Geometry::ColliderType::HEIGHTFIELD:
						// 	collisionShape = 
						// break;
					}
					if (!obj->IsActive()){
						delete collisionShape;
						collisionShape = nullptr;
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
		auto obj = objectInstance.lock();if(!obj)return;
		RemoveRigidbody();
		if (groupedCollisionShape == nullptr) {
			LOG_ERROR("PhysicsObject has no collision shape")
			return;
		}
		btVector3 localInertia {0,0,0};
		btScalar mass = obj->mass;
		if (obj->rigidbodyType == v4d::scene::ObjectInstance::RigidBodyType::DYNAMIC) {
			// groupedCollisionShape->calculateLocalInertia(mass, localInertia);
		} else {
			mass = 0;
		}
		btRigidBody::btRigidBodyConstructionInfo rbInfo(mass, this, groupedCollisionShape, localInertia);
		
		//TODO set additional stuff in rbInfo
		rbInfo.m_restitution = 0.7;
		
		rigidbody = new btRigidBody(rbInfo);
		rigidbody->setUserPointer(this);
		if (obj->rigidbodyType == v4d::scene::ObjectInstance::RigidBodyType::KINEMATIC) {
			rigidbody->setCollisionFlags(rigidbody->getCollisionFlags() | btCollisionObject::CF_KINEMATIC_OBJECT);
			rigidbody->setActivationState(DISABLE_DEACTIVATION);
		}
		globalDynamicsWorld->addRigidBody(rigidbody);
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

std::unordered_map<v4d::scene::ObjectInstance*, PhysicsObject> physicsObjects {};
std::queue<v4d::scene::ObjectInstancePtr> objectsToRefresh {};
std::queue<v4d::scene::ObjectInstancePtr> objectsToRemove {};
std::mutex objectsToRemoveMutex;

void RemovePhysicsObject(v4d::scene::ObjectInstance* obj) {
	obj->physicsObject = nullptr;
	physicsObjects[obj].RemoveRigidbody();
	physicsObjects.erase(obj);
}

void AddPhysicsObject(v4d::scene::ObjectInstancePtr obj) {
	physicsObjects[obj.get()] = obj;
	obj->physicsObject = &physicsObjects[obj.get()];
}

void UpdatePhysicsObject(v4d::scene::ObjectInstancePtr obj) {
	if (!obj->IsActive()) return;
	try {
		physicsObjects.at(obj.get());
	} catch(...) {
		LOG_ERROR("physics object does not exist for this objectInstance")
		return;
	}
	physicsObjects[obj.get()].Refresh();
}

void CleanupObjects() {
	std::lock_guard lock(objectsToRemoveMutex);
	while (!objectsToRemove.empty()) {
		RemovePhysicsObject(objectsToRemove.front().get());
		objectsToRemove.pop();
	}
}

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
		
		scene->objectInstanceRemovedCallbacks[THIS_MODULE] = [](v4d::scene::ObjectInstancePtr obj) {
			std::lock_guard lock(objectsToRemoveMutex);
			objectsToRemove.emplace(obj);
		};
	}
	
	V4D_MODULE_FUNC(void, UnloadScene) {
		while (!objectsToRefresh.empty()) {
			objectsToRefresh.pop();
		}
		CleanupObjects();
		scene->objectInstanceRemovedCallbacks.erase(THIS_MODULE);
		
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
	
	V4D_MODULE_FUNC(void, PhysicsUpdate, double deltaTime) {
		// scene->Lock();
			globalDynamicsWorld->setGravity(btVector3(scene->gravityVector.x, scene->gravityVector.y, scene->gravityVector.z));
			// Refresh info from scene objects
			bool tmpPhysicsActive;
			for (auto obj : scene->objectInstances) {
				obj->Lock();
					if ((tmpPhysicsActive = obj->IsPhysicsActive()) || obj->physicsDirty) {
						if (tmpPhysicsActive && !obj->physicsObject && obj->rigidbodyType != v4d::scene::ObjectInstance::RigidBodyType::NONE) {
							AddPhysicsObject(obj);
						}
						if (obj->physicsDirty) {
							if (obj->rigidbodyType == v4d::scene::ObjectInstance::RigidBodyType::NONE) {
								if (obj->physicsObject) {
									std::lock_guard lock(objectsToRemoveMutex);
									objectsToRemove.emplace(obj);
									obj->physicsDirty = false;
									obj->Unlock();
									continue;
								}
							} else if (tmpPhysicsActive) {
								objectsToRefresh.emplace(obj);
							}
							obj->physicsDirty = false;
						}
						if (tmpPhysicsActive && (obj->addedForce || obj->physicsForceImpulses.size() > 0) && obj->physicsObject) {
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
					}
				obj->Unlock();
			}
		// scene->Unlock();
		
		// Generate/Update collision shapes
		while (!objectsToRefresh.empty()) {
			UpdatePhysicsObject(objectsToRefresh.front());
			objectsToRefresh.pop();
		}
		CleanupObjects();
		
		// Physics Simulation
		globalDynamicsWorld->stepSimulation(deltaTime);
	}
	
	V4D_MODULE_FUNC(bool, PhysicsRayCastClosest, v4d::scene::Scene::RayCastHit* hit, glm::dvec3 origin, glm::dvec3 target, uint32_t mask) {
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
			
			if (!rayResult.hasHit())
				return false;
				
			auto* physicsObject = (PhysicsObject*)rayResult.m_collisionObject->getUserPointer();
			if (!physicsObject)
				return false;
			
			*hit = {
				physicsObject->objectInstance.lock(),
				BulletToGlm(rayResult.m_hitPointWorld),
				BulletToGlm(rayResult.m_hitNormalWorld)
			};
			return true;
		}
		return false;
	}
	
	V4D_MODULE_FUNC(int, PhysicsRayCastAll, std::vector<v4d::scene::Scene::RayCastHit>* hits, glm::dvec3 origin, glm::dvec3 target, uint32_t mask) {
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
						((PhysicsObject*)rayResult.m_collisionObjects[i]->getUserPointer())->objectInstance.lock(),
						BulletToGlm(rayResult.m_hitPointWorld[i]),
						BulletToGlm(rayResult.m_hitNormalWorld[i])
					);
				}
				return nbHits;
			}
		}
		return 0;
	}
	
	// #ifdef _DEBUG
		V4D_MODULE_FUNC(void, SecondaryRenderUpdate2, VkCommandBuffer commandBuffer) {
			if (globalDynamicsWorld && (scene->camera.debugOptions & DEBUG_OPTION_PHYSICS)) {
				globalDynamicsWorld->debugDrawWorld();
			}
		}
	// #endif
	
};
