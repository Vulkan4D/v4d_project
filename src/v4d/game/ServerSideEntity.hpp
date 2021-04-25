#pragma once

#include <v4d.h>

#include "Entity.h"
#include "Rigidbody.hpp"
#include "Collider.hpp"

struct V4DGAME ServerSideEntity : Entity {
	V4D_ENTITY_DECLARE_CLASS_MAP(ServerSideEntity)
	V4D_ENTITY_DECLARE_COMPONENT(ServerSideEntity, Rigidbody, rigidbody)
	
	bool active = false;
	bool isDynamic = false;
	
	std::atomic<Iteration> iteration {0};

	std::unordered_map<uint64_t, Joint> joints {};

	std::unordered_map<v4d::TextID, std::unique_ptr<Collider>> colliders {};
	int colliderCacheIndex = -1;
	static bool colliderCacheValid;
	
	inline Iteration Iterate() {
		return ++iteration;
	}

	inline void Activate() {
		active = true;
		Iterate();
		colliderCacheValid = false;
	}
	inline void Deactivate() {
		active = false;
		Iterate();
		colliderCacheValid = false;
	}
	inline void ChangeReferenceFrame(uint64_t referenceFrame, uint64_t referenceFrameExtra = 0) {
		this->referenceFrame = referenceFrame;
		this->referenceFrameExtra = referenceFrameExtra;
		colliderCacheValid = false;
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
