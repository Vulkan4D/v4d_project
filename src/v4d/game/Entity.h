#pragma once

#include <v4d.h>
#include <string_view>
#include "utilities/io/Logger.h"
#include "utilities/graphics/vulkan/Loader.h"

#include "utilities/graphics/RenderableGeometryEntity.h"

struct Rigidbody {
	// Initial information
	float mass;
	glm::vec3 localInertiaVector;
	float restitution = 0.7f;
	float friction = 0.7f;
	
	// Broadphase Collision detection
	double boundingRadius = 0;
	
	// Linear physics
	double invMass;
	glm::dvec3 force {0,0,0};
	glm::dvec3 linearAcceleration {0,0,0};
	glm::dvec3 linearVelocity {0,0,0};
	glm::dvec3 position {0,0,0};
	
	// Angular physics
	glm::dmat3 invInertiaTensorWorld;
	glm::dvec3 torque {0,0,0};
	glm::dvec3 angularAcceleration {0,0,0};
	glm::dvec3 angularVelocity {0,0,0};
	glm::dquat orientation {1,0,0,0};
	
	bool isInitialized = false;
	bool isKinematic = false;
	bool isOrientationLocked = false;
	
	inline void SetKinematic(bool kinematic = true) {
		isKinematic = kinematic;
	}
	inline bool IsKinematic() const {
		return isKinematic;
	}
	inline void SetInitialized(bool initialized = true) {
		isInitialized = initialized;
	}
	inline bool IsInitialized() const {
		return isInitialized;
	}
	inline void SetOrientationLocked(bool locked = true) {
		isOrientationLocked = locked;
	}
	inline bool IsOrientationLocked() const {
		return isOrientationLocked;
	}
	
	#pragma region Constructor
	Rigidbody(float mass, glm::vec3 localInertiaVector = glm::vec3{1})
		: mass(mass>0? mass:0)
		, localInertiaVector(localInertiaVector)
		, invMass(mass>0? (1.0/mass) : 0.0)
	{
		ComputeInvInertiaTensorWorld();
	}
	#pragma endregion
	
	void ComputeInvInertiaTensorWorld() {
		glm::dvec3 invInertiaVector = localInertiaVector;
			if (invInertiaVector.x != 0) invInertiaVector.x = 1.0 / invInertiaVector.x;
			if (invInertiaVector.y != 0) invInertiaVector.y = 1.0 / invInertiaVector.y;
			if (invInertiaVector.z != 0) invInertiaVector.z = 1.0 / invInertiaVector.z;
		
		glm::dmat3 rotationMatrix = glm::mat3_cast(orientation);
		glm::dmat3 invInertiaMatrix = glm::transpose(rotationMatrix);
			invInertiaMatrix[0].x *= invInertiaVector.x;
			invInertiaMatrix[0].y *= invInertiaVector.y;
			invInertiaMatrix[0].z *= invInertiaVector.z;
			invInertiaMatrix[1].x *= invInertiaVector.x;
			invInertiaMatrix[1].y *= invInertiaVector.y;
			invInertiaMatrix[1].z *= invInertiaVector.z;
			invInertiaMatrix[2].x *= invInertiaVector.x;
			invInertiaMatrix[2].y *= invInertiaVector.y;
			invInertiaMatrix[2].z *= invInertiaVector.z;
		
		invInertiaTensorWorld = invInertiaMatrix * rotationMatrix;
	}
	
	#pragma region Inertia matrix functions
	struct Inertia {float mass; glm::vec3 localInertiaVector;};
	Rigidbody(Inertia&& inertia) : Rigidbody(inertia.mass, inertia.localInertiaVector) {}
	struct SphereInertia : Inertia {
		// https://scienceworld.wolfram.com/physics/MomentofInertiaSphere.html
		SphereInertia(float mass, float radius) {
			const float I = 2.0f/5 * mass * radius*radius;
			this->mass = mass;
			this->localInertiaVector.x = I;
			this->localInertiaVector.y = I;
			this->localInertiaVector.z = I;
		}
	};
	struct BoxInertia : Inertia {
		// https://en.wikipedia.org/wiki/List_of_moments_of_inertia
		BoxInertia(float mass, float sizeX, float sizeY, float sizeZ) {
			const float Im = mass / 12;
			const float xx = sizeX*sizeX;
			const float yy = sizeY*sizeY;
			const float zz = sizeZ*sizeZ;
			this->mass = mass;
			this->localInertiaVector.x = Im * (yy+zz);
			this->localInertiaVector.y = Im * (xx+zz);
			this->localInertiaVector.z = Im * (xx+yy);
		}
	};
	struct ConeInertia : Inertia {
		// https://scienceworld.wolfram.com/physics/MomentofInertiaCone.html
		ConeInertia(float mass, float radiusXY, float heightZ) {
			const float mr2 = mass * radiusXY*radiusXY;
			const float mh2 = mass * heightZ*heightZ;
			const float Ibase = 3.0f/20 * mr2 + mh2/10;
			const float Iheight = 3.0f/10 * mr2;
			this->mass = mass;
			this->localInertiaVector.x = Ibase;
			this->localInertiaVector.y = Ibase;
			this->localInertiaVector.z = Iheight;
		}
	};
	struct CylinderInertia : Inertia {
		// https://scienceworld.wolfram.com/physics/MomentofInertiaCylinder.html
		CylinderInertia(float mass, float radiusXY, float heightZ) {
			const float mr2 = mass * radiusXY*radiusXY;
			const float mh2 = mass * heightZ*heightZ;
			const float Ibase = mr2/4 + mh2/12;
			const float Iheight = mr2/2;
			this->mass = mass;
			this->localInertiaVector.x = Ibase;
			this->localInertiaVector.y = Ibase;
			this->localInertiaVector.z = Iheight;
		}
	};
	struct CylinderShellInertia : Inertia {
		// https://en.wikipedia.org/wiki/List_of_moments_of_inertia
		CylinderShellInertia(float mass, float innerRadiusXY, float outerRadiusXY, float heightZ) {
			const float rr = innerRadiusXY*innerRadiusXY + outerRadiusXY*outerRadiusXY;
			const float Ibase = mass/12 * (rr*3 + heightZ*heightZ);
			const float Iheight = mass/2 * rr;
			this->mass = mass;
			this->localInertiaVector.x = Ibase;
			this->localInertiaVector.y = Ibase;
			this->localInertiaVector.z = Iheight;
		}
	};
	#pragma endregion
	
	#pragma region Forces & Impulses
	void ApplyForce(glm::dvec3 force, glm::dvec3 point = {0,0,0}) {
		this->force += force;
		if (!isOrientationLocked && (point.x != 0 || point.y != 0 || point.z != 0)) {
			this->torque += glm::cross(point, force);
		}
	}
	void ApplyAcceleration(glm::dvec3 acceleration) {
		this->linearAcceleration += acceleration;
	}
	void ApplyImpulse(glm::dvec3 impulse, glm::dvec3 point = {0,0,0}) {
		this->linearVelocity += this->invMass * impulse;
		if (!isOrientationLocked && (point.x != 0 || point.y != 0 || point.z != 0)) {
			this->angularVelocity += this->invInertiaTensorWorld * glm::cross(point, impulse);
		}
	}
	#pragma endregion
};

struct Collider {
	enum class Type : uint8_t {
		SPHERE = 1 << 0,
		TRIANGLE = 1 << 1,
		BOX = 1 << 2,
		CYLINDER = 1 << 3,
		RING = 1 << 4,
	} type;

	glm::dvec3 position;
	glm::dmat3 rotation;
	
	union {
		struct {
			float radius;
		} sphere;
		struct {
			float vertex1_y;
			glm::vec2 vertex2;
			glm::vec3 center;
		} triangle; // triangle should be oriented with line 0-1 towards +Y, and 0-2 towards +X+Y. Up is +Z, triangle lays flat on Z=0. Center is the position of the center of the triangle mesh (relative to collider.position or vertex0)
		struct {
			glm::vec3 halfSize;
		} box;
		struct {
			float length;
			float radius;
		} cylinder;
		struct {
			float length;
			float innerRadius;
			float outerRadius;
		} ring;
	};
	
	// Sphere at center
	Collider(float radius)
	: type(Type::SPHERE)
	, position(0,0,0)
	, rotation(1)
	{
		sphere.radius = radius;
	}
	
	// Sphere
	Collider(glm::dvec3 position, float radius)
	: type(Type::SPHERE)
	, position(position)
	, rotation(1)
	{
		sphere.radius = radius;
	}
	
	// Triangle
	Collider(glm::dvec3 position, glm::dmat3 rotation, float vertex1_y, glm::vec2 vertex2_xy, glm::vec3 center)
	: type(Type::TRIANGLE)
	, position(position)
	, rotation(rotation)
	{
		triangle.vertex1_y = vertex1_y;
		triangle.vertex2 = vertex2_xy;
		triangle.center = center;
	}
	Collider(glm::dvec3 vertex0, glm::dvec3 vertex1, glm::dvec3 vertex2, glm::dvec3 center)
	: type(Type::TRIANGLE)
	, position(vertex0)
	, rotation(1)
	{
		const glm::dvec3 v1 = vertex1 - vertex0;
		const glm::dvec3 v2 = vertex2 - vertex0;
		rotation = glm::mat3_cast(glm::quatLookAt(glm::normalize(glm::cross(glm::normalize(v1), glm::normalize(v2))), glm::normalize(v1)));
		triangle.vertex1_y = float((glm::inverse(rotation) * v1).y);
		triangle.vertex2 = glm::inverse(rotation) * v2;
		triangle.center = glm::inverse(rotation) * (center - vertex0);
	}
	
	// Box
	Collider(glm::dvec3 position, glm::dmat3 rotation, glm::vec3 halfSize)
	: type(Type::BOX)
	, position(position)
	, rotation(rotation)
	{
		box.halfSize = halfSize;
	}
	
	// Cylinder
	Collider(glm::dvec3 position, glm::dmat3 rotation, float length, float radius)
	: type(Type::CYLINDER)
	, position(position)
	, rotation(rotation)
	{
		cylinder.length = length;
		cylinder.radius = radius;
	}
	
	// Ring
	Collider(glm::dvec3 position, glm::dmat3 rotation, float length, float innerRadius, float outerRadius)
	: type(Type::RING)
	, position(position)
	, rotation(rotation)
	{
		ring.length = length;
		ring.innerRadius = innerRadius;
		ring.outerRadius = outerRadius;
	}
	
};

// struct Renderable {
// 	uint32_t id; //TODO static increment like physicsInfo
// 	glm::dmat4 offset;
// 	// geometries, ....
// 	glm::dmat4 worldTransform;
// 	glm::mat4 modelViewTransform;
// };



	// void NetworkGameObject::SetAttributes(Attributes attrs) {
	// 	for (size_t i = 0; i < attributesPtrs.size(); ++i) {
	// 		*attributesPtrs[i] = attrs & (1 << i);
	// 	}
	// }
	// NetworkGameObject::Attributes NetworkGameObject::GetAttributes() const {
	// 	Attributes attrs = 0;
	// 	for (size_t i = 0; i < attributesPtrs.size(); ++i) {
	// 		attrs |= (*attributesPtrs[i]? 1:0) << i;
	// 	}
	// 	return attrs;
	// }

	
	// bool NetworkGameObject::ReverseUpdateGameObjectTransform() {
	// 	std::lock_guard lock(mu);
	// 	if (auto entity = renderableGeometryEntityInstance.lock(); entity) {
	// 		const auto& t = entity->GetLocalTransform();
	// 		targetPosition = t[3];
	// 		targetOrientation = glm::quat_cast(t);
	// 		if (position != targetPosition || orientation != targetOrientation) {
	// 			position = targetPosition;
	// 			orientation = targetOrientation;
	// 			return true;
	// 		}
	// 	}
	// 	return false;
	// }


struct Entity {
	using Id = v4d::ECS::EntityIndex_T;
	using Type = uint64_t;
	
	using ReferenceFrame = uint64_t;
	using ReferenceFrameExtra = uint64_t;
	using Position = glm::dvec3;
	using Orientation = glm::dquat;
	
	using Iteration = uint64_t;
	
	Entity() {};
	
	v4d::modular::ModuleID moduleID {0,0};
	Type type {0};
	
	ReferenceFrame referenceFrame {0};
	ReferenceFrameExtra referenceFrameExtra {0};
	Position position {0};
	Orientation orientation {1,0,0,0};
	
	inline void operator()(v4d::modular::ModuleID moduleID) {
		this->moduleID = moduleID;
	}
	inline void operator()(v4d::modular::ModuleID moduleID, Type type) {
		operator()(moduleID);
		this->type = type;
	}

	inline void operator()(v4d::modular::ModuleID moduleID, Type type, ReferenceFrame referenceFrame) {
		operator()(moduleID, type);
		this->referenceFrame = referenceFrame;
	}
	inline void operator()(v4d::modular::ModuleID moduleID, Type type, ReferenceFrame referenceFrame, ReferenceFrameExtra referenceFrameExtra) {
		operator()(moduleID, type, referenceFrame);
		this->referenceFrameExtra = referenceFrameExtra;
	}
	inline void operator()(v4d::modular::ModuleID moduleID, Type type, ReferenceFrame referenceFrame, Position position, Orientation orientation = {1,0,0,0}) {
		operator()(moduleID, type, referenceFrame);
		this->position = position;
		this->orientation = orientation;
	}
	inline void operator()(v4d::modular::ModuleID moduleID, Type type, ReferenceFrame referenceFrame, ReferenceFrameExtra referenceFrameExtra, Position position, Orientation orientation = {1,0,0,0}) {
		operator()(moduleID, type, referenceFrame, referenceFrameExtra);
		this->position = position;
		this->orientation = orientation;
	}
};

struct V4DGAME ServerSideEntity : Entity {
	V4D_ENTITY_DECLARE_CLASS_MAP(ServerSideEntity)
	V4D_ENTITY_DECLARE_COMPONENT(ServerSideEntity, Rigidbody, rigidbody)
	
	bool active = false;
	bool isDynamic = false;
	
	std::atomic<Iteration> iteration {0};
	std::unordered_map<uint64_t/*clientID*/, Iteration /*iteration*/> clientIterations {};

	std::vector<Collider> colliders {};

	inline Iteration Iterate() {
		return ++iteration;
	}

	inline void Activate() {
		active = true;
		Iterate();
	}
	inline void Deactivate() {
		active = false;
		Iterate();
	}
	inline bool IsActive() const {
		return active;
	}
	inline bool IsDynamic() const {
		return isDynamic;
	}
	inline void SetDynamic(bool dynamic = true) {
		isDynamic = dynamic;
	}
};

struct V4DGAME ClientSideEntity : Entity {
	V4D_ENTITY_DECLARE_CLASS_MAP(ClientSideEntity)
	// V4D_ENTITY_DECLARE_COMPONENT_MAP(ClientSideEntity, std::string_view, Renderable, renderable)
	
	Iteration iteration {0};

	bool posInit = false;
	Position targetPosition {0};
	Orientation targetOrientation {1,0,0,0};
	
	// Temporary
		v4d::graphics::RenderableGeometryEntity::WeakPtr renderableGeometryEntityInstance;
		inline void UpdateRenderable() {
			if (auto renderableEntity = renderableGeometryEntityInstance.lock(); renderableEntity) {
				renderableEntity->parentId = referenceFrame;
				renderableEntity->SetLocalTransform(glm::translate(glm::dmat4(1), position) * glm::mat4_cast(orientation));
			}
		}
		inline void DestroyRenderable() {
			if (auto renderableEntity = renderableGeometryEntityInstance.lock(); renderableEntity) {
				renderableEntity->Destroy();
			}
		}
	
	inline void operator()(v4d::modular::ModuleID moduleID, Type type, ReferenceFrame referenceFrame, Position position, Orientation orientation = {1,0,0,0}) {
		Entity::operator()(moduleID, type, referenceFrame, position, orientation);
		this->targetPosition = position;
		this->targetOrientation = orientation;
		this->posInit = true;
	}
	inline void operator()(v4d::modular::ModuleID moduleID, Type type, ReferenceFrame referenceFrame, ReferenceFrameExtra referenceFrameExtra, Position position, Orientation orientation = {1,0,0,0}) {
		Entity::operator()(moduleID, type, referenceFrame, referenceFrameExtra, position, orientation);
		this->targetPosition = position;
		this->targetOrientation = orientation;
		this->posInit = true;
	}
	inline void operator()(v4d::modular::ModuleID moduleID, Type type, ReferenceFrame referenceFrame, ReferenceFrameExtra referenceFrameExtra, Position position, Orientation orientation, Iteration iteration) {
		operator()(moduleID, type, referenceFrame, referenceFrameExtra, position, orientation);
		this->iteration = iteration;
	}
};

struct V4DGAME ServerSidePlayer {
	V4D_ENTITY_DECLARE_CLASS_MAP(ServerSidePlayer)
	// GetID() is client_id
	Entity::ReferenceFrame referenceFrame {0};
	Entity::ReferenceFrameExtra referenceFrameExtra {0};
	Entity::Id parentEntityId {-1};
	
	ServerSideEntity::Ptr GetServerSideEntity() const {
		if (parentEntityId < 0) return nullptr;
		return ServerSideEntity::Get(parentEntityId);
	}
	
};
