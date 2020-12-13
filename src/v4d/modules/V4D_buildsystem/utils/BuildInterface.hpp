#pragma once
#include <v4d.h>

struct BuildInterface {
	v4d::scene::Scene* scene = nullptr;
	v4d::graphics::vulkan::Device* device = nullptr;
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
	bool createMode = false;
	bool isValid = false;
	bool isDirty = false;
	bool paintMode = false;
	bool paintModeVertexGradients = false;
	int selectedColor = BLOCK_COLOR_GREY;
	mutable std::recursive_mutex mu;
	
	// RayCast hit block
	std::optional<v4d::graphics::RenderRayCastHit> hitBlock = std::nullopt;
	std::shared_ptr<Build> hitBuild = nullptr;
	std::shared_ptr<Build> tmpBuildParent = nullptr;
	struct {
		bool hasHit = false;
		uint32_t objId = 0;
		uint32_t customData0 = 0;
		glm::vec3 hitPositionOnBuild = {0,0,0};
		glm::vec3 gridPos = {0,0,0};
		bool highPrecisionGrid = false;
		bool createMode = false;
		
		std::weak_ptr<Build> build;
		
		bool Invalidated(const BuildInterface* buildInterface) const {
			if (hasHit != (buildInterface->hitBlock.has_value())) return true;
			if (hasHit) {
				if (this->highPrecisionGrid != buildInterface->highPrecisionGrid) return true;
				if (this->createMode != buildInterface->createMode) return true;
				if (objId != buildInterface->hitBlock->objId) return true;
				if (customData0 != buildInterface->hitBlock->customData0) return true;
				if (highPrecisionGrid) {
					if (gridPos != glm::round(buildInterface->hitBlock->position*10.001f)/10.0f) return true;
				} else {
					if (gridPos != glm::round(buildInterface->hitBlock->position*1.001f)) return true;
				}
			}
			return false;
		}
		
		void Refresh(BuildInterface* buildInterface) {
			this->highPrecisionGrid = buildInterface->highPrecisionGrid;
			this->createMode = buildInterface->createMode;
			
			if (buildInterface->hitBlock.has_value() && buildInterface->hitBuild) {
				build = buildInterface->hitBuild;
			} else {
				build.reset();
			}
			
			hasHit = buildInterface->hitBlock.has_value();
			if (hasHit) {
				objId = buildInterface->hitBlock->objId;
				customData0 = buildInterface->hitBlock->customData0;
				hitPositionOnBuild = buildInterface->hitBlock->position;
				if (highPrecisionGrid) {
					gridPos = glm::round(buildInterface->hitBlock->position*10.001f)/10.0f;
				} else {
					gridPos = glm::round(buildInterface->hitBlock->position*1.001f);
				}
			} else {
				objId = 0;
				customData0 = 0;
			}
			
			// Get parent hit build (if any)
			buildInterface->tmpBuildParent = (buildInterface->hitBlock.has_value() && buildInterface->hitBuild)? buildInterface->hitBuild : nullptr;
		}
		
	} cachedHitBlock;
	
	void UpdateTmpBlock() {
		std::lock_guard lock(mu);
		if (cachedHitBlock.Invalidated(this)) {
			isDirty = false;
			cachedHitBlock.Refresh(this);
			blockRotation = -1;
			FindNextValidBlockRotation();
			RemakeTmpBlock();
		} else if (isDirty) {
			isDirty = false;
			RemakeTmpBlock();
		} else {
			auto cameraParent = scene->cameraParent.lock();
			if (scene && cameraParent && tmpBlock) {
				if (tmpBuildParent) {
					tmpBlock->SetWorldTransform(tmpBuildParent->GetWorldTransform());
				} else {
					double angle = 20;
					glm::dvec3 axis = glm::normalize(glm::dvec3{1,-1,-0.3});
					glm::dvec3 position = {0.0, 0.0, tmpBlock->boundingDistance*-3.0f};
					tmpBlock->SetWorldTransform(glm::rotate(glm::translate(cameraParent->GetWorldTransform(), position), glm::radians(angle), axis));
				}
			}
		}
	}
	
	void Reset() {
		std::lock_guard lock(mu);
		delete tmpBlock;
		tmpBlock = nullptr;
		tmpBuildParent = nullptr;
		hitBuild = nullptr;
	}
	
	int/* score = number of points that are touching with points from the hit face, + 1 or 2 for a good fit */
		TestAddBlockOrientationAndPosition(Block& testBlock, const Build* const parentBuild, const Block& parentBlock, const BlockFace& parentFace, int rotation, glm::vec3 gridPos) {
		if (!parentFace.canAddBlock) return 0;
		
		int score = 1;
		
		// Set initial block position to hit position on grid
		testBlock.SetOrientation(rotation);
		testBlock.SetPosition(gridPos);
		
		{// Find the correct position of our block based on where we are aiming on the surface of the parent build and the selected size/orientation
			// Find the corresponding (opposite) face on the new block
			glm::vec3 hitFaceNormal = Block::GetFaceDirNormal(parentFace.facedirs[0]);
			glm::vec3 hitBuildNormal = glm::round(glm::translate(parentBlock.GetRotationMatrix(), hitFaceNormal)[3]);
			glm::vec3 faceNormal = glm::round(glm::translate(glm::inverse(testBlock.GetRotationMatrix()), -hitBuildNormal)[3]);
			auto blockFace = testBlock.GetTheFaceWhichTheNormalIs(faceNormal);
			
			// Make sure that we have found a valid block face, facing the opposite direction
			if (!blockFace.has_value())
				return 0;
			
			auto&[faceIndex, face] = *blockFace;
			
			{// Next, figure out the correct position for the new block, based on the idea that their respective opposing faces must touch perfectly, but using the hit grid for the other axis
				// get the axis multiplier and its reverse
				auto hitBuildAxis = glm::abs(hitBuildNormal);
				auto hitBuildAxisReverse = glm::abs(hitBuildAxis - 1.0f);
				// prepare a position on the grid that is aligned with the aimed position but at the center of the aimed block on the axis of the aimed face
				auto newPositionOnGrid = gridPos * hitBuildAxisReverse + parentBlock.GetPosition() * hitBuildAxis;
				// offset the position using the sum of half the sizes of both the aimed block and the new block on their respective axis
				float totalOffset = parentBlock.GetHalfSizeForFaceDir(parentFace.facedirs[0]) + testBlock.GetHalfSizeForFaceDir(face.facedirs[0]);
				totalOffset = glm::floor(totalOffset * 10.001f) / 10.0f; // align offset to grid
				newPositionOnGrid += hitBuildNormal * totalOffset;
				// Set the final position to the new block
				testBlock.SetPosition(newPositionOnGrid);
			}
			
			// Count touching points
			auto testBlockPointsPositions = testBlock.GetFinalPointsPositions();
			auto parentBlockPointsPositions = parentBlock.GetFinalPointsPositions();
			for (auto& p : face.triangles) {
				auto point = glm::round(testBlockPointsPositions[p] * 100.0f);
				for (auto& pp : parentFace.triangles) {
					auto parentPoint = glm::round(parentBlockPointsPositions[pp] * 100.0f);
					if (point == parentPoint) score += face.triangles.size() == parentFace.triangles.size()? 2 : 1;
				}
			}
		}
		
		// Make sure there is no conflict with other blocks if we place the new block here
		if (!Build::IsBlockAdditionValid(parentBuild->GetBlocks(), testBlock))
			return 0;
		
		return score;
	}
	
	void RemakeTmpBlock() {
		std::lock_guard lock(mu);
		{
			cachedHitBlock.Refresh(this);
			if (tmpBlock) {
				delete tmpBlock;
				tmpBlock = nullptr;
			}
			if (selectedBlockType != -1) {
				
				// Create a block in the tmp build and assign selected orientation and size
				Block block((SHAPE)selectedBlockType);
				block.SetOrientation(blockRotation);
				block.SetSize({blockSize[selectedBlockType][0], blockSize[selectedBlockType][1], blockSize[selectedBlockType][2]});
				
				glm::vec4 wireframeColor;
				
				if (createMode) {
					// We are not aiming at a parent build, create a new build
					block.SetColor(BLOCK_COLOR_GREY);
				} else {
					// If we are aiming at a parent build, we want to add the new block to that build
					if (tmpBuildParent && hitBlock->distance < 100) {
						
						// Get hit block&face info from raycast's custom data
						PackedBlockCustomData customData;
						customData.packed = cachedHitBlock.customData0;
						auto parentBlock = tmpBuildParent->GetBlock(customData.blockIndex);
						if (!parentBlock.has_value())
							goto INVALID;
							
						// Make sure we have hit a face and not the structure
						if (customData.faceIndex == 7) {
							block.SetSimilarTo(*parentBlock);
							goto INVALID;
						}
						
						auto parentFace = parentBlock->GetFace(customData.faceIndex);
						
						// Make sure we can add a block on this face
						if (!parentFace.canAddBlock) {
							block.SetSimilarTo(*parentBlock);
							goto INVALID;
						}
						
						if (!TestAddBlockOrientationAndPosition(block, tmpBuildParent.get(), parentBlock.value(), parentFace, blockRotation, cachedHitBlock.gridPos)) {
							goto INVALID;
						}
						
						// All is good, block is positioned correctly and does not conflict, make it GREEN
						block.SetColor(BLOCK_COLOR_GREEN);
						
					} else {
						return;
					}
				}
				
				VALID:
					isValid = true;
					wireframeColor = {0.0f, 1.0f, 0.0f, 0.5f};
					goto FINALLY;
				
				INVALID:
					block.SetColor(BLOCK_COLOR_RED);
					wireframeColor = {1.0f, 0.0f, 0.0f, 1.0f};
					isValid = false;
					
				FINALLY:
					tmpBlock = new TmpBlock(block);
					tmpBlock->wireframeColor = wireframeColor;
					UpdateTmpBlock();
			}
		}
	}
	
	glm::dmat4 GetTmpBuildWorldTransform() const {
		std::lock_guard lock(mu);
		return tmpBlock->GetWorldTransform();
	}
	
	bool FindNextValidBlockRotation(int increment = +1 /* or -1 */) {
		for (int i = 0; i < NB_ORIENTATIONS; ++i) {
			if (NextBlockRotation(increment)) return true;
		}
		return false;
	}
	
	bool NextBlockRotation(int increment = +1 /* or -1 */) {
		
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
		
		// Get hit block&face info from raycast's custom data
		PackedBlockCustomData customData;
		customData.packed = cachedHitBlock.customData0;
		std::optional<Block> parentBlock = std::nullopt;
		if (tmpBuildParent && customData.faceIndex != 7 && (parentBlock = tmpBuildParent->GetBlock(customData.blockIndex)).has_value()) {
			std::vector<int> rotations {};{
				rotations.reserve(uniqueOrientations.size());
				for (auto& o : uniqueOrientations) rotations.push_back(o);
			}
			auto parentFace = parentBlock->GetFace(customData.faceIndex);
			Block testBlock((SHAPE)selectedBlockType);{
				testBlock.SetSize({blockSize[selectedBlockType][0], blockSize[selectedBlockType][1], blockSize[selectedBlockType][2]});
			}
			
			// Sort rotations according to which makes the most sense
			std::sort(rotations.begin(), rotations.end(), [&testBlock, &parentFace, &parentBlock, this](int a, int b){
				int aTouches = TestAddBlockOrientationAndPosition(testBlock, tmpBuildParent.get(), parentBlock.value(), parentFace, a, cachedHitBlock.gridPos);
				int bTouches = TestAddBlockOrientationAndPosition(testBlock, tmpBuildParent.get(), parentBlock.value(), parentFace, b, cachedHitBlock.gridPos);
				if (aTouches == bTouches) return a < b;
				return aTouches > bTouches;
			});
			
			// Set next or previous point from the sorted list
			auto it = std::find(rotations.begin(), rotations.end(), blockRotation);
			if (increment > 0) {
				if (it == rotations.end()) blockRotation = rotations.front();
				else blockRotation = *++it;
			} else {
				if (it == rotations.begin()) blockRotation = rotations.back();
				else blockRotation = *--it;
			}
			
			if (!TestAddBlockOrientationAndPosition(testBlock, tmpBuildParent.get(), parentBlock.value(), parentFace, blockRotation, cachedHitBlock.gridPos)) {
				return false;
			}
			
		} else {
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
		
		return true;
	}
	
};
