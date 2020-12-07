#pragma once
#include <v4d.h>
#include "Mesh.h"

using Blas = v4d::graphics::vulkan::rtx::AccelerationStructure;

class RenderableGeometryEntity {
public:
	struct LightSource {
		glm::vec3 position;
		float radius;
		glm::vec3 color;
		float reach;
		LightSource() {
			static_assert(sizeof(LightSource) == 32);
			Reset();
		}
		LightSource(glm::vec3 position, glm::vec3 color, float radius, float reach) : 
			position(position),
			radius(radius),
			color(color),
			reach(reach)
		 {}
		void Reset() {
			position = glm::vec3(0);
			radius = 0;
			color = glm::vec3(0);
			reach = 0;
		}
	};
private:
	V4D_ENTITY_DECLARE_CLASS(RenderableGeometryEntity)
	V4D_ENTITY_DECLARE_COMPONENT(RenderableGeometryEntity, Mesh::DataBuffer<Mesh::ModelTransform>, transform)
	V4D_ENTITY_DECLARE_COMPONENT(RenderableGeometryEntity, Mesh::DataBuffer<Mesh::ProceduralVertexAABB>, proceduralVertexAABB)
	V4D_ENTITY_DECLARE_COMPONENT(RenderableGeometryEntity, Mesh::DataBuffer<Mesh::VertexPosition>, meshVertexPosition)
	V4D_ENTITY_DECLARE_COMPONENT(RenderableGeometryEntity, Mesh::DataBuffer<Mesh::VertexNormal>, meshVertexNormal)
	V4D_ENTITY_DECLARE_COMPONENT(RenderableGeometryEntity, Mesh::DataBuffer<Mesh::VertexColor>, meshVertexColor)
	V4D_ENTITY_DECLARE_COMPONENT(RenderableGeometryEntity, Mesh::DataBuffer<Mesh::VertexUV>, meshVertexUV)
	V4D_ENTITY_DECLARE_COMPONENT(RenderableGeometryEntity, Mesh::DataBuffer<Mesh::Index>, meshIndices)
	V4D_ENTITY_DECLARE_COMPONENT(RenderableGeometryEntity, RenderableGeometryEntity::LightSource, lightSource)
	
public:
	Device* device = nullptr;
	v4d::graphics::vulkan::rtx::AccelerationStructure::GeometryData geometryData;
	std::shared_ptr<Blas> blas = nullptr;
	bool generated = false;
	std::function<void(RenderableGeometryEntity*)> generator = [](auto*){};
	v4d::modular::ModuleID moduleId {0,0};
	uint64_t objId;
	uint64_t customData;
	
	uint32_t sbtOffset = 0;
	uint32_t rayTracingMask = 0xff;
	VkGeometryInstanceFlagsKHR rayTracingFlags = 0;
	
	static std::unordered_map<std::string, uint32_t> sbtOffsets;
	
	void FreeComponentsBuffers() {
		transform.Do([this](auto& component){
			component.FreeBuffers(device);
		});
		meshVertexPosition.Do([this](auto& component){
			component.FreeBuffers(device);
		});
		proceduralVertexAABB.Do([this](auto& component){
			component.FreeBuffers(device);
		});
		meshVertexNormal.Do([this](auto& component){
			component.FreeBuffers(device);
		});
		meshVertexColor.Do([this](auto& component){
			component.FreeBuffers(device);
		});
		meshVertexUV.Do([this](auto& component){
			component.FreeBuffers(device);
		});
		meshIndices.Do([this](auto& component){
			component.FreeBuffers(device);
		});
		generated = false;
		blas = nullptr;
	}
	
	void operator()(v4d::modular::ModuleID moduleId, int objId, int customData) {
		this->moduleId = moduleId;
		this->objId = objId;
		this->customData = customData;
	}
	
	void Prepare(Device* renderingDevice, glm::dmat4 worldTransform, std::string sbtOffset = "default", uint32_t rayTracingMask = 0xff, VkGeometryInstanceFlagsKHR rayTracingFlags = 0) {
		this->device = renderingDevice;
		Add_transform();
		transform->AllocateBuffers(renderingDevice);
		transform->data->worldTransform = worldTransform;
		this->sbtOffset = sbtOffsets[sbtOffset];
		this->rayTracingMask = rayTracingMask;
		this->rayTracingFlags = rayTracingFlags;
	}
	
	~RenderableGeometryEntity() {
		FreeComponentsBuffers();
		generator = [](auto*){};
	}
	
};

#ifdef V4D_HYBRID_RENDERER_MODULE
	V4D_ENTITY_DEFINE_CLASS(RenderableGeometryEntity)
	V4D_ENTITY_DEFINE_COMPONENT(RenderableGeometryEntity, Mesh::DataBuffer<Mesh::ModelTransform>, transform);
	V4D_ENTITY_DEFINE_COMPONENT(RenderableGeometryEntity, Mesh::DataBuffer<Mesh::ProceduralVertexAABB>, proceduralVertexAABB)
	V4D_ENTITY_DEFINE_COMPONENT(RenderableGeometryEntity, Mesh::DataBuffer<Mesh::VertexPosition>, meshVertexPosition)
	V4D_ENTITY_DEFINE_COMPONENT(RenderableGeometryEntity, Mesh::DataBuffer<Mesh::VertexNormal>, meshVertexNormal)
	V4D_ENTITY_DEFINE_COMPONENT(RenderableGeometryEntity, Mesh::DataBuffer<Mesh::VertexColor>, meshVertexColor)
	V4D_ENTITY_DEFINE_COMPONENT(RenderableGeometryEntity, Mesh::DataBuffer<Mesh::VertexUV>, meshVertexUV)
	V4D_ENTITY_DEFINE_COMPONENT(RenderableGeometryEntity, Mesh::DataBuffer<Mesh::Index>, meshIndices)
	V4D_ENTITY_DEFINE_COMPONENT(RenderableGeometryEntity, RenderableGeometryEntity::LightSource, lightSource)
	std::unordered_map<std::string, uint32_t> RenderableGeometryEntity::sbtOffsets {};
#endif
