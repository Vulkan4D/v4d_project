#pragma once
#include <set>


struct BuildInterface {
	v4d::scene::Scene* scene = nullptr;
	int selectedBlockType = -1;
	float blockSize[NB_BLOCKS][3] = {
		{1.0f, 1.0f, 1.0f},
		{1.0f, 1.0f, 1.0f},
		{1.0f, 1.0f, 1.0f},
		{1.0f, 1.0f, 1.0f},
		{1.0f, 1.0f, 1.0f},
	};
	int selectedEditValue = 0;
	TmpBlock* tmpBlock = nullptr;
	int blockRotation = 0;
	bool highPrecisionGrid = false;
	
	// RayCast hit block
	std::optional<v4d::graphics::RenderRayCastHit> hitBlock = std::nullopt;
	std::shared_ptr<Build> hitBuild = nullptr;
	std::shared_ptr<Build> tmpBuildParent = nullptr;
	struct {
		bool hasHit = false;
		uint32_t objId = 0;
		uint32_t customData0 = 0;
		glm::vec3 gridPos = {0,0,0};
		bool highPrecisionGrid = false;
		
		bool Invalidated(const std::optional<v4d::graphics::RenderRayCastHit>& hitBlock, bool highPrecisionGrid) {
			if (hasHit != (hitBlock.has_value())) return true;
			if (hasHit) {
				if (this->highPrecisionGrid != highPrecisionGrid) return true;
				if (objId != hitBlock->objId) return true;
				if (customData0 != hitBlock->customData0) return true;
				if (highPrecisionGrid) {
					if (gridPos != glm::round(hitBlock->position*10.0f)/10.0f) return true;
				} else {
					if (gridPos != glm::round(hitBlock->position)) return true;
				}
			}
			return false;
		}
		void operator= (const std::optional<v4d::graphics::RenderRayCastHit>& hitBlock) {
			hasHit = hitBlock.has_value();
			if (hasHit) {
				objId = hitBlock->objId;
				customData0 = hitBlock->customData0;
				if (highPrecisionGrid) {
					gridPos = glm::round(hitBlock->position*10.0f)/10.0f;
				} else {
					gridPos = glm::round(hitBlock->position);
				}
			} else {
				objId = 0;
				customData0 = 0;
			}
		}
	} cachedHitBlock;
	
	void UpdateTmpBlock() {
		if (cachedHitBlock.Invalidated(hitBlock, highPrecisionGrid)) {
			RemakeTmpBlock();
		} else {
			if (scene && scene->cameraParent && tmpBlock) {
				scene->Lock();
					if (tmpBuildParent) {
						tmpBlock->SetWorldTransform(tmpBuildParent->GetWorldTransform());
					} else {
						double angle = 20;
						glm::dvec3 axis = glm::normalize(glm::dvec3{1,-1,-0.3});
						glm::dvec3 position = {0.0, 0.0, tmpBlock->boundingDistance*-3.0f};
						tmpBlock->SetWorldTransform(glm::rotate(glm::translate(scene->cameraParent->GetWorldTransform(), position), glm::radians(angle), axis));
					}
				scene->Unlock();
			}
		}
	}
	
	void RemakeTmpBlock() {
		float tmpBlockBoundingDistance = 1.0;
		scene->Lock();
			cachedHitBlock.highPrecisionGrid = highPrecisionGrid;
			cachedHitBlock = hitBlock;
			if (tmpBlock) {
				tmpBlockBoundingDistance = tmpBlock->boundingDistance;
				delete tmpBlock;
				tmpBlock = nullptr;
			}
			tmpBuildParent = nullptr;
			if (selectedBlockType != -1) {
				tmpBlock = new TmpBlock(scene);
				tmpBuildParent = (hitBlock.has_value() && hitBuild)? hitBuild : nullptr;
				Block& block = tmpBlock->SetBlock((SHAPE)selectedBlockType);
				auto currentBlockRotation = blockRotation;
				auto currentBlockSize = blockSize[selectedBlockType];
				if (tmpBuildParent) {
					union {
						struct {
							uint32_t blockIndex : 24;
							uint32_t faceIndex : 3;
							uint32_t materialId : 5;
						};
						uint32_t packed;
					} customData;
					customData.packed = cachedHitBlock.customData0;
					auto parentBlock = tmpBuildParent->GetBlock(customData.blockIndex);
					auto parentFace = parentBlock.GetFace(customData.faceIndex);
					
					
					
					
					LOG(cachedHitBlock.gridPos.x << ", " << cachedHitBlock.gridPos.y << ", " << cachedHitBlock.gridPos.z)
					
					
					
					
					
					
					
					// if (parentFace.resizedirs.size() == 0) {
					// 	// Can't put a block on this face
					// 	delete tmpBlock;
					// 	tmpBlock = nullptr;
					// 	goto Unlock;
					// }
					// auto parentPoints = parentBlock.GetPointsPositions();
					// glm::vec3 parentFaceNormal = glm::normalize(glm::cross(parentPoints[parentFace.triangles[1]] - parentPoints[parentFace.triangles[0]], parentPoints[parentFace.triangles[1]] - parentPoints[parentFace.triangles[2]]));
					// glm::vec3 parentFacePosition = {0,0,0};
					// for (auto& p : parentFace.triangles) {
					// 	parentFacePosition += parentPoints[p];
					// }
					// parentFacePosition /= parentFace.triangles.size();
					
					// //TODO constraint currentBlockRotation
					block.SetOrientation(currentBlockRotation);
					
					// //TODO constraint currentBlockSize
					block.SetSize({currentBlockSize[0], currentBlockSize[1], currentBlockSize[2]});
					
					// // auto points = block.GetPointsPositions();
					
					// block.SetPosition(parentBlock.GetPosition() + parentFacePosition + parentFaceNormal/2.0f);
					
					block.SetPosition(cachedHitBlock.gridPos);
					
					
					
					
					
					
				} else {
					block.SetOrientation(currentBlockRotation);
					block.SetSize({currentBlockSize[0], currentBlockSize[1], currentBlockSize[2]});
				}
				tmpBlock->boundingDistance = tmpBlockBoundingDistance;
				tmpBlock->ResetGeometry();
				UpdateTmpBlock();
			}
			Unlock:
		scene->Unlock();
	}
	
	
	void NextBlockRotation(int increment = +1 /* or -1 */) {
		
		// Skip orientations that results in a similar block
		
		std::set<int> uniqueOrientations {}; {
			// Create a temporary block to test orientations with
			Block tmp {(SHAPE)selectedBlockType};
			tmp.SetSize({blockSize[selectedBlockType][0], blockSize[selectedBlockType][1], blockSize[selectedBlockType][2]});
			std::unordered_map<std::string, int> uniqueOrientationsMap {};
			// for each orientations
			for (int i = 0; i < NB_ORIENTATIONS; ++i) {
				// Set an orientation and get all raw vertex points
				tmp.SetOrientation(i);
				auto points = tmp.GetPointsPositions(false);
				// Pack all points to a unique sorted set
				std::set<uint64_t> packedPoints {points.size()};
				for (auto& p : points) {
					union {
						struct {
							int64_t x : 21;
							int64_t y : 21;
							int64_t z : 21;
							int64_t : 1;
						};
						uint64_t packed;
					} packedPoint;
					packedPoint.x = (int32_t)glm::round(p.x);
					packedPoint.y = (int32_t)glm::round(p.y);
					packedPoint.z = (int32_t)glm::round(p.z);
					packedPoints.emplace(packedPoint.packed);
				}
				std::string hash {""};
				for (auto& p : packedPoints) {
					hash += std::to_string(p) + ";";
				}
				uniqueOrientationsMap[hash] = i;
			}
			for (auto&[hash, i] : uniqueOrientationsMap) {
				uniqueOrientations.emplace(i);
			}
		}
		
		// Set next or previous point from the unique list
		if (increment > 0) {
			auto it = uniqueOrientations.upper_bound(blockRotation);
			if (it == uniqueOrientations.end()) it = uniqueOrientations.begin();
			blockRotation = *it;
		} else {
			auto it = uniqueOrientations.lower_bound(blockRotation);
			if (it == uniqueOrientations.begin()) it = uniqueOrientations.end();
			blockRotation = *--it;
		}
	}
	
	void UnloadScene() {
		if (tmpBlock) delete tmpBlock;
		tmpBuildParent = nullptr;
		hitBuild = nullptr;
	}
};
