#pragma once

class Build {
	v4d::scene::ObjectInstancePtr sceneObject = nullptr;
	std::vector<Block> blocks {};
	mutable std::mutex blocksMutex;
	std::weak_ptr<v4d::scene::Geometry> geometryWeakPtr;
	
public:
	v4d::scene::NetworkGameObject::Id networkId;

	Build(v4d::scene::NetworkGameObject::Id networkId) : networkId(networkId) {}
	
	~Build() {
		ClearBlocks();
	}
	
	v4d::scene::ObjectInstancePtr AddToScene(v4d::scene::Scene* scene, glm::dvec3 position = {0, 0, 0}, double angle = (0.0), glm::dvec3 axis = {0, 0, 1}) {
		std::lock_guard lock(blocksMutex);
		sceneObject = scene->AddObjectInstance();
		sceneObject->Configure([this](v4d::scene::ObjectInstance* obj){
			std::lock_guard lock(blocksMutex);
			if (blocks.size() > 0) {
				obj->Lock();
				{
					auto geometry = geometryWeakPtr.lock();
					if (!geometry) {
						geometryWeakPtr = geometry = obj->AddGeometry("block", Block::MAX_VERTICES*blocks.size(), Block::MAX_INDICES*blocks.size());
					} else {
						geometry->Reset(Block::MAX_VERTICES*blocks.size(), Block::MAX_INDICES*blocks.size());
					}
					
					uint nextVertex = 0;
					uint nextIndex = 0;
					for (int i = 0; i < blocks.size(); ++i) {
						auto[vertexCount, indexCount] = blocks[i].GenerateGeometry(geometry->GetVertexPtr(nextVertex), geometry->GetIndexPtr(nextIndex), nextVertex, 0.6);
						nextVertex += vertexCount;
						nextIndex += indexCount;
					}
					geometry->Shrink(nextVertex, nextIndex);
					
					for (int i = 0; i < geometry->vertexCount; ++i) {
						auto* vert = geometry->GetVertexPtr(i);
						geometry->boundingDistance = glm::max(geometry->boundingDistance, glm::length(vert->pos));
						geometry->boundingBoxSize = glm::max(glm::abs(vert->pos), geometry->boundingBoxSize);
					}
					geometry->isDirty = true;
				}
				obj->Unlock();
			}
		}, position, angle, axis);
		return sceneObject;
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
	
	void ResetGeometry() {
		if (sceneObject) {
			sceneObject->ResetGeometries();
		}
	}
	
	void SwapBlocksVector(std::vector<Block>& other) {
		std::lock_guard lock(blocksMutex);
		blocks.swap(other);
		ResetGeometry();
	}

	void ClearBlocks() {
		std::lock_guard lock(blocksMutex);
		blocks.clear();
	}

	void SetWorldTransform(glm::dmat4 t) {
		if (sceneObject) {
			sceneObject->SetWorldTransform(t);
		}
	}
	
	glm::dmat4 GetWorldTransform() const {
		return sceneObject->GetWorldTransform();
	}
	
};
