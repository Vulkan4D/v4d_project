#pragma once
#include <v4d.h>
#include "Mesh.h"

using Blas = v4d::graphics::vulkan::rtx::AccelerationStructure;

class RenderableGeometryEntity {
	
	V4D_ENTITY_DECLARE_CLASS(RenderableGeometryEntity)
	V4D_ENTITY_DECLARE_COMPONENT(RenderableGeometryEntity, Mesh::DataBuffer<Mesh::ModelTransform>, transform)
	V4D_ENTITY_DECLARE_COMPONENT(RenderableGeometryEntity, Mesh::DataBuffer<Mesh::VertexPosition>, meshVertexPosition)
	V4D_ENTITY_DECLARE_COMPONENT(RenderableGeometryEntity, Mesh::DataBuffer<Mesh::VertexNormal>, meshVertexNormal)
	V4D_ENTITY_DECLARE_COMPONENT(RenderableGeometryEntity, Mesh::DataBuffer<Mesh::VertexColor>, meshVertexColor)
	V4D_ENTITY_DECLARE_COMPONENT(RenderableGeometryEntity, Mesh::DataBuffer<Mesh::VertexUV>, meshVertexUV)
	V4D_ENTITY_DECLARE_COMPONENT(RenderableGeometryEntity, Mesh::DataBuffer<Mesh::Index>, meshIndices)
	V4D_ENTITY_DECLARE_COMPONENT(RenderableGeometryEntity, Mesh::ModelInfo, buffersDeviceAddresses)
	
public:
	Device* device = nullptr;
	v4d::graphics::vulkan::rtx::AccelerationStructure::GeometryData geometryData;
	std::shared_ptr<Blas> blas = nullptr;
	bool generated = false;
	std::function<void(RenderableGeometryEntity*)> generator = [](auto*){};
	v4d::modular::ModuleID moduleId {0,0};
	uint64_t objId;
	uint64_t customData;
	
	void FreeComponentsBuffers() {
		transform.Do([this](auto& component){
			component.FreeBuffers(device);
		});
		meshVertexPosition.Do([this](auto& component){
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
	
	~RenderableGeometryEntity() {
		FreeComponentsBuffers();
	}
	
};

#ifdef V4D_HYBRID_RENDERER_MODULE
	V4D_ENTITY_DEFINE_CLASS(RenderableGeometryEntity)
	V4D_ENTITY_DEFINE_COMPONENT(RenderableGeometryEntity, Mesh::DataBuffer<Mesh::ModelTransform>, transform);
	V4D_ENTITY_DEFINE_COMPONENT(RenderableGeometryEntity, Mesh::DataBuffer<Mesh::VertexPosition>, meshVertexPosition)
	V4D_ENTITY_DEFINE_COMPONENT(RenderableGeometryEntity, Mesh::DataBuffer<Mesh::VertexNormal>, meshVertexNormal)
	V4D_ENTITY_DEFINE_COMPONENT(RenderableGeometryEntity, Mesh::DataBuffer<Mesh::VertexColor>, meshVertexColor)
	V4D_ENTITY_DEFINE_COMPONENT(RenderableGeometryEntity, Mesh::DataBuffer<Mesh::VertexUV>, meshVertexUV)
	V4D_ENTITY_DEFINE_COMPONENT(RenderableGeometryEntity, Mesh::DataBuffer<Mesh::Index>, meshIndices)
	V4D_ENTITY_DEFINE_COMPONENT(RenderableGeometryEntity, Mesh::ModelInfo, buffersDeviceAddresses)
#endif
