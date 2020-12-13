#pragma once

#include <v4d.h>

class TmpBlock {
	std::shared_ptr<v4d::graphics::RenderableGeometryEntity> entity = nullptr;
	
public:
	Block block;
	float boundingDistance = 1.0;
	glm::vec4 wireframeColor {0.5f, 0.5f, 0.5f, 0.5f};

	TmpBlock(Block b) : block(b) {
		entity = RenderableGeometryEntity::Create(THIS_MODULE);
		entity->generator = [this](RenderableGeometryEntity* entity, Device* device){
			entity->Prepare(device, "V4D_buildsystem.block");
			entity->rayTracingMask = 0;
			
			entity->Add_meshIndices();
			entity->Add_meshVertexPosition();
			entity->Add_meshVertexNormal();
			entity->Add_meshVertexColor();
			entity->Add_customData();
			
			std::vector<Mesh::Index> meshIndices (Block::MAX_INDICES);
			std::vector<Mesh::VertexPosition> vertexPositions (Block::MAX_VERTICES);
			std::vector<Mesh::VertexNormal> vertexNormals (Block::MAX_VERTICES);
			std::vector<Mesh::VertexColor> vertexColors (Block::MAX_VERTICES);
			std::vector<uint32_t> customData (Block::MAX_VERTICES);
			
			auto[vertexCount, indexCount] = block.GenerateSimpleGeometry(meshIndices.data(), vertexPositions.data(), vertexNormals.data(), vertexColors.data(), customData.data(), 0, 0.3);
			
			for (int i = 0; i < vertexCount; ++i) {
				auto vert = vertexPositions[i];
				boundingDistance = glm::max(boundingDistance, glm::length(glm::vec3(vert)));
			}
			
			entity->meshIndices->AllocateBuffers(device, meshIndices.data(), indexCount);
			entity->meshVertexPosition->AllocateBuffers(device, vertexPositions.data(), vertexCount);
			entity->meshVertexNormal->AllocateBuffers(device, vertexNormals.data(), vertexCount);
			entity->meshVertexColor->AllocateBuffers(device, vertexColors.data(), vertexCount);
			entity->customData->AllocateBuffers(device, (float*)customData.data(), vertexCount);
			
			entity->raster_transparent = true;
			entity->raster_wireframe = 5.0f;
			entity->raster_wireframe_color = wireframeColor;
		};
	}
	
	~TmpBlock() {
		if (entity) entity->Destroy();
	}
	
	void SetWorldTransform(glm::dmat4 t) {
		entity->SetWorldTransform(t);
	}
	
	glm::dmat4 GetWorldTransform() const {
		return entity->GetWorldTransform();
	}
	
};
