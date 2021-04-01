#pragma once

#include <v4d.h>
#include <string_view>
#include "utilities/io/Logger.h"
#include "utilities/graphics/vulkan/Loader.h"

#include "utilities/graphics/RenderableGeometryEntity.h"

struct Rigidbody {
	// Initial information
	double mass;
	glm::dmat3 inertiaMatrix;
	double restitution = 0.7;
	double friction = 0.7;
	
	// Broadphase Collision detection
	double boundingRadius = 0;
	
	// Linear physics
	double invMass;
	glm::dvec3 force {0,0,0};
	glm::dvec3 linearAcceleration {0,0,0};
	glm::dvec3 linearVelocity {0,0,0};
	glm::dvec3 position {0,0,0};
	
	// Angular physics
	glm::dmat3 invInertiaMatrix;
	glm::dvec3 torque {0,0,0};
	glm::dvec3 angularAcceleration {0,0,0};
	glm::dvec3 angularVelocity {0,0,0};
	glm::dquat orientation {1,0,0,0};
	
	#pragma region Constructor
	Rigidbody(double mass, glm::dmat3 inertiaMatrix = glm::dmat3{1})
		: mass(mass>0? mass:0)
		, inertiaMatrix(inertiaMatrix)
		, invMass(mass>0? (1.0/mass) : 0.0)
		, invInertiaMatrix(glm::inverse(inertiaMatrix))
	{}
	#pragma endregion
	
	#pragma region Inertia matrix functions
	struct Inertia {double mass; glm::dmat3 inertiaMatrix;};
	Rigidbody(Inertia&& inertia) : Rigidbody(inertia.mass, inertia.inertiaMatrix) {}
	struct SphereInertia : Inertia {
		// https://scienceworld.wolfram.com/physics/MomentofInertiaSphere.html
		SphereInertia(double mass, double radius) {
			const double I = 2.0/5 * mass * radius*radius;
			this->mass = mass;
			this->inertiaMatrix = glm::dmat3{0};
			this->inertiaMatrix[0][0] = I;
			this->inertiaMatrix[1][1] = I;
			this->inertiaMatrix[2][2] = I;
		}
	};
	struct BoxInertia : Inertia {
		// https://en.wikipedia.org/wiki/List_of_moments_of_inertia
		BoxInertia(double mass, double sizeX, double sizeY, double sizeZ) {
			const double Im = mass / 12;
			const double xx = sizeX*sizeX;
			const double yy = sizeY*sizeY;
			const double zz = sizeZ*sizeZ;
			this->mass = mass;
			this->inertiaMatrix = glm::dmat3{0};
			this->inertiaMatrix[0][0] = Im * (yy+zz);
			this->inertiaMatrix[1][1] = Im * (xx+zz);
			this->inertiaMatrix[2][2] = Im * (xx+yy);
		}
	};
	struct ConeInertia : Inertia {
		// https://scienceworld.wolfram.com/physics/MomentofInertiaCone.html
		ConeInertia(double mass, double radiusXY, double heightZ) {
			const double mr2 = mass * radiusXY*radiusXY;
			const double mh2 = mass * heightZ*heightZ;
			const double Ibase = 3.0/20 * mr2 + mh2/10;
			const double Iheight = 3.0/10 * mr2;
			this->mass = mass;
			this->inertiaMatrix = glm::dmat3{0};
			this->inertiaMatrix[0][0] = Ibase;
			this->inertiaMatrix[1][1] = Ibase;
			this->inertiaMatrix[2][2] = Iheight;
		}
	};
	struct CylinderInertia : Inertia {
		// https://scienceworld.wolfram.com/physics/MomentofInertiaCylinder.html
		CylinderInertia(double mass, double radiusXY, double heightZ) {
			const double mr2 = mass * radiusXY*radiusXY;
			const double mh2 = mass * heightZ*heightZ;
			const double Ibase = mr2/4 + mh2/12;
			const double Iheight = mr2/2;
			this->mass = mass;
			this->inertiaMatrix = glm::dmat3{0};
			this->inertiaMatrix[0][0] = Ibase;
			this->inertiaMatrix[1][1] = Ibase;
			this->inertiaMatrix[2][2] = Iheight;
		}
	};
	struct CylinderShellInertia : Inertia {
		// https://en.wikipedia.org/wiki/List_of_moments_of_inertia
		CylinderShellInertia(double mass, double innerRadiusXY, double outerRadiusXY, double heightZ) {
			const double rr = innerRadiusXY*innerRadiusXY + outerRadiusXY*outerRadiusXY;
			const double Ibase = mass/12 * (rr*3 + heightZ*heightZ);
			const double Iheight = mass/2 * rr;
			this->mass = mass;
			this->inertiaMatrix = glm::dmat3{0};
			this->inertiaMatrix[0][0] = Ibase;
			this->inertiaMatrix[1][1] = Ibase;
			this->inertiaMatrix[2][2] = Iheight;
		}
	};
	#pragma endregion
	
	#pragma region Forces & Impulses
	void ApplyForce(glm::dvec3 force, glm::dvec3 point = {0,0,0}) {
		this->force += force;
		if (point.x != 0 || point.y != 0 || point.z != 0) {
			this->torque += glm::cross(point, force);
		}
	}
	void ApplyAcceleration(glm::dvec3 acceleration) {
		this->linearAcceleration += acceleration;
	}
	void ApplyImpulse(glm::dvec3 impulse, glm::dvec3 point = {0,0,0}) {
		this->linearVelocity += this->invMass * impulse;
		if (point.x != 0 || point.y != 0 || point.z != 0) {
			this->angularVelocity += this->invInertiaMatrix * glm::cross(point, impulse);
		}
	}
	#pragma endregion
};

struct Collider {
	glm::dmat4 offset;
	glm::vec3 shape;
	enum class Type : uint8_t {
		SPHERE = 1 << 0,
		BOX = 1 << 1,
		// CONE = 1 << 2,
		// CYLINDER = 1 << 3,
		// CYLINDER_SHELL = 1 << 4,
		// TRIANGLE = 1 << 5,
		// TERRAIN = 1 << 6,
		// ? = 1 << 7,
	} type;
	
	Collider(glm::dmat4 offset, glm::vec3 shape, Type type)
	: offset(offset)
	, shape(shape)
	, type(type)
	{}
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

	Iteration Iterate() {
		return ++iteration;
	}

	void Activate() {
		active = true;
		Iterate();
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
		void UpdateRenderable() {
			if (auto renderableEntity = renderableGeometryEntityInstance.lock(); renderableEntity) {
				renderableEntity->parentId = referenceFrame;
				renderableEntity->SetLocalTransform(glm::translate(glm::dmat4(1), position) * glm::mat4_cast(orientation));
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
