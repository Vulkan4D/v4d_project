#pragma once

class Build {
	std::shared_ptr<v4d::graphics::RenderableGeometryEntity> entity = nullptr;
	std::vector<Block> blocks {};
	mutable std::recursive_mutex blocksMutex;
	
public:
	v4d::scene::NetworkGameObject::Id networkId;

	Build(v4d::scene::NetworkGameObject::Id networkId) : networkId(networkId) {}
	
	~Build() {
		if (entity) entity->Destroy();
		std::lock_guard lock(blocksMutex);
		blocks.clear();
	}
	
	std::shared_ptr<v4d::graphics::RenderableGeometryEntity> CreateEntity() {
		std::lock_guard lock(blocksMutex);
		if (entity) {
			entity->Destroy();
			entity = nullptr;
		}
		if (blocks.size() > 0) {
			entity = RenderableGeometryEntity::Create(THIS_MODULE, networkId);
			entity->Add_physics(v4d::scene::PhysicsInfo::RigidBodyType::STATIC, 1.0f);
			entity->generator = [this](RenderableGeometryEntity* entity, Device* device){
				std::lock_guard lock(blocksMutex);
				RenderableGeometryEntity::Material material {};
				material.visibility.roughness = 0;
				material.visibility.metallic = 1;
				entity->Allocate(device, "V4D_buildsystem:block")->material = material;
				
				std::vector<Mesh::Index32> meshIndices (Block::MAX_INDICES * blocks.size());
				std::vector<Mesh::VertexPosition> vertexPositions (Block::MAX_VERTICES * blocks.size());
				std::vector<Mesh::VertexNormal> vertexNormals (Block::MAX_VERTICES * blocks.size());
				std::vector<Mesh::VertexColor<uint8_t>> vertexColors (Block::MAX_VERTICES * blocks.size());
				std::vector<uint32_t> customData (Block::MAX_VERTICES * blocks.size());
				
				uint nextVertex = 0;
				uint nextIndex = 0;
				for (int i = 0; i < blocks.size(); ++i) {
					auto[vertexCount, indexCount] = blocks[i].GenerateGeometry(&meshIndices.data()[nextIndex], &vertexPositions.data()[nextVertex], &vertexNormals.data()[nextVertex], &vertexColors.data()[nextVertex], &customData.data()[nextVertex], nextVertex);
					nextVertex += vertexCount;
					nextIndex += indexCount;
				}
				
				entity->Add_meshIndices32()->AllocateBuffersFromArray(device, meshIndices.data(), nextIndex);
				entity->Add_meshVertexPosition()->AllocateBuffersFromArray(device, vertexPositions.data(), nextVertex);
				entity->Add_meshVertexNormal()->AllocateBuffersFromArray(device, vertexNormals.data(), nextVertex);
				entity->Add_meshVertexColorU8()->AllocateBuffersFromArray(device, vertexColors.data(), nextVertex);
				entity->Add_meshCustomData()->AllocateBuffersFromArray(device, (float*)customData.data(), nextVertex);
				
				entity->Add_physics()->SetMeshCollider();
			};
		}
		return entity;
	}
	
	static bool IsBlockAdditionValid(const std::vector<Block>& existingBlocks, const Block& newBlock) {
		return true; //TODO
	}
	
	std::optional<Block> GetBlock(uint32_t index) const {
		std::lock_guard lock(blocksMutex);
		for (auto& b : blocks) {
			if (b.GetIndex() == index) return b;
		}
		return std::nullopt;
	}
	
	const std::vector<Block>& GetBlocks() const {
		return blocks;
	}
	
	std::shared_ptr<v4d::graphics::RenderableGeometryEntity> SwapBlocksAndRebuild(std::vector<Block>& otherBlocks) {
		std::lock_guard lock(blocksMutex);
		blocks.swap(otherBlocks);
		if (entity) {
			CreateEntity();
			return entity;
		}
		return nullptr;
	}
	
	void SetWorldTransform(glm::dmat4 t) {
		if (entity) {
			entity->SetWorldTransform(t);
		}
	}
	
	glm::dmat4 GetWorldTransform() const {
		return entity->GetWorldTransform();
	}
	
};
