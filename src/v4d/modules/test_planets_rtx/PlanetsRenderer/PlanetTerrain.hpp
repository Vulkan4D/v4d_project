#pragma once
#include <v4d.h>

#include "CubeToSphere.hpp"
#include "../../incubator_galaxy4d/Noise.hpp"

using namespace v4d::graphics;
using namespace v4d::graphics::vulkan;

struct PlanetTerrain {
	
	#pragma region Constructor arguments
	double radius; // top of atmosphere (maximum radius)
	double solidRadius; // standard radius of solid surface (AKA sea level, surface height can go below or above)
	double heightVariation; // half the total variation (surface height is +- heightVariation)
	glm::dvec3 absolutePosition; // position of planet relative to world (star system center)
	#pragma endregion
	
	glm::dvec3 rotationAxis {0,0,1};
	double rotationAngle = 0;
	glm::dmat4 matrix {1};
	
	#pragma region Graphics configuration
	static const int chunkSubdivisionsPerFace = 1;
	static const int vertexSubdivisionsPerChunk = 256; // low=32, medium=64, high=128, extreme=256
	static constexpr float chunkSubdivisionDistanceFactor = 1.0f;
	static constexpr float targetVertexSeparationInMeters = 1.0f; // approximative vertex separation in meters for the most precise level of detail
	static const size_t chunkGeneratorNbThreads = 4;
	static constexpr double garbageCollectionInterval = 20; // seconds
	static constexpr double chunkOptimizationMinMoveDistance = 500; // meters
	static constexpr double chunkOptimizationMinTimeInterval = 10; // seconds
	static const bool useSkirts = true;
	#pragma endregion

	#pragma region Calculated constants
	static const int nbChunksPerFace = chunkSubdivisionsPerFace * chunkSubdivisionsPerFace;
	static const int nbBaseChunksPerPlanet = nbChunksPerFace * 6;
	static const int nbVerticesPerChunk = (vertexSubdivisionsPerChunk+1) * (vertexSubdivisionsPerChunk+1) + (useSkirts? (vertexSubdivisionsPerChunk * 4) : 0);
	static const int nbIndicesPerChunk = vertexSubdivisionsPerChunk*vertexSubdivisionsPerChunk*6 + (useSkirts? (vertexSubdivisionsPerChunk * 4 * 6) : 0);
	#pragma endregion

	// Camera
	glm::dvec3 cameraPos {0};
	double cameraAltitudeAboveTerrain = 0;
	
	// Cache
	glm::dvec3 lastOptimizePosition {0};
	v4d::Timer lastOptimizeTime {true};
	static v4d::Timer lastGarbageCollectionTime;
	
	// Info
	int totalChunks = 0;
	int activeChunks = 0;
	int renderedChunks = 0;
	
	struct Chunk {
	
		#pragma region Constructor arguments
		PlanetTerrain* planet;
		CubeToSphere::FACE face;
		int level;
		// Cube positions (-1.0 to +1.0)
		glm::dvec3 topLeft;
		glm::dvec3 topRight;
		glm::dvec3 bottomLeft;
		glm::dvec3 bottomRight;
		#pragma endregion
		
		#pragma region Caching
		// Cube positions (-1.0 to +1.0)
		glm::dvec3 top {0};
		glm::dvec3 left {0};
		glm::dvec3 right {0};
		glm::dvec3 bottom {0};
		glm::dvec3 center {0};
		// true positions on planet
		glm::dvec3 centerPos {0};
		glm::dvec3 topLeftPos {0};
		glm::dvec3 topRightPos {0};
		glm::dvec3 bottomLeftPos {0};
		glm::dvec3 bottomRightPos {0};
		glm::dvec3 centerPosLowestPoint {0};
		glm::dvec3 topLeftPosLowestPoint {0};
		glm::dvec3 topRightPosLowestPoint {0};
		glm::dvec3 bottomLeftPosLowestPoint {0};
		glm::dvec3 bottomRightPosLowestPoint {0};
		glm::dvec3 centerPosHighestPoint {0};
		double chunkSize = 0;
		double triangleSize = 0;
		double heightAtCenter = 0;
		double boundingDistance = 0;
		std::atomic<double> distanceFromCamera = 0;
		#pragma endregion
		
		#pragma region States
		std::atomic<bool> active = false;
		std::atomic<bool> render = false;
		std::atomic<bool> allocated = false;
		std::atomic<bool> meshShouldGenerate = false;
		std::atomic<bool> meshGenerated = false;
		std::atomic<bool> meshGenerating = false;
		std::recursive_mutex stateMutex;
		std::recursive_mutex subChunksMutex;
		#pragma endregion
		
		#pragma region Data
		std::vector<Chunk*> subChunks {};
		std::mutex generatorMutex;
		ObjectInstance* obj = nullptr;
		Geometry* geometry = nullptr;
		#pragma endregion
		
		bool IsLastLevel() {
			return (triangleSize < targetVertexSeparationInMeters*1.9);
		}
		
		bool ShouldAddSubChunks() {
			if (IsLastLevel()) return false;
			return distanceFromCamera < chunkSubdivisionDistanceFactor*chunkSize;
		}
		
		bool ShouldRemoveSubChunks() {
			if (IsLastLevel()) return false;
			return distanceFromCamera > chunkSubdivisionDistanceFactor*chunkSize * 1.5;
		}
		
		void RefreshDistanceFromCamera() {
			distanceFromCamera = glm::distance(planet->cameraPos, centerPos);
			if (distanceFromCamera > chunkSize/2.0)
				distanceFromCamera = glm::min((double)distanceFromCamera, glm::distance(planet->cameraPos, topLeftPos));
			if (distanceFromCamera > chunkSize/2.0)
				distanceFromCamera = glm::min((double)distanceFromCamera, glm::distance(planet->cameraPos, topRightPos));
			if (distanceFromCamera > chunkSize/2.0)
				distanceFromCamera = glm::min((double)distanceFromCamera, glm::distance(planet->cameraPos, bottomLeftPos));
			if (distanceFromCamera > chunkSize/2.0)
				distanceFromCamera = glm::min((double)distanceFromCamera, glm::distance(planet->cameraPos, bottomRightPos));
			if (distanceFromCamera < chunkSize)
				distanceFromCamera = glm::min((double)distanceFromCamera, planet->cameraAltitudeAboveTerrain);
			distanceFromCamera = glm::max((double)distanceFromCamera, 1.0);
		}
		
		Chunk(PlanetTerrain* planet, int face, int level, glm::dvec3 topLeft, glm::dvec3 topRight, glm::dvec3 bottomLeft, glm::dvec3 bottomRight)
		: planet(planet), face((CubeToSphere::FACE)face), level(level), topLeft(topLeft), topRight(topRight), bottomLeft(bottomLeft), bottomRight(bottomRight) {
			
			chunkSize = planet->GetSolidCirconference() / 4.0 / chunkSubdivisionsPerFace / glm::pow(2, level);
			triangleSize = chunkSize/vertexSubdivisionsPerChunk;
			center = (topLeft + topRight + bottomLeft + bottomRight) / 4.0;
			top = (topLeft + topRight) / 2.0;
			left = (topLeft + bottomLeft) / 2.0;
			right = (topRight + bottomRight) / 2.0;
			bottom = (bottomLeft + bottomRight) / 2.0;
			
			//TODO maybe optimize these, get the actual altitudes instead of potential extrimities
			double lowestAltitude = planet->solidRadius - glm::max(chunkSize/2.0, planet->heightVariation*2.0);
			double highestAltitude = planet->solidRadius + planet->heightVariation;
			
			centerPos = CubeToSphere::Spherify(center, face);
			centerPosLowestPoint = centerPos * lowestAltitude;
			centerPosHighestPoint = centerPos * highestAltitude;
			heightAtCenter = planet->GetHeightMap(centerPos, triangleSize);
			centerPos = glm::round(centerPos * heightAtCenter);
			topLeftPos = CubeToSphere::Spherify(topLeft, face);
			topLeftPosLowestPoint = topLeftPos * lowestAltitude;
			topLeftPos *= planet->GetHeightMap(topLeftPos, triangleSize);
			topRightPos = CubeToSphere::Spherify(topRight, face);
			topRightPosLowestPoint = topRightPos * lowestAltitude;
			topRightPos *= planet->GetHeightMap(topRightPos, triangleSize);
			bottomLeftPos = CubeToSphere::Spherify(bottomLeft, face);
			bottomLeftPosLowestPoint = bottomLeftPos * lowestAltitude;
			bottomLeftPos *= planet->GetHeightMap(bottomLeftPos, triangleSize);
			bottomRightPos = CubeToSphere::Spherify(bottomRight, face);
			bottomRightPosLowestPoint = bottomRightPos * lowestAltitude;
			bottomRightPos *= planet->GetHeightMap(bottomRightPos, triangleSize);
			
			RefreshDistanceFromCamera();
		}
		
		~Chunk() {
			for (auto* subChunk : subChunks) {
				delete subChunk;
			}
			subChunks.clear();
			Remove();
			std::lock_guard lock(generatorMutex);
		}
		
		bool Cleanup() { // bool returns whether to delete the parent as well
			std::scoped_lock lock(stateMutex, subChunksMutex);
			
			bool mustCleanup = true;
			for (auto* subChunk : subChunks) {
				if (!subChunk->Cleanup()) {
					mustCleanup = false;
				}
			}
			if (!mustCleanup) return false;
			
			for (auto* subChunk : subChunks) {
				delete subChunk;
			}
			subChunks.clear();
			
			if (meshShouldGenerate || render || allocated)
				return false;
			
			CancelMeshGeneration();
			std::lock_guard lockGenerator(generatorMutex);
			return true;
		}
		
		void CancelMeshGeneration() {
			meshShouldGenerate = false;
		}
		
		void Generate() {
			if (!obj) {
				obj = new ObjectInstance("planet_terrain");
			}
			if (!geometry) {
				geometry = obj->AddGeometry(nbVerticesPerChunk, nbIndicesPerChunk /* , material */ );
			}
			
			auto [faceDir, topDir, rightDir] = GetFaceVectors(face);
			double topSign = topDir.x + topDir.y + topDir.z;
			double rightSign = rightDir.x + rightDir.y + rightDir.z;
			
			int genRow = 0;
			int genCol = 0;
			int genVertexIndex = 0;
			int genIndexIndex = 0;
			
			while (genRow <= vertexSubdivisionsPerChunk) {
				while (genCol <= vertexSubdivisionsPerChunk) {
					uint32_t currentIndex = (vertexSubdivisionsPerChunk+1) * genRow + genCol;
					
					glm::dvec3 topOffset = glm::mix(topLeft - center, bottomLeft - center, double(genRow)/vertexSubdivisionsPerChunk);
					glm::dvec3 rightOffset = glm::mix(topLeft - center, topRight - center, double(genCol)/vertexSubdivisionsPerChunk);
					
					// position
					glm::dvec3 pos = CubeToSphere::Spherify(center + topDir*topOffset + rightDir*rightOffset, face);
					double altitude = planet->GetHeightMap(pos, triangleSize);
					glm::dvec3 posOnChunk = pos * altitude - centerPos;
					auto* vertex = geometry->GetVertexPtr(currentIndex);
					vertex->pos = glm::vec4(posOnChunk, altitude - planet->solidRadius);
					vertex->SetUV(glm::vec4(glm::vec2(genCol, genRow) / float(vertexSubdivisionsPerChunk), 0, 0));
					genVertexIndex++;
					
					// Bounding distance
					boundingDistance = std::max(boundingDistance, glm::length(posOnChunk));

					// indices
					if (genRow < vertexSubdivisionsPerChunk) {
						uint32_t topLeftIndex = currentIndex;
						uint32_t topRightIndex = topLeftIndex+1;
						uint32_t bottomLeftIndex = (vertexSubdivisionsPerChunk+1) * (genRow+1) + genCol;
						uint32_t bottomRightIndex = bottomLeftIndex+1;
						if (genCol < vertexSubdivisionsPerChunk) {
							if (topSign == rightSign) {
								geometry->SetIndex(genIndexIndex++, topLeftIndex);
								geometry->SetIndex(genIndexIndex++, bottomLeftIndex);
								geometry->SetIndex(genIndexIndex++, bottomRightIndex);
								geometry->SetIndex(genIndexIndex++, topLeftIndex);
								geometry->SetIndex(genIndexIndex++, bottomRightIndex);
								geometry->SetIndex(genIndexIndex++, topRightIndex);
							} else {
								geometry->SetIndex(genIndexIndex++, topLeftIndex);
								geometry->SetIndex(genIndexIndex++, bottomRightIndex);
								geometry->SetIndex(genIndexIndex++, bottomLeftIndex);
								geometry->SetIndex(genIndexIndex++, topLeftIndex);
								geometry->SetIndex(genIndexIndex++, topRightIndex);
								geometry->SetIndex(genIndexIndex++, bottomRightIndex);
							}
						}
					}
					
					++genCol;
					
					if (!meshShouldGenerate) return;
				}
				
				genCol = 0;
				++genRow;
			}
			
			if (!meshShouldGenerate) return;
			
			// Normals
			for (genRow = 0; genRow <= vertexSubdivisionsPerChunk; ++genRow) {
				for (genCol = 0; genCol <= vertexSubdivisionsPerChunk; ++genCol) {
					uint32_t currentIndex = (vertexSubdivisionsPerChunk+1) * genRow + genCol;
					auto* vertex = geometry->GetVertexPtr(currentIndex);
					
					glm::vec3 tangentX, tangentY;
					
					if (genRow < vertexSubdivisionsPerChunk && genCol < vertexSubdivisionsPerChunk) {
						// For full face (generate top left)
						uint32_t topLeftIndex = currentIndex;
						uint32_t topRightIndex = topLeftIndex+1;
						uint32_t bottomLeftIndex = (vertexSubdivisionsPerChunk+1) * (genRow+1) + genCol;
						
						tangentX = glm::normalize(geometry->GetVertexPtr(topRightIndex)->pos - vertex->pos);
						tangentY = glm::normalize(vertex->pos - geometry->GetVertexPtr(bottomLeftIndex)->pos);
						
					} else if (genCol == vertexSubdivisionsPerChunk && genRow == vertexSubdivisionsPerChunk) {
						// For right-most bottom-most vertex (generate bottom-most right-most)
						
						glm::vec3 bottomLeftPos {0};
						{
							glm::dvec3 topOffset = glm::mix(topLeft - center, bottomLeft - center, double(genRow+1)/vertexSubdivisionsPerChunk);
							glm::dvec3 rightOffset = glm::mix(topLeft - center, topRight - center, double(genCol)/vertexSubdivisionsPerChunk);
							glm::dvec3 pos = Spherify(center + topDir*topOffset + rightDir*rightOffset, face);
							bottomLeftPos = {pos * planet->GetHeightMap(pos, triangleSize) - centerPos};
						}

						glm::vec3 topRightPos {0};
						{
							glm::dvec3 topOffset = glm::mix(topLeft - center, bottomLeft - center, double(genRow)/vertexSubdivisionsPerChunk);
							glm::dvec3 rightOffset = glm::mix(topLeft - center, topRight - center, double(genCol+1)/vertexSubdivisionsPerChunk);
							glm::dvec3 pos = Spherify(center + topDir*topOffset + rightDir*rightOffset, face);
							topRightPos = {pos * planet->GetHeightMap(pos, triangleSize) - centerPos};
						}

						tangentX = glm::normalize(topRightPos - vertex->pos);
						tangentY = glm::normalize(vertex->pos - bottomLeftPos);
						
					} else if (genCol == vertexSubdivisionsPerChunk) {
						// For others in right col (generate top right)
						uint32_t bottomRightIndex = currentIndex+vertexSubdivisionsPerChunk+1;
						
						glm::vec3 topRightPos {0};
						{
							glm::dvec3 topOffset = glm::mix(topLeft - center, bottomLeft - center, double(genRow)/vertexSubdivisionsPerChunk);
							glm::dvec3 rightOffset = glm::mix(topLeft - center, topRight - center, double(genCol+1)/vertexSubdivisionsPerChunk);
							glm::dvec3 pos = Spherify(center + topDir*topOffset + rightDir*rightOffset, face);
							topRightPos = {pos * planet->GetHeightMap(pos, triangleSize) - centerPos};
						}

						tangentX = glm::normalize(topRightPos - vertex->pos);
						tangentY = glm::normalize(vertex->pos - geometry->GetVertexPtr(bottomRightIndex)->pos);
						
					} else if (genRow == vertexSubdivisionsPerChunk) {
						// For others in bottom row (generate bottom left)
						
						glm::vec3 bottomLeftPos {0};
						{
							glm::dvec3 topOffset = glm::mix(topLeft - center, bottomLeft - center, double(genRow+1)/vertexSubdivisionsPerChunk);
							glm::dvec3 rightOffset = glm::mix(topLeft - center, topRight - center, double(genCol)/vertexSubdivisionsPerChunk);
							glm::dvec3 pos = Spherify(center + topDir*topOffset + rightDir*rightOffset, face);
							bottomLeftPos = {pos * planet->GetHeightMap(pos, triangleSize) - centerPos};
						}

						tangentX = glm::normalize(geometry->GetVertexPtr(currentIndex+1)->pos - vertex->pos);
						tangentY = glm::normalize((vertex->pos - bottomLeftPos));
					}
					
					tangentX *= (float)rightSign;
					tangentY *= (float)topSign;
					
					glm::vec3 normal = glm::normalize(glm::cross(tangentX, tangentY));
					
					vertex->normal = normal;
					
					// slope = (float) glm::max(0.0, dot(glm::dvec3(normal), glm::normalize(centerPos + glm::dvec3(vertex->pos))));
				}
			}
			
			// Skirts
			if (useSkirts) {
				int firstSkirtIndex = genVertexIndex;
				auto addSkirt = [this, &genVertexIndex, &genIndexIndex, firstSkirtIndex, topSign, rightSign](int pointIndex, int nextPointIndex, bool firstPoint = false, bool lastPoint = false) {
					int skirtIndex = genVertexIndex++;
					*geometry->GetVertexPtr(skirtIndex) = *geometry->GetVertexPtr(pointIndex);
					geometry->GetVertexPtr(skirtIndex)->pos -= glm::vec3(glm::normalize(centerPos)*chunkSize/double(vertexSubdivisionsPerChunk)*2.0);
					if (topSign == rightSign) {
						geometry->SetIndex(genIndexIndex++, pointIndex);
						geometry->SetIndex(genIndexIndex++, skirtIndex);
						geometry->SetIndex(genIndexIndex++, nextPointIndex);
						geometry->SetIndex(genIndexIndex++, nextPointIndex);
						geometry->SetIndex(genIndexIndex++, skirtIndex);
						if (lastPoint) {
							geometry->SetIndex(genIndexIndex++, firstSkirtIndex);
						} else {
							geometry->SetIndex(genIndexIndex++, skirtIndex + 1);
						}
					} else {
						geometry->SetIndex(genIndexIndex++, pointIndex);
						geometry->SetIndex(genIndexIndex++, nextPointIndex);
						geometry->SetIndex(genIndexIndex++, skirtIndex);
						geometry->SetIndex(genIndexIndex++, skirtIndex);
						geometry->SetIndex(genIndexIndex++, nextPointIndex);
						if (lastPoint) {
							geometry->SetIndex(genIndexIndex++, firstSkirtIndex);
						} else {
							geometry->SetIndex(genIndexIndex++, skirtIndex + 1);
						}
					}
				};
				// Left
				for (int i = 0; i < vertexSubdivisionsPerChunk; ++i) {
					int pointIndex = i * (vertexSubdivisionsPerChunk+1);
					int nextPointIndex = (i+1) * (vertexSubdivisionsPerChunk+1);
					addSkirt(pointIndex, nextPointIndex, i == 0);
				}
				// Bottom
				for (int i = 0; i < vertexSubdivisionsPerChunk; ++i) {
					int pointIndex = vertexSubdivisionsPerChunk*(vertexSubdivisionsPerChunk+1) + i;
					int nextPointIndex = pointIndex + 1;
					addSkirt(pointIndex, nextPointIndex);
				}
				// Right
				for (int i = 0; i < vertexSubdivisionsPerChunk; ++i) {
					int pointIndex = (vertexSubdivisionsPerChunk+1) * (vertexSubdivisionsPerChunk+1) - 1 - i*(vertexSubdivisionsPerChunk+1);
					int nextPointIndex = pointIndex - vertexSubdivisionsPerChunk - 1;
					addSkirt(pointIndex, nextPointIndex);
				}
				// Top
				for (int i = 0; i < vertexSubdivisionsPerChunk; ++i) {
					int pointIndex = vertexSubdivisionsPerChunk - i;
					int nextPointIndex = pointIndex - 1;
					addSkirt(pointIndex, nextPointIndex, false, i == vertexSubdivisionsPerChunk - 1);
				}
			}
			
			// Check for errors
			if (genVertexIndex != nbVerticesPerChunk) {
				INVALIDCODE("Problem with terrain mesh generation, generated vertices do not match array size " << genVertexIndex << " != " << nbVerticesPerChunk)
				return;
			}
			if (genIndexIndex != nbIndicesPerChunk) {
				INVALIDCODE("Problem with terrain mesh generation, generated indices do not match array size " << genIndexIndex << " != " << nbIndicesPerChunk)
				return;
			}
			
			std::scoped_lock lock(stateMutex);
			meshGenerated = true;
		}
		
		void FreeBuffers() {
			if (allocated) {
				// PlanetTerrain::vertexBufferPool.Free(vertexBufferAllocation);
				// PlanetTerrain::indexBufferPool.Free(indexBufferAllocation);
				allocated = false;
			}
		}
	
		void AddSubChunks() {
			subChunks.reserve(4);
			
			subChunks.emplace_back(new Chunk{
				planet,
				face,
				level+1,
				topLeft,
				top,
				left,
				center
			});
			subChunks.emplace_back(new Chunk{
				planet,
				face,
				level+1,
				top,
				topRight,
				center,
				right
			});
			subChunks.emplace_back(new Chunk{
				planet,
				face,
				level+1,
				left,
				center,
				bottomLeft,
				bottom
			});
			subChunks.emplace_back(new Chunk{
				planet,
				face,
				level+1,
				center,
				right,
				bottom,
				bottomRight
			});
			
			SortSubChunks();
		}
		
		void Remove() {
			std::scoped_lock lock(stateMutex, subChunksMutex);
			active = false;
			render = false;
			CancelMeshGeneration();
			FreeBuffers();
			if (subChunks.size() > 0) {
				for (auto* subChunk : subChunks) {
					subChunk->Remove();
				}
			}
		}
		
		void Process(Device* device, Queue* transferQueue) {
			std::scoped_lock lock(stateMutex);
			
			// Angle Culling
			bool chunkVisibleByAngle = 
				glm::dot(planet->cameraPos - centerPosLowestPoint, centerPos) > 0.0 ||
				glm::dot(planet->cameraPos - topLeftPosLowestPoint, topLeftPos) > 0.0 ||
				glm::dot(planet->cameraPos - topRightPosLowestPoint, topRightPos) > 0.0 ||
				glm::dot(planet->cameraPos - bottomLeftPosLowestPoint, bottomLeftPos) > 0.0 ||
				glm::dot(planet->cameraPos - bottomRightPosLowestPoint, bottomRightPos) > 0.0 ;
				
			if (chunkVisibleByAngle) {
				active = true;
				
				if (ShouldAddSubChunks()) {
					std::scoped_lock lock(subChunksMutex);
					if (subChunks.size() == 0) AddSubChunks();
					bool allSubchunksGenerated = true;
					for (auto* subChunk : subChunks) {
						subChunk->Process(device, transferQueue);
						if (subChunk->meshShouldGenerate && !subChunk->meshGenerated) {
							allSubchunksGenerated = false;
						}
					}
					if (allSubchunksGenerated) {
						render = false;
					}
				} else {
					if (ShouldRemoveSubChunks()) {
						std::scoped_lock lock(subChunksMutex);
						for (auto* subChunk : subChunks) {
							if (meshGenerated) {
								subChunk->Remove();
							}
						}
						render = true;
					}
					if (!allocated) {
						render = false;
						meshShouldGenerate = true;
						if (meshGenerated) {
							// vertexBufferAllocation = PlanetTerrain::vertexBufferPool.Allocate(device, transferQueue, vertices.data());
							// indexBufferAllocation = PlanetTerrain::indexBufferPool.Allocate(device, transferQueue, indices.data());
							allocated = true;
							render = true;
						} else {
							if (!meshGenerating) {
								Generate();
							}
						}
					}
				}
				
			} else {
				Remove();
			}
			
		}
		
		void BeforeRender(Device* device, Queue* transferQueue) {
			std::scoped_lock lock(stateMutex, subChunksMutex);
			RefreshDistanceFromCamera();
			for (auto* subChunk : subChunks) {
				subChunk->BeforeRender(device, transferQueue);
			}
		}
		
		void SortSubChunks() {
			std::lock_guard lock(subChunksMutex);
			std::sort(subChunks.begin(), subChunks.end(), [](Chunk* a, Chunk* b) -> bool {return /*a->level > b->level ||*/ a->distanceFromCamera < b->distanceFromCamera;});
			for (auto* chunk : subChunks) {
				chunk->SortSubChunks();
			}
		}
		
	};
	
	static v4d::processing::ThreadPoolPriorityQueue<Chunk*>* chunkGenerator;

	std::recursive_mutex chunksMutex;
	std::vector<Chunk*> chunks {};
	
	double GetHeightMap(glm::dvec3 normalizedPos, double triangleSize) {
		double height = 0;
		height += v4d::noise::FastSimplexFractal((normalizedPos*solidRadius/1000000.0), 10)*15000.0;
		height += v4d::noise::FastSimplexFractal((normalizedPos*solidRadius/30000.0), 8)*4000.0;
		if (triangleSize < 200)
			height += v4d::noise::FastSimplexFractal((normalizedPos*solidRadius/50.0), 7)*4.0;
		if (triangleSize < 4)
			height += v4d::noise::FastSimplexFractal(normalizedPos*solidRadius/6.0, 3)*0.5;
		return solidRadius + height;
	}
	
	double GetSolidCirconference() {
		return solidRadius * 2.0 * 3.14159265359;
	}
	
	PlanetTerrain(
		double radius,
		double solidRadius,
		double heightVariation,
		glm::dvec3 absolutePosition
	) : radius(radius),
		solidRadius(solidRadius),
		heightVariation(heightVariation),
		absolutePosition(absolutePosition)
	{
		AddBaseChunks();
	}
	
	~PlanetTerrain() {
		RemoveBaseChunks();
	}
	
	void AddBaseChunks() {
		chunks.reserve(nbBaseChunksPerPlanet);
		
		for (int face = 0; face < 6; ++face) {
			auto [faceDir, topDir, rightDir] = CubeToSphere::GetFaceVectors(face);
			
			topDir /= chunkSubdivisionsPerFace;
			rightDir /= chunkSubdivisionsPerFace;
			
			for (int row = 0; row < chunkSubdivisionsPerFace; ++row) {
				for (int col = 0; col < chunkSubdivisionsPerFace; ++col) {
					double bottomOffset =	row*2 - chunkSubdivisionsPerFace;
					double topOffset =		row*2 - chunkSubdivisionsPerFace + 2;
					double leftOffset =		col*2 - chunkSubdivisionsPerFace;
					double rightOffset =	col*2 - chunkSubdivisionsPerFace + 2;
					
					chunks.emplace_back(new Chunk{
						this,
						face,
						0,
						faceDir + topDir*topOffset 		+ rightDir*leftOffset,
						faceDir + topDir*topOffset 		+ rightDir*rightOffset,
						faceDir + topDir*bottomOffset 	+ rightDir*leftOffset,
						faceDir + topDir*bottomOffset 	+ rightDir*rightOffset
					});
				}
			}
		}
		
		SortChunks();
	}
	
	void RemoveBaseChunks() {
		for (auto* chunk : chunks) {
			delete chunk;
		}
		chunks.clear();
	}

	void SortChunks() {
		std::lock_guard lock(chunksMutex);
		std::sort(chunks.begin(), chunks.end(), [](Chunk* a, Chunk* b) -> bool {return /*a->level > b->level ||*/ a->distanceFromCamera < b->distanceFromCamera;});
		for (auto* chunk : chunks) {
			chunk->SortSubChunks();
		}
	}
	
	void CleanupOldChunks() {
		std::lock_guard lock(chunksMutex);
		
		for (auto* chunk : chunks) {
			chunk->Cleanup();
		}
	}
	
	void Optimize() {
		// Optimize only when no chunk is being generated, camera moved at least x distance and not more than once every x seconds
		if ((PlanetTerrain::chunkGenerator && PlanetTerrain::chunkGenerator->Count() > 0) || glm::distance(cameraPos, lastOptimizePosition) < chunkOptimizationMinMoveDistance || lastOptimizeTime.GetElapsedSeconds() < chunkOptimizationMinTimeInterval)
			return;
		
		// reset 
		lastOptimizePosition = cameraPos;
		lastOptimizeTime.Reset();
		
		// Optimize
		CleanupOldChunks();
		SortChunks();
	}

	static void CollectGarbage(Device* device) {
		// Collect garbage not more than once every x seconds
		if (lastGarbageCollectionTime.GetElapsedSeconds() < garbageCollectionInterval)
			return;
		
		// reset
		lastGarbageCollectionTime.Reset();
		
		// // Collect garbage
		// vertexBufferPool.CollectGarbage(device);
		// indexBufferPool.CollectGarbage(device);
	}
	
	void RefreshMatrix() {
		matrix = glm::rotate(glm::translate(glm::dmat4(1), absolutePosition), rotationAngle, rotationAxis);
	}
	
};

// Buffer pools
v4d::Timer PlanetTerrain::lastGarbageCollectionTime {true};
// PlanetTerrain::ChunkVertexBufferPool PlanetTerrain::vertexBufferPool {};
// PlanetTerrain::ChunkIndexBufferPool PlanetTerrain::indexBufferPool {};

// // Chunk Generator thread pool
// v4d::processing::ThreadPoolPriorityQueue<PlanetTerrain::Chunk*>* PlanetTerrain::chunkGenerator = nullptr;
