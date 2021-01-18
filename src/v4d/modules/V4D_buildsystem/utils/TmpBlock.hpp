#pragma once

#include <v4d.h>

class TmpBlock {
	std::shared_ptr<v4d::graphics::RenderableGeometryEntity> entity = nullptr;
	mutable std::recursive_mutex blocksMutex;
	
public:
	Block block;
	float boundingDistance = 1.0;
	glm::vec4 wireframeColor {0.5f, 0.5f, 0.5f, 0.5f};

	TmpBlock(Block b) : block(b) {
		std::lock_guard lock(blocksMutex);
		entity = RenderableGeometryEntity::Create(THIS_MODULE);
		entity->generator = [this](RenderableGeometryEntity* entity, Device* device){
			std::lock_guard lock(blocksMutex);
			if (entity->GetIndex() == -1) {
				entity->generated = false;
				LOG_ERROR("Entity generator executed for destroyed entity")
				return;
			}
			entity->Allocate(device);
			entity->rayTracingMask = 0;
			entity->raster_transparent = true;
			entity->raster_wireframe = 5.0f;
			entity->raster_wireframe_color = wireframeColor;
			
			std::vector<Mesh::Index32> meshIndices (Block::MAX_INDICES);
			std::vector<Mesh::VertexPosition> vertexPositions (Block::MAX_VERTICES);
			std::vector<Mesh::VertexNormal> vertexNormals (Block::MAX_VERTICES);
			std::vector<Mesh::VertexColor<uint8_t>> vertexColors (Block::MAX_VERTICES);
			std::vector<uint32_t> customData (Block::MAX_VERTICES);
			
			auto[vertexCount, indexCount] = block.GenerateSimpleGeometry(meshIndices.data(), vertexPositions.data(), vertexNormals.data(), vertexColors.data(), customData.data(), 0, 0.3);
			
			boundingDistance = 1;
			for (int i = 0; i < vertexCount; ++i) {
				auto vert = vertexPositions[i];
				boundingDistance = glm::max(boundingDistance, glm::length(glm::vec3(vert)));
			}
			
			entity->Add_meshIndices32()->AllocateBuffersFromArray(device, meshIndices.data(), indexCount); 
			entity->Add_meshVertexPosition()->AllocateBuffersFromArray(device, vertexPositions.data(), vertexCount);
			entity->Add_meshVertexNormal()->AllocateBuffersFromArray(device, vertexNormals.data(), vertexCount);
			entity->Add_meshVertexColorU8()->AllocateBuffersFromArray(device, vertexColors.data(), vertexCount);
			entity->Add_meshCustomData()->AllocateBuffersFromArray(device, (float*)customData.data(), vertexCount);
		};
	}
	
	~TmpBlock() {
		std::lock_guard lock(blocksMutex);
		if (entity) {
			entity->Destroy();
		}
	}
	
	void SetWorldTransform(glm::dmat4 t) {
		entity->SetWorldTransform(t);
	}
	
	glm::dmat4 GetWorldTransform() const {
		return entity->GetWorldTransform();
	}
	
};
