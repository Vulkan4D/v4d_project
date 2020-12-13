#pragma once

class Build {
	std::shared_ptr<v4d::graphics::RenderableGeometryEntity> entity = nullptr;
	std::vector<Block> blocks {};
	mutable std::mutex blocksMutex;
	
public:
	v4d::scene::NetworkGameObject::Id networkId;

	Build(v4d::scene::NetworkGameObject::Id networkId) : networkId(networkId) {}
	
	~Build() {
		if (entity) entity->Destroy();
		std::lock_guard lock(blocksMutex);
		blocks.clear();
	}
	
	std::shared_ptr<v4d::graphics::RenderableGeometryEntity> CreateEntity(glm::dmat4 initialTransform = glm::dmat4(1)) {
		std::lock_guard lock(blocksMutex);
		if (entity) {
			entity->Destroy();
			entity = nullptr;
		}
		if (blocks.size() > 0) {
			entity = RenderableGeometryEntity::Create(THIS_MODULE, networkId);
			entity->Add_physics(v4d::scene::PhysicsInfo::RigidBodyType::STATIC, 1.0f);
			entity->generator = [this, initialTransform](RenderableGeometryEntity* entity, Device* device){
				entity->Prepare(device, "V4D_buildsystem.block");
				entity->SetInitialTransform(initialTransform);
				
				entity->Add_meshIndices();
				entity->Add_meshVertexPosition();
				entity->Add_meshVertexNormal();
				entity->Add_meshVertexColor();
				entity->Add_customData();
				
				std::vector<Mesh::Index> meshIndices (Block::MAX_INDICES * blocks.size());
				std::vector<Mesh::VertexPosition> vertexPositions (Block::MAX_VERTICES * blocks.size());
				std::vector<Mesh::VertexNormal> vertexNormals (Block::MAX_VERTICES * blocks.size());
				std::vector<Mesh::VertexColor> vertexColors (Block::MAX_VERTICES * blocks.size());
				std::vector<uint32_t> customData (Block::MAX_VERTICES * blocks.size());
				
				uint nextVertex = 0;
				uint nextIndex = 0;
				for (int i = 0; i < blocks.size(); ++i) {
					auto[vertexCount, indexCount] = blocks[i].GenerateGeometry(meshIndices.data(), vertexPositions.data(), vertexNormals.data(), vertexColors.data(), customData.data(), nextVertex);
					nextVertex += vertexCount;
					nextIndex += indexCount;
				}
				
				entity->meshIndices->AllocateBuffers(device, meshIndices.data(), nextIndex);
				entity->meshVertexPosition->AllocateBuffers(device, vertexPositions.data(), nextVertex);
				entity->meshVertexNormal->AllocateBuffers(device, vertexNormals.data(), nextVertex);
				entity->meshVertexColor->AllocateBuffers(device, vertexColors.data(), nextVertex);
				entity->customData->AllocateBuffers(device, (float*)customData.data(), nextVertex);
				
				entity->physics->SetMeshCollider();
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
			glm::dmat4 worldTransform;
			auto transform = entity->transform.Lock();
			if (transform && transform->data) {
				worldTransform = transform->data->worldTransform;
			} else {
				worldTransform = entity->initialTransform;
			}
			return entity = CreateEntity(worldTransform);
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
