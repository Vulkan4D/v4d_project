#define _V4D_MODULE
#include <v4d.h>
#include "btBulletDynamicsCommon.h"
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

struct PhysicsObject : btMotionState {
	std::weak_ptr<v4d::scene::RenderableGeometryEntity> entityInstance;
	btTransform centerOfMassOffset {};
	btRigidBody* rigidbody = nullptr;
	bool isGroupedCollisionShape = false;
	btCollisionShape* groupedCollisionShape = nullptr;
	std::vector<btCollisionShape*> localCollisionShapes {};
	
	PhysicsObject(std::shared_ptr<v4d::scene::RenderableGeometryEntity> entity = nullptr) : entityInstance(entity) {
		centerOfMassOffset.setIdentity();
	}
	
	virtual void getWorldTransform(btTransform& centerOfMassWorldTrans) const override {
		auto entity = entityInstance.lock();if(!entity || entity->GetIndex()==-1 || !entity->transform)return;
		auto transform = entity->transform.Lock();if(!transform || !transform->data)return;
		centerOfMassWorldTrans = GlmToBullet(transform->data->worldTransform) * centerOfMassOffset.inverse();
	}

	//Bullet only calls the update of worldtransform for active objects
	virtual void setWorldTransform(const btTransform& centerOfMassWorldTrans) override {
		auto entity = entityInstance.lock();if(!entity || entity->GetIndex()==-1 || !entity->transform)return;
		auto transform = entity->transform.Lock();if(!transform || !transform->data)return;
		transform->data->worldTransform = BulletToGlm(centerOfMassWorldTrans * centerOfMassOffset);
	}
	
	void Update() {
		auto entity = entityInstance.lock();if(!entity)return;
		auto physics = entity->physics.Lock();if(!physics)return;
		physics->physicsDirty = false;
		
		{// Update Center Of Mass
			centerOfMassOffset.setIdentity();
			centerOfMassOffset.setOrigin(btVector3(physics->centerOfMass.x, physics->centerOfMass.y, physics->centerOfMass.z));
		}
		
		{// Update Collision Shape
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
			
			{// calculate geometries shapes
				btCollisionShape* collisionShape = (btCollisionShape*)physics->colliderShapeObject;
				if (physics->colliderDirty) {
					physics->colliderDirty = false;
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
					if (physics && physics->colliderType != v4d::scene::PhysicsInfo::ColliderType::NONE) {
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
								
								auto generateColliderMeshFunc = [&physics, &mesh](v4d::scene::Mesh::VertexPosition* vertices){
									for (int i = 0; i < physics->colliderMeshIndices.size(); i+=3) {
										btVector3 v0(vertices[physics->colliderMeshIndices[i]].x, vertices[physics->colliderMeshIndices[i]].y, vertices[physics->colliderMeshIndices[i]].z);
										btVector3 v1(vertices[physics->colliderMeshIndices[i+1]].x, vertices[physics->colliderMeshIndices[i+1]].y, vertices[physics->colliderMeshIndices[i+1]].z);
										btVector3 v2(vertices[physics->colliderMeshIndices[i+2]].x, vertices[physics->colliderMeshIndices[i+2]].y, vertices[physics->colliderMeshIndices[i+2]].z);
										mesh->addTriangle(v0, v1, v2, true);
									}
								};
								
								if (physics->colliderMeshVertices.size() > 0){
									generateColliderMeshFunc(physics->colliderMeshVertices.data());
								} else if (entity->meshVertexPosition) {
									auto meshVertexPositions = entity->meshVertexPosition.Lock();
									if (meshVertexPositions) {
										generateColliderMeshFunc(meshVertexPositions->data);
									}
								}
								try {
									collisionShape = new btBvhTriangleMeshShape(mesh, true);
								} catch(...){
									LOG_ERROR("Exception occured in Bullet BVH generation from triangle mesh")
								}
							}break;
							case v4d::scene::PhysicsInfo::ColliderType::STATIC_PLANE:{
								auto transform = entity->transform.Lock();
								collisionShape = new btStaticPlaneShape(btVector3(0, 0, 1), transform->data->worldTransform[3].z);
							}break;
							// case v4d::scene::PhysicsInfo::ColliderType::HEIGHTFIELD:
							// 	collisionShape = 
							// break;
						}
						if (collisionShape) {
							globalCollisionShapes.push_back(collisionShape);
							localCollisionShapes.push_back(collisionShape);
						}
					}
					// Assign collision shape
					physics->colliderShapeObject = collisionShape;
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

		{// Update RigidBody
			RemoveRigidbody();
			
			if (groupedCollisionShape == nullptr) {
				LOG_ERROR("PhysicsObject has no collision shape")
				return;
			}
			
			btVector3 localInertia {0,0,0};
			btScalar mass = physics->mass;
			if (physics->rigidbodyType == v4d::scene::PhysicsInfo::RigidBodyType::DYNAMIC) {
				// groupedCollisionShape->calculateLocalInertia(mass, localInertia);
			} else {
				mass = 0;
			}
			
			btRigidBody::btRigidBodyConstructionInfo rbInfo(mass, this, groupedCollisionShape, localInertia);
			
			//TODO set additional stuff in rbInfo
			rbInfo.m_restitution = 0.7;
			
			rigidbody = new btRigidBody(rbInfo);
			rigidbody->setUserPointer(this);
			if (physics->rigidbodyType == v4d::scene::PhysicsInfo::RigidBodyType::KINEMATIC) {
				rigidbody->setCollisionFlags(rigidbody->getCollisionFlags() | btCollisionObject::CF_KINEMATIC_OBJECT);
				rigidbody->setActivationState(DISABLE_DEACTIVATION);
			}
			globalDynamicsWorld->addRigidBody(rigidbody);
		}
	}
	
	void RemoveRigidbody() {
		if (rigidbody) {
			globalDynamicsWorld->removeRigidBody(rigidbody);
			delete rigidbody;
			rigidbody = nullptr;
		}
	}
	
	~PhysicsObject() {
		RemoveRigidbody();
	}
	
};

std::unordered_map<uint32_t, PhysicsObject*> physicsObjects {};

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
			if (!e || e->GetIndex() == -1 || !e->physics || !e->transform) {
				delete physicsObj;
				it = physicsObjects.erase(it);
			} else {
				++it;
			}
		}
		
		// Loop through all physics components within active entities
		v4d::scene::RenderableGeometryEntity::physicsComponents.ForEach_LockEntities([](int32_t entityInstanceIndex, auto& physics){
			// Fetch/Add physics object
			PhysicsObject* physicsObj;
			try {
				physicsObj = physicsObjects.at(physics.uniqueId);
			} catch(...) {
				physicsObj = (physicsObjects[physics.uniqueId] = new PhysicsObject(v4d::scene::RenderableGeometryEntity::Get(entityInstanceIndex)));
			}
			
			// Update physics objects
			if (physics.physicsDirty) {
				#ifdef PHYSICS_REFRESH_COLLIDERS_OUTSIDE_OF_LOOP
					objectsToRefreshCollisionShape.push_back(physicsObj);
				#else
					physicsObj->Update();
				#endif
			}
			
			// Apply forces
			if (physics.addedForce || physics.physicsForceImpulses.size() > 0) {
				auto* rb = physicsObj->rigidbody;
				if (rb) {
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
