#pragma once

#include <v4d.h>
#include <string_view>
#include "utilities/io/Logger.h"
#include "utilities/data/Stream.h"
#include "utilities/graphics/vulkan/Loader.h"

#include "utilities/graphics/RenderableGeometryEntity.h"

struct Transform {
	glm::dvec3 position;
	glm::dquat orientation;
};

struct Joint {
	v4d::TextID parentJoint;
	glm::vec3 position {0,0,0};
	glm::quat orientation {1,0,0,0};
	std::unordered_map<v4d::TextID, Transform> parts {};
	std::vector<v4d::TextID> childJoints {};
	
	glm::vec3 linearValue {0,0,0};
	glm::vec3 angularValue {0,0,0};
	glm::vec3 linearMin {0,0,0};
	glm::vec3 angularMin {0,0,0};
	glm::vec3 linearMax {0,0,0};
	glm::vec3 angularMax {0,0,0};
	
	virtual void Serialize(v4d::data::Stream& stream) {
		stream << linearValue;
		stream << angularValue;
	}
	virtual void Deserialize(v4d::data::Stream& stream) {
		stream >> linearValue;
		stream >> angularValue;
	}
	virtual ~Joint() = default;
};


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
