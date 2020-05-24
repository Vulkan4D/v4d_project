#define _V4D_MODULE
#include <v4d.h>

#include "btBulletDynamicsCommon.h"

v4d::graphics::Renderer* r = nullptr;
v4d::graphics::Scene* scene = nullptr;

V4D_Renderer* rendererModule = nullptr;

btCollisionConfiguration* collisionConfiguration = nullptr;
btDispatcher* dispatcher = nullptr;
btBroadphaseInterface* overlappingPairCache = nullptr;
btConstraintSolver* solver = nullptr;
btDynamicsWorld* dynamicsWorld = nullptr;
btAlignedObjectArray<btCollisionShape*> collisionShapes {};
btAlignedObjectArray<btTriangleMesh*> triangleMeshes {};

void RemovePhysicsObject(v4d::graphics::ObjectInstance* obj) {
	obj->LockPhysics();
		//...
		obj->physicsObject = nullptr;
		obj->removePhysicsCallback = nullptr;
	obj->UnlockPhysics();
}

void AddPhysicsObject(v4d::graphics::ObjectInstance* obj) {
	//... obj->physicsObject = 
	obj->removePhysicsCallback = RemovePhysicsObject;
}

void UpdatePhysicsObject(v4d::graphics::ObjectInstance* obj) {
	for (auto& geometryInstance : obj->GetGeometries()) {
		auto geom = geometryInstance.geometry;
		btCollisionShape* collisionShape = (btCollisionShape*)geom->colliderShapeObject;
		if (geom->colliderDirty) {
			// Delete old collision shape
			if (collisionShape) {
				for (int i = 0; i < collisionShapes.size(); ++i) {
					if (collisionShapes[i] == collisionShape) {
						collisionShapes[i] = collisionShapes[collisionShapes.size()-1];
						collisionShapes.pop_back();
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
							triangleMeshes.push_back(mesh);
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
						collisionShape = new btStaticPlaneShape(btVector3(0, 1, 0), 0);
					break;
					// case v4d::graphics::Geometry::ColliderType::HEIGHTFIELD:
					// 	collisionShape = 
					// break;
				}
				if (collisionShape) collisionShapes.push_back(collisionShape);
			}
			// Assign collision shape
			geom->colliderShapeObject = collisionShape;
			geom->colliderDirty = false;
		}
	}
	//...
	obj->physicsDirty = false;
}

extern "C" {
	
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
		collisionConfiguration = new btDefaultCollisionConfiguration();
		dispatcher = new btCollisionDispatcher(collisionConfiguration);
		overlappingPairCache = new btDbvtBroadphase();
		solver = new btSequentialImpulseConstraintSolver();
		dynamicsWorld = new btDiscreteDynamicsWorld(dispatcher, overlappingPairCache, solver, collisionConfiguration);
		
		dynamicsWorld->setGravity(btVector3(0, -10, 0));
		
		// #ifdef _DEBUG
			if (rendererModule) {
				dynamicsWorld->setDebugDrawer((btIDebugDraw*)rendererModule->ModuleGetCustomPtr(0));
				rendererModule->ModuleSetCustomPtr(0, dynamicsWorld);
			}
		// #endif
	}
	
	void UnloadScene() {
		
		for (int i = 0; i < collisionShapes.size(); i++) {
			auto* obj = collisionShapes[i];
			collisionShapes[i] = 0;
			delete obj;
		}
		collisionShapes.clear();
		
		for (int i = 0; i < triangleMeshes.size(); i++) {
			auto* obj = triangleMeshes[i];
			triangleMeshes[i] = 0;
			delete obj;
		}
		triangleMeshes.clear();
		
		delete dynamicsWorld;
		delete solver;
		delete overlappingPairCache;
		delete dispatcher;
		delete collisionConfiguration;
	}
	
	void RunUi() {
		
	}
	
	void StepSimulation(double deltaTime) {
		scene->Lock();
			for (auto* obj : scene->objectInstances) {
				obj->LockPhysics();
					if (!obj->physicsObject && obj->rigidbodyType != v4d::graphics::ObjectInstance::RigidBodyType::NONE) {
						AddPhysicsObject(obj);
					}
					if (obj->physicsDirty) {
						if (obj->rigidbodyType == v4d::graphics::ObjectInstance::RigidBodyType::NONE) {
							RemovePhysicsObject(obj);
						} else {
							UpdatePhysicsObject(obj);
						}
					}
				obj->UnlockPhysics();
			}
		scene->Unlock();
		dynamicsWorld->stepSimulation(deltaTime);
	}
	
}



		
		
		// ///create a few basic rigid bodies

		// //the ground is a cube of side 100 at position y = -56.
		// //the sphere will hit it at y = -6, with center at -5
		// {
						// 	btCollisionShape* groundShape = new btBoxShape(btVector3(btScalar(50.), btScalar(50.), btScalar(50.)));

						// 	collisionShapes.push_back(groundShape);

						// 	btTransform groundTransform;
						// 	groundTransform.setIdentity();
						// 	groundTransform.setOrigin(btVector3(0, -56, 0));

						// 	btScalar mass(0.);

						// 	//rigidbody is dynamic if and only if mass is non zero, otherwise static
						// 	bool isDynamic = (mass != 0.f);

						// 	btVector3 localInertia(0, 0, 0);
						// 	if (isDynamic)
						// 		groundShape->calculateLocalInertia(mass, localInertia);

								// 	//using motionstate is optional, it provides interpolation capabilities, and only synchronizes 'active' objects
								// 	btDefaultMotionState* myMotionState = new btDefaultMotionState(groundTransform);
								// 	btRigidBody::btRigidBodyConstructionInfo rbInfo(mass, myMotionState, groundShape, localInertia);
								// 	btRigidBody* body = new btRigidBody(rbInfo);

								// 	//add the body to the dynamics world
								// 	dynamicsWorld->addRigidBody(body);
						// }

		// {
		// 	//create a dynamic rigidbody

		// 	//btCollisionShape* colShape = new btBoxShape(btVector3(1,1,1));
		// 	btCollisionShape* colShape = new btSphereShape(btScalar(1.));
		// 	collisionShapes.push_back(colShape);

		// 	/// Create Dynamic Objects
		// 	btTransform startTransform;
		// 	startTransform.setIdentity();

		// 	btScalar mass(1.f);

		// 	//rigidbody is dynamic if and only if mass is non zero, otherwise static
		// 	bool isDynamic = (mass != 0.f);

		// 	btVector3 localInertia(0, 0, 0);
		// 	if (isDynamic)
		// 		colShape->calculateLocalInertia(mass, localInertia);

		// 	startTransform.setOrigin(btVector3(2, 10, 0));

		// 	//using motionstate is recommended, it provides interpolation capabilities, and only synchronizes 'active' objects
		// 	btDefaultMotionState* myMotionState = new btDefaultMotionState(startTransform);
		// 	btRigidBody::btRigidBodyConstructionInfo rbInfo(mass, myMotionState, colShape, localInertia);
		// 	btRigidBody* body = new btRigidBody(rbInfo);

		// 	dynamicsWorld->addRigidBody(body);
		// }

		// /// Do some simulation
		

		// ///-----stepsimulation_start-----
		// for (i = 0; i < 150; i++)
		// {
		// 	dynamicsWorld->stepSimulation(1.f / 60.f, 10);

		// 	//print positions of all objects
		// 	for (int j = dynamicsWorld->getNumCollisionObjects() - 1; j >= 0; j--)
		// 	{
		// 		btCollisionObject* obj = dynamicsWorld->getCollisionObjectArray()[j];
		// 		btRigidBody* body = btRigidBody::upcast(obj);
		// 		btTransform trans;
		// 		if (body && body->getMotionState())
		// 		{
		// 			body->getMotionState()->getWorldTransform(trans);
		// 		}
		// 		else
		// 		{
		// 			trans = obj->getWorldTransform();
		// 		}
		// 		// printf("world pos object %d = %f,%f,%f\n", j, float(trans.getOrigin().getX()), float(trans.getOrigin().getY()), float(trans.getOrigin().getZ()));
		// 	}
		// }

		// ///-----stepsimulation_end-----

		// //cleanup in the reverse order of creation/initialization

		// ///-----cleanup_start-----

		// //remove the rigidbodies from the dynamics world and delete them
		// for (i = dynamicsWorld->getNumCollisionObjects() - 1; i >= 0; i--)
		// {
		// 	btCollisionObject* obj = dynamicsWorld->getCollisionObjectArray()[i];
		// 	btRigidBody* body = btRigidBody::upcast(obj);
		// 	if (body && body->getMotionState())
		// 	{
		// 		delete body->getMotionState();
		// 	}
		// 	dynamicsWorld->removeCollisionObject(obj);
		// 	delete obj;
		// }
