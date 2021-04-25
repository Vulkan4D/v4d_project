#pragma once

#include <v4d.h>

#include "Entity.h"

struct V4DGAME ClientSideEntity : Entity {
	V4D_ENTITY_DECLARE_CLASS_MAP(ClientSideEntity)
	// V4D_ENTITY_DECLARE_COMPONENT_MAP(ClientSideEntity, v4d::TextID, Renderable, renderable)
	
	Iteration iteration {0};

	bool posInit = false;
	Position targetPosition {0};
	Orientation targetOrientation {1,0,0,0};
	
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
	
	
	
	// Temporary... Replace with renderable component map in the future?
			std::unordered_map<v4d::TextID, v4d::graphics::RenderableGeometryEntity::WeakPtr> renderableGeometryEntityInstances {};
			inline void UpdateRenderable() {
				for (auto&[_,renderableGeometryEntityInstance] : renderableGeometryEntityInstances) {
					if (auto renderableEntity = renderableGeometryEntityInstance.lock(); renderableEntity) {
						renderableEntity->parentId = referenceFrame;
						renderableEntity->SetLocalTransform(glm::translate(glm::dmat4(1), position) * glm::mat4_cast(orientation));
					}
				}
			}
			inline void DestroyRenderable() {
				for (auto&[_,renderableGeometryEntityInstance] : renderableGeometryEntityInstances) {
					if (auto renderableEntity = renderableGeometryEntityInstance.lock(); renderableEntity) {
						renderableEntity->Destroy();
					}
				}
			}
	
	
};

/*
struct V4DGAME RenderableGeometry {
	V4D_ENTITY_DECLARE_CLASS(RenderableGeometry)
	
	v4d::graphics::Blas blas {};
	uint64_t geometriesBuffer = 0;
	bool isRayTracedTriangles = false;
	bool isRayTracedProceduralAABB = false;
	std::vector<Geometry> geometries {};
	std::vector<v4d::graphics::vulkan::rtx::AccelerationStructure::GeometryAccelerationStructureInfo> geometriesAccelerationStructureInfo;
	
	void PrepareBlas() {
		
	}
};
*/

/*
struct RenderableEntityInstance {
	glm::mat4 modelViewTransform {1};
	glm::mat4 modelViewTransform_history {1};
	uint64_t moduleVen {0};
	uint64_t moduleId {0};
	uint64_t objId {0};
	uint64_t geometries {0};
	//TODO add instance data like colors and such
};

struct LightSource {
	glm::vec3 position;
	float radius;
	glm::vec3 color;
	float intensity;
	LightSource() {
		static_assert(sizeof(LightSource) == 32);
		Reset();
	}
	LightSource(glm::vec3 position, glm::vec3 color, float radius, float intensity) : 
		position(position),
		radius(radius),
		color(color),
		intensity(intensity)
	{}
	void Reset() {
		position = glm::vec3(0);
		radius = 0;
		color = glm::vec3(0);
		intensity = 0;
	}
};

struct Material {
	static constexpr int DEFAULT_VISIBILITY_MATERIAL_RCALL = 0;
	static constexpr int DEFAULT_SPECTRAL_EMIT_RCALL = 1;
	static constexpr int DEFAULT_SPECTRAL_ABSORB_RCALL = 2;
	static constexpr int DEFAULT_COLLIDER_RCALL = 3;
	static constexpr int DEFAULT_PHYSICS_RCALL = 4;
	static constexpr int DEFAULT_SOUND_PROP_RCALL = 5;
	static constexpr int DEFAULT_SOUND_GEN_RCALL = 6;
	
	// Visibility
	struct {
		uint32_t rcall_material = DEFAULT_VISIBILITY_MATERIAL_RCALL;
		glm::u8vec4 baseColor {255,255,255,255};
		uint8_t metallic = 0;
		uint8_t roughness = 255;
		uint8_t indexOfRefraction = (uint8_t)(1.45 * 50);
		uint8_t extra = 0;
		float emission = 0;
		uint8_t textures[8] {0};
		uint8_t texFactors[8] {0};
	} visibility {};
	
	// Spectral
	struct {
		uint32_t rcall_emit = DEFAULT_SPECTRAL_EMIT_RCALL;
		uint32_t rcall_absorb = DEFAULT_SPECTRAL_ABSORB_RCALL;
		float blackBodyRadiator = 1;
		float temperature = 0;
		uint8_t elements[8] {0};
		uint8_t elementRatios[8] {0};
	} spectral {};
	
	// Sound
	struct {
		uint32_t rcall_props = DEFAULT_SOUND_PROP_RCALL;
		uint32_t rcall_gen = DEFAULT_SOUND_GEN_RCALL;
		glm::u8vec4 props {};
		glm::u8vec4 gen {};
	} sound {};
	
	Material() {
		static_assert(sizeof(Material::visibility) == 32);
		static_assert(sizeof(Material::spectral) == 32);
		static_assert(sizeof(Material::sound) == 16);
		static_assert(sizeof(Material) == 96);
	}
};

struct Geometry {
	glm::mat4 transform {1};
	Material material {};
	std::string_view materialName {""};
	uint32_t indexCount = 0;
	uint32_t vertexCount = 0;
	uint32_t firstIndex = 0;
	uint32_t firstVertexPosition = 0;
	uint32_t firstVertexAABB = 0;
	uint32_t firstVertexNormal = 0;
	uint32_t firstVertexColorU8 = 0;
	uint32_t firstVertexColorU16 = 0;
	uint32_t firstVertexColorF32 = 0;
	uint32_t firstVertexUV = 0;
	uint32_t firstCustomData = 0;
};

struct GeometryInfo {
	glm::mat4 transform {1};
	uint64_t indices16 {};
	uint64_t indices32 {};
	uint64_t vertexPositions {};
	uint64_t vertexNormals {};
	uint64_t vertexColorsU8 {};
	uint64_t vertexColorsU16 {};
	uint64_t vertexColorsF32 {};
	uint64_t vertexUVs {};
	uint64_t customData = 0;
	uint64_t extra = 0;
	Material material {};
	GeometryInfo() {static_assert(sizeof(GeometryInfo) == 240 && (sizeof(GeometryInfo) % 16) == 0);}
};
*/
