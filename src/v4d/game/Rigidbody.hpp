#pragma once

#include <v4d.h>

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
	bool atRest = false;
	
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
	struct RingInertia : Inertia {
		// https://en.wikipedia.org/wiki/List_of_moments_of_inertia
		RingInertia(float mass, float innerRadiusXY, float outerRadiusXY, float heightZ) {
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
	//TODO add verification to ignore the force if not strong enough when atRest (static friction)
	void ApplyForce(glm::dvec3 force, glm::dvec3 point = {0,0,0}) {
		this->force += force;
		if (!isOrientationLocked && (point.x != 0 || point.y != 0 || point.z != 0)) {
			this->torque += glm::cross(point, force);
		}
		this->atRest = false;
	}
	void ApplyAcceleration(glm::dvec3 acceleration) {
		this->linearAcceleration += acceleration;
		this->atRest = false;
	}
	void ApplyImpulse(glm::dvec3 impulse, glm::dvec3 point = {0,0,0}) {
		this->linearVelocity += this->invMass * impulse;
		if (!isOrientationLocked && (point.x != 0 || point.y != 0 || point.z != 0)) {
			this->angularVelocity += this->invInertiaTensorWorld * glm::cross(point, impulse);
		}
		this->atRest = false;
	}
	#pragma endregion
};
