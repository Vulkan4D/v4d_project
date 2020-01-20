#pragma once
#include <v4d.h>

#include "../../incubator_galaxy4d/Noise.hpp"

using namespace v4d::graphics;
using namespace v4d::graphics::vulkan;

// #define SPHERIFY_CUBE_BY_NORMALIZE // otherwise use technique shown here : http://mathproofs.blogspot.com/2005/07/mapping-cube-to-sphere.html

struct PlanetaryTerrain {
	
	#pragma region Constructor arguments
	double radius; // top of atmosphere (maximum radius)
	double solidRadius; // standard radius of solid surface (AKA sea level, surface height can go below or above)
	double heightVariation; // half the total variation (surface height is +- heightVariation)
	glm::dvec3 absolutePosition; // position of planet relative to world (star system center)
	#pragma endregion
	
	float lightIntensity = 0;
	LightSource lightSource {};
	
	#pragma region Graphics configuration
	static const int chunkSubdivisionsPerFace = 16;
	static const int vertexSubdivisionsPerChunk = 16;
	static constexpr float targetVertexSeparationInMeters = 1.0f; // approximative vertex separation in meters for the most precise level of detail
	static const size_t chunkGeneratorNbThreads = 4;
	static const int nbChunksPerBufferPool = 128;
	#pragma endregion

	#pragma region Calculated constants
	static const int nbChunksPerFace = chunkSubdivisionsPerFace * chunkSubdivisionsPerFace;
	static const int nbBaseChunksPerPlanet = nbChunksPerFace * 6;
	static const int nbVerticesPerChunk = (vertexSubdivisionsPerChunk+1) * (vertexSubdivisionsPerChunk+1);
	static const int nbIndicesPerChunk = (vertexSubdivisionsPerChunk) * (vertexSubdivisionsPerChunk) * 6;
	#pragma endregion

	struct Vertex {
		glm::vec4 pos;
		glm::vec4 normal;
		glm::vec2 uv;
		
		static std::vector<VertexInputAttributeDescription> GetInputAttributes() {
			return {
				{0, offsetof(Vertex, pos), VK_FORMAT_R32G32B32A32_SFLOAT},
				{1, offsetof(Vertex, normal), VK_FORMAT_R32G32B32A32_SFLOAT},
				{2, offsetof(Vertex, uv), VK_FORMAT_R32G32_SFLOAT},
			};
		}
	};

	enum FACE : int {
		FRONT,
		BACK,
		RIGHT,
		LEFT,
		TOP,
		BOTTOM
	};

	// Buffer pools
	typedef DeviceLocalBufferPool<VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, nbVerticesPerChunk*sizeof(Vertex), nbChunksPerBufferPool> ChunkVertexBufferPool;
	typedef DeviceLocalBufferPool<VK_BUFFER_USAGE_INDEX_BUFFER_BIT, nbIndicesPerChunk*sizeof(uint32_t), nbChunksPerBufferPool> ChunkIndexBufferPool;
	static ChunkVertexBufferPool vertexBufferPool;
	static ChunkIndexBufferPool indexBufferPool;

	glm::dvec3 cameraPos {0};
	
	struct Chunk {
	
		#pragma region Constructor arguments
		PlanetaryTerrain* planet;
		FACE face;
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
		double chunkSize = 0;
		double heightAtCenter = 0;
		std::atomic<double> distanceFromCamera = 0;
		#pragma endregion
		
		#pragma region States
		std::atomic<bool> active = false;
		std::atomic<bool> render = false;
		std::atomic<bool> allocated = false;
		std::atomic<bool> meshShouldGenerate = false;
		std::atomic<bool> meshGenerated = false;
		std::atomic<bool> meshGenerating = false;
		#pragma endregion
		
		#pragma region Data
		std::array<Vertex, nbVerticesPerChunk> vertices {};
		std::array<uint32_t, nbIndicesPerChunk> indices {};
		std::vector<Chunk*> subChunks {};
		std::mutex generatorMutex;
		#pragma endregion
		
		#pragma region Buffers
		BufferPoolAllocation vertexBufferAllocation {};
		BufferPoolAllocation indexBufferAllocation {};
		#pragma endregion
		
		#pragma region Testing
		glm::vec4 testColor {1,1,1,1};
		#pragma endregion
		
		static glm::dvec3 Spherify(glm::dvec3 point) {
			#ifdef SPHERIFY_CUBE_BY_NORMALIZE
				point = glm::normalize(point);
			#else
				// http://mathproofs.blogspot.com/2005/07/mapping-cube-to-sphere.html
				double	x2 = point.x * point.x,
						y2 = point.y * point.y,
						z2 = point.z * point.z;
				point.x *= glm::sqrt(1.0 - y2 / 2.0 - z2 / 2.0 + y2 * z2 / 3.0);
				point.y *= glm::sqrt(1.0 - z2 / 2.0 - x2 / 2.0 + z2 * x2 / 3.0);
				point.z *= glm::sqrt(1.0 - x2 / 2.0 - y2 / 2.0 + x2 * y2 / 3.0);
			#endif
			return point;
		}
	
		bool IsLastLevel() {
			return (chunkSize/vertexSubdivisionsPerChunk < targetVertexSeparationInMeters*1.9);
		}
		
		bool ShouldAddSubChunks() {
			if (IsLastLevel()) return false;
			return chunkSize / distanceFromCamera > 1.0;
		}
		
		bool ShouldRemoveSubChunks() {
			if (IsLastLevel()) return false;
			return chunkSize / distanceFromCamera < 0.5;
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
			if (distanceFromCamera < chunkSize/2.0)
				distanceFromCamera = glm::max((double)distanceFromCamera, chunkSize/2.0);
		}
		
		Chunk(PlanetaryTerrain* planet, int face, int level, glm::dvec3 topLeft, glm::dvec3 topRight, glm::dvec3 bottomLeft, glm::dvec3 bottomRight)
		: planet(planet), face((FACE)face), level(level), topLeft(topLeft), topRight(topRight), bottomLeft(bottomLeft), bottomRight(bottomRight) {
			
			chunkSize = planet->GetSolidCirconference() / 4.0 / chunkSubdivisionsPerFace / glm::pow(2, level);
			center = (topLeft + topRight + bottomLeft + bottomRight) / 4.0;
			top = (topLeft + topRight) / 2.0;
			left = (topLeft + bottomLeft) / 2.0;
			right = (topRight + bottomRight) / 2.0;
			bottom = (bottomLeft + bottomRight) / 2.0;
			centerPos = Spherify(center);
			heightAtCenter = planet->GetHeightMap(centerPos);
			centerPos = glm::round(centerPos * heightAtCenter);
			topLeftPos = Spherify(topLeft);
			topLeftPos *= planet->GetHeightMap(topLeftPos);
			topRightPos = Spherify(topRight);
			topRightPos *= planet->GetHeightMap(topRightPos);
			bottomLeftPos = Spherify(bottomLeft);
			bottomLeftPos *= planet->GetHeightMap(bottomLeftPos);
			bottomRightPos = Spherify(bottomRight);
			bottomRightPos *= planet->GetHeightMap(bottomRightPos);
			centerPosLowestPoint = Spherify(center) * (planet->solidRadius - glm::max(chunkSize/2.0, planet->heightVariation*2.0));
			topLeftPosLowestPoint = Spherify(topLeft) * (planet->solidRadius - glm::max(chunkSize/2.0, planet->heightVariation*2.0));
			topRightPosLowestPoint = Spherify(topRight) * (planet->solidRadius - glm::max(chunkSize/2.0, planet->heightVariation*2.0));
			bottomLeftPosLowestPoint = Spherify(bottomLeft) * (planet->solidRadius - glm::max(chunkSize/2.0, planet->heightVariation*2.0));
			bottomRightPosLowestPoint = Spherify(bottomRight) * (planet->solidRadius - glm::max(chunkSize/2.0, planet->heightVariation*2.0));
			
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
		
		void GenerateAsync() {
			static v4d::processing::ThreadPoolPriorityQueue<Chunk*> chunkGenerator ([](Chunk* chunk){
				std::lock_guard lock(chunk->generatorMutex);
				chunk->Generate();
				chunk->meshGenerating = false;
			}, [](Chunk* a, Chunk* b) {
				return a->distanceFromCamera > b->distanceFromCamera;
			});
			if (meshGenerating) return;
			std::lock_guard lock(generatorMutex);
			meshGenerating = true;
			chunkGenerator.Enqueue(this);
			chunkGenerator.RunThreads(chunkGeneratorNbThreads);
		}
		
		void CancelMeshGeneration() {
			meshShouldGenerate = false;
		}
		
		int genRow = 0;
		int genCol = 0;
		int genIndexIndex = 0;
		void Generate() {
			if (!meshShouldGenerate) return;
			
			auto [faceDir, topDir, rightDir] = GetFaceVectors(face);
			
			while (genRow <= vertexSubdivisionsPerChunk) {
				while (genCol <= vertexSubdivisionsPerChunk) {
					
					uint32_t topLeftIndex = (vertexSubdivisionsPerChunk+1) * genRow + genCol;
					
					glm::dvec3 topOffset = glm::mix(topLeft - center, bottomLeft - center, double(genRow)/vertexSubdivisionsPerChunk);
					glm::dvec3 leftOffset =	glm::mix(topLeft - center, topRight - center, double(genCol)/vertexSubdivisionsPerChunk);
					
					glm::dvec3 pos = Spherify(center + topDir*topOffset + rightDir*leftOffset);
					
					vertices[topLeftIndex].pos = glm::vec4(pos * planet->GetHeightMap(pos) - centerPos, 0);
					vertices[topLeftIndex].normal = glm::vec4(Spherify(pos), 0);
					vertices[topLeftIndex].uv = glm::vec2(genCol, genRow) / float(vertexSubdivisionsPerChunk);
					
					if (genRow < vertexSubdivisionsPerChunk && genCol < vertexSubdivisionsPerChunk) {
						uint32_t topRightIndex = topLeftIndex+1;
						uint32_t bottomLeftIndex = (vertexSubdivisionsPerChunk+1) * (genRow+1) + genCol;
						uint32_t bottomRightIndex = bottomLeftIndex+1;
						switch (face) {
							case FRONT:
							case LEFT:
							case BOTTOM:
								indices[genIndexIndex++] = topLeftIndex;
								indices[genIndexIndex++] = bottomLeftIndex;
								indices[genIndexIndex++] = bottomRightIndex;
								indices[genIndexIndex++] = topLeftIndex;
								indices[genIndexIndex++] = bottomRightIndex;
								indices[genIndexIndex++] = topRightIndex;
								break;
							case BACK:
							case RIGHT:
							case TOP:
								indices[genIndexIndex++] = topLeftIndex;
								indices[genIndexIndex++] = bottomRightIndex;
								indices[genIndexIndex++] = bottomLeftIndex;
								indices[genIndexIndex++] = topLeftIndex;
								indices[genIndexIndex++] = topRightIndex;
								indices[genIndexIndex++] = bottomRightIndex;
								break;
						}
					}
					
					++genCol;
					
					if (!meshShouldGenerate) return;
				}
				
				genCol = 0;
				++genRow;
			}
			
			meshGenerated = true;
		}
		
		void FreeBuffers() {
			if (allocated) {
				PlanetaryTerrain::vertexBufferPool.Free(vertexBufferAllocation);
				PlanetaryTerrain::indexBufferPool.Free(indexBufferAllocation);
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
		}
		
		void Remove() {
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
		
		void Process() {
			
		}
		
		void BeforeRender(Device* device, Queue* transferQueue) {
			RefreshDistanceFromCamera();
			
			// Angle Culling
			bool chunkVisibleByAngle = 
				glm::dot(planet->cameraPos - centerPosLowestPoint, centerPos) > 0.0 ||
				glm::dot(planet->cameraPos - topLeftPosLowestPoint, topLeftPos) > 0.0 ||
				glm::dot(planet->cameraPos - topRightPosLowestPoint, topRightPos) > 0.0 ||
				glm::dot(planet->cameraPos - bottomLeftPosLowestPoint, bottomLeftPos) > 0.0 ||
				glm::dot(planet->cameraPos - bottomRightPosLowestPoint, bottomRightPos) > 0.0 ;
			
			if (chunkVisibleByAngle) {
				active = true;
				
				// Green = last level (most precise)
				if (IsLastLevel())
					testColor = glm::vec4{0,1,0,1};
				else
					testColor = glm::vec4{1,1,1,1};
				
				if (ShouldAddSubChunks()) {
					if (subChunks.size() == 0) AddSubChunks();
					bool allSubchunksGenerated = true;
					for (auto* subChunk : subChunks) {
						subChunk->BeforeRender(device, transferQueue);
						if (subChunk->meshShouldGenerate && !subChunk->meshGenerated) {
							allSubchunksGenerated = false;
						}
					}
					if (allSubchunksGenerated) {
						render = false;
					}
				} else {
					if (ShouldRemoveSubChunks()) {
						for (auto* subChunk : subChunks) {
							subChunk->Remove();
						}
						render = true;
					}
					if (!allocated) {
						render = false;
						meshShouldGenerate = true;
						if (meshGenerated) {
							vertexBufferAllocation = PlanetaryTerrain::vertexBufferPool.Allocate(device, transferQueue, vertices.data());
							indexBufferAllocation = PlanetaryTerrain::indexBufferPool.Allocate(device, transferQueue, indices.data());
							allocated = true;
							render = true;
						} else {
							if (!meshGenerating) {
								GenerateAsync();
							}
						}
					}
				}
				
			} else {
				Remove();
			}
		}
		
	};

	std::vector<Chunk*> chunks {};
	
	double GetHeightMap(glm::dvec3 normalizedPos) {
		double height = v4d::noise::SimplexFractal(normalizedPos*200.0, 16);
		return solidRadius + height*heightVariation;
	}
	
	double GetSolidCirconference() {
		return solidRadius * 2.0 * 3.14159265359;
	}
	
	static constexpr std::tuple<glm::dvec3, glm::dvec3, glm::dvec3> GetFaceVectors(int face) {
		glm::dvec3 dir {0};
		glm::dvec3 top {0};
		glm::dvec3 right {0};
		switch (face) {
			case FRONT:
				dir = glm::dvec3(0, 0, 1);
				top = glm::dvec3(0, 1, 0);
				right = glm::dvec3(1, 0, 0);
				break;
			case BACK:
				dir = glm::dvec3(0, 0, -1);
				top = glm::dvec3(0, 1, 0);
				right = glm::dvec3(-1, 0, 0);
				break;
			case RIGHT:
				dir = glm::dvec3(1, 0, 0);
				top = glm::dvec3(0, 1, 0);
				right = glm::dvec3(0, 0, -1);
				break;
			case LEFT:
				dir = glm::dvec3(-1, 0, 0);
				top = glm::dvec3(0, -1, 0);
				right = glm::dvec3(0, 0, -1);
				break;
			case TOP:
				dir = glm::dvec3(0, 1, 0);
				top = glm::dvec3(0, 0, 1);
				right = glm::dvec3(-1, 0, 0);
				break;
			case BOTTOM:
				dir = glm::dvec3(0, -1, 0);
				top = glm::dvec3(0, 0, -1);
				right = glm::dvec3(-1, 0, 0);
				break;
		}
		return {dir, top, right};
	}
	
	PlanetaryTerrain(
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
	
	~PlanetaryTerrain() {
		RemoveBaseChunks();
	}
	
	void AddBaseChunks() {
		chunks.reserve(nbBaseChunksPerPlanet);
		
		for (int face = 0; face < 6; ++face) {
			auto [faceDir, topDir, rightDir] = GetFaceVectors(face);
			
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
	}
	
	void RemoveBaseChunks() {
		for (auto* chunk : chunks) {
			delete chunk;
		}
		chunks.clear();
	}
	
};

// Buffer pools
PlanetaryTerrain::ChunkVertexBufferPool PlanetaryTerrain::vertexBufferPool {};
PlanetaryTerrain::ChunkIndexBufferPool PlanetaryTerrain::indexBufferPool {};
