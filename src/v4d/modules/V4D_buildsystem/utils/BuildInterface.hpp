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
	bool isValid = false;
	std::recursive_mutex mu;
	
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
		
		std::weak_ptr<Build> build;
		
		bool Invalidated(const std::optional<v4d::graphics::RenderRayCastHit>& hitBlock, bool highPrecisionGrid) {
			if (hasHit != (hitBlock.has_value())) return true;
			if (hasHit) {
				if (this->highPrecisionGrid != highPrecisionGrid) return true;
				if (objId != hitBlock->objId) return true;
				if (customData0 != hitBlock->customData0) return true;
				if (highPrecisionGrid) {
					if (gridPos != glm::round(hitBlock->position*10.01f)/10.0f) return true;
				} else {
					if (gridPos != glm::round(hitBlock->position*1.01f)) return true;
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
					gridPos = glm::round(hitBlock->position*10.01f)/10.0f;
				} else {
					gridPos = glm::round(hitBlock->position*1.01f);
				}
			} else {
				objId = 0;
				customData0 = 0;
			}
		}
	} cachedHitBlock;
	
	void UpdateTmpBlock() {
		std::lock_guard lock(mu);
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
		std::lock_guard lock(mu);
		float tmpBlockBoundingDistance = 1.0;
		scene->Lock();
		{
			cachedHitBlock.highPrecisionGrid = highPrecisionGrid;
			if (hitBlock.has_value() && hitBuild) {
				cachedHitBlock.build = hitBuild;
			} else {
				cachedHitBlock.build.reset();
			}
			cachedHitBlock = hitBlock;
			if (tmpBlock) {
				tmpBlockBoundingDistance = tmpBlock->boundingDistance;
				delete tmpBlock;
				tmpBlock = nullptr;
			}
			tmpBuildParent = nullptr;
			if (selectedBlockType != -1) {
				
				// Create a tmp build
				tmpBlock = new TmpBlock(scene);
				
				// Get parent hit build (if any)
				tmpBuildParent = (hitBlock.has_value() && hitBuild)? hitBuild : nullptr;
				
				// Create a block in the tmp build and assign selected orientation and size
				Block& block = tmpBlock->SetBlock((SHAPE)selectedBlockType);
				block.SetOrientation(blockRotation);
				block.SetSize({blockSize[selectedBlockType][0], blockSize[selectedBlockType][1], blockSize[selectedBlockType][2]});
				
				// If we are aiming at a parent build, we want to add the new block to that build
				if (tmpBuildParent) {
					
					// Get hit block&face info from raycast's custom data
					PackedBlockCustomData customData;
					customData.packed = cachedHitBlock.customData0;
					auto parentBlock = tmpBuildParent->GetBlock(customData.blockIndex);
					if (!parentBlock.has_value())
						goto INVALID;
					
					auto parentFace = parentBlock->GetFace(customData.faceIndex);
					
					// Set initial block position to hit position on grid
					block.SetPosition(cachedHitBlock.gridPos);
					
					// Make sure we can add a block on this face
					if (!parentFace.canAddBlock)
						goto INVALID;
						
					{// Find the correct position of our block based on where we are aiming on the surface of the parent build and the selected size/orientation
						
						// Find the corresponding (opposite) face on the new block
						glm::vec3 hitFaceNormal = Block::GetFaceDirNormal(parentFace.facedirs[0]);
						glm::vec3 hitBuildNormal = glm::round(glm::translate(parentBlock->GetRotationMatrix(), hitFaceNormal)[3]);
						glm::vec3 faceNormal = glm::round(glm::translate(glm::inverse(block.GetRotationMatrix()), -hitBuildNormal)[3]);
						auto blockFace = block.GetTheFaceWhichTheNormalIs(faceNormal);
						
						// Make sure that we have found a valid block face, facing the opposite direction
						if (!blockFace.has_value())
							goto INVALID;
						
						auto faceIndex = std::get<0>(*blockFace);
						auto face = std::get<1>(*blockFace);
						
						{// Next, figure out the correct position for the new block, based on the idea that their respective opposing faces must touch perfectly, but using the hit grid for the other axis
							// get the axis multiplier and its reverse
							auto hitBuildAxis = glm::abs(hitBuildNormal);
							auto hitBuildAxisReverse = glm::abs(hitBuildAxis - 1.0f);
							// prepare a position on the grid that is aligned with the aimed position but at the center of the aimed block on the axis of the aimed face
							auto gridPosition = cachedHitBlock.gridPos * hitBuildAxisReverse + parentBlock->GetPosition() * hitBuildAxis;
							// offset the position using the sum of half the sizes of both the aimed block and the new block on their respective axis
							float totalOffset = parentBlock->GetHalfSizeForFaceDir(parentFace.facedirs[0]) + block.GetHalfSizeForFaceDir(face.facedirs[0]);
							totalOffset = glm::floor(totalOffset * 10.001f) / 10.0f; // align offset to grid
							gridPosition += hitBuildNormal * totalOffset;
							// Set the final position to the new block
							block.SetPosition(gridPosition);
						}
					}
					
					// Make sure there is no conflict with other blocks if we place the new block here
					if (!Build::IsBlockAdditionValid(tmpBuildParent->GetBlocks(), block))
						goto INVALID;
					
					// All is good, block is positioned correctly and does not conflict, make it GREEN
					block.SetColor(BLOCK_COLOR_GREEN);
					
				} else {
					// We are not aiming at a parent build, create a new build
					block.SetColor(BLOCK_COLOR_GREY);
				}
				
				VALID:
					isValid = true;
					tmpBlock->wireframeColor = {0.0f, 1.0f, 0.0f, 0.5f};
					goto FINALLY;
				
				INVALID:
					block.SetColor(BLOCK_COLOR_RED);
					tmpBlock->wireframeColor = {1.0f, 0.0f, 0.0f, 1.0f};
					isValid = false;
					
				FINALLY:
					tmpBlock->boundingDistance = tmpBlockBoundingDistance;
					tmpBlock->ResetGeometry();
					UpdateTmpBlock();
			}
		}
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
		
		std::lock_guard lock(mu);
		
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
		std::lock_guard lock(mu);
		if (tmpBlock) delete tmpBlock;
		tmpBuildParent = nullptr;
		hitBuild = nullptr;
	}
};
