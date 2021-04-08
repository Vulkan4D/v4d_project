#pragma once
#include <v4d.h>
#include "utilities/io/Logger.h"
#include "utilities/io/BinaryFileStream.h"
#include "utilities/graphics/Mesh.hpp"
#include "utilities/graphics/RenderableGeometryEntity.h"

#include <condition_variable>

#include "CubeToSphere.hpp"
// #include "Noise.hpp"

#include "v4d/modules/V4D_raytracing/camera_options.hh"
#include "TerrainGeneratorCommon.hh"

using namespace v4d::graphics;
using namespace v4d::graphics::vulkan;
using namespace v4d::graphics::Mesh;

#define PLANET_CHUNK_CACHE_ENABLE

struct PlanetTerrain {
	
	#pragma region Constructor arguments
	double radius; // top of atmosphere (maximum radius)
	double solidRadius; // standard radius of solid surface (AKA sea level, surface height can go below or above)
	double heightVariation; // half the total variation (surface height is +- heightVariation)
	std::weak_ptr<v4d::graphics::RenderableGeometryEntity> atmosphere;
	float atmosphereDensityFactor = 1.0f;
	glm::vec3 atmosphereColor = glm::vec3(0.85, 0.8, 1.0);
	#pragma endregion
	
	glm::dmat4 matrix {1};
	
	#pragma region Graphics configuration
	static const int chunkSubdivisionsPerFace = 5;
	static const int vertexSubdivisionsPerChunk = 64; // low=64, high=128
	static float targetVertexSeparationInMeters; // approximative vertex separation in meters for the most precise level of detail (minimum is 0.02 = 2 cm)
	static constexpr double garbageCollectionInterval = 20; // seconds
	static constexpr double chunkOptimizationMinMoveDistance = 500; // meters
	static constexpr double chunkOptimizationMinTimeInterval = 10; // seconds
	static constexpr int CHUNK_CACHE_VERSION = 5;
	static const bool useSkirts = true;
	static bool generateAabbChunks;
	#pragma endregion

	#pragma region Calculated constants
	static const int nbChunksPerFace = chunkSubdivisionsPerFace * chunkSubdivisionsPerFace;
	static const int nbBaseChunksPerPlanet = nbChunksPerFace * 6;
	static const int nbVerticesPerChunk = (vertexSubdivisionsPerChunk+1) * (vertexSubdivisionsPerChunk+1) + (useSkirts? (vertexSubdivisionsPerChunk * 4) : 0);
	static const int nbIndicesPerChunk = vertexSubdivisionsPerChunk*vertexSubdivisionsPerChunk*6 + (useSkirts? (vertexSubdivisionsPerChunk * 4 * 6) : 0);
	static const int nbAabbPerChunk = vertexSubdivisionsPerChunk * vertexSubdivisionsPerChunk;
	#pragma endregion

	// Camera
	glm::dvec3 cameraPos {0};
	double cameraAltitudeAboveTerrain = 0;
	float chunkSubdivisionDistanceFactor = 1.0; // low=0.5, normal=1.0, medium=1.5, high=4.0
	
	static Device* renderingDevice;public:
	static std::unordered_map<uint64_t, std::shared_ptr<PlanetTerrain>> terrains;
	
	// Cache
	glm::dvec3 lastOptimizePosition {0};
	v4d::Timer lastOptimizeTime {true};
	static v4d::Timer lastGarbageCollectionTime;
	// #ifdef _DEBUG
		int totalChunkTimeNb = 0;
		float totalChunkTime = 0;
	// #endif
		
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
		
		bool aabb;
		
		#pragma region Caching
		// Cube positions (-1.0 to +1.0)
		glm::dvec3 top {0};
		glm::dvec3 left {0};
		glm::dvec3 right {0};
		glm::dvec3 bottom {0};
		glm::dvec3 center {0};
		
		// true positions on planet
		glm::dmat4 transform {1};
		glm::dmat4 inverseTransform {1};
		glm::dmat3 upDirTransform {1};
		glm::dvec3 centerPos {0};
		glm::dvec3 topLeftPos {0};
		glm::dvec3 topRightPos {0};
		glm::dvec3 bottomLeftPos {0};
		glm::dvec3 bottomRightPos {0};
		// glm::dvec3 centerPosLowest {0};
		// glm::dvec3 centerPosHighest {0};
		glm::dvec3 topLeftPosLowest {0};
		glm::dvec3 topRightPosLowest {0};
		glm::dvec3 bottomLeftPosLowest {0};
		glm::dvec3 bottomRightPosLowest {0};
		// glm::dvec3 topLeftPosHighest {0};
		// glm::dvec3 topRightPosHighest {0};
		// glm::dvec3 bottomLeftPosHighest {0};
		// glm::dvec3 bottomRightPosHighest {0};
		
		float uvMult = 1;
		float uvOffsetX = 0;
		float uvOffsetY = 0;
		double chunkSize = 0;
		double triangleSize = 0;
		double heightAtCenter = 0;
		double lowestAltitude = 0;
		double highestAltitude = 0;
		double boundingDistance = 0;
		std::atomic<double> distanceFromCamera = 0;
		#pragma endregion
		
		#pragma region States
		std::atomic<bool> active = false;
		std::atomic<bool> render = false;

		std::atomic<bool> meshEnqueuedForGeneration = false;
		std::atomic<bool> meshGenerating = false;
		std::atomic<bool> meshGenerated = false;

		// std::recursive_mutex stateMutex;
		std::recursive_mutex subChunksMutex;
		#pragma endregion
		
		#pragma region Data
		std::vector<Chunk*> subChunks {};
		std::shared_ptr<RenderableGeometryEntity> entity = nullptr;
		std::recursive_mutex generatorMutex;
		std::vector<uint16_t> colliderIndices {};
		#pragma endregion
		
		bool IsLastLevel() {
			return (triangleSize < targetVertexSeparationInMeters*1.9);
		}
		
		bool ShouldAddSubChunks() {
			if (IsLastLevel()) 
				return false;
			return distanceFromCamera < planet->chunkSubdivisionDistanceFactor*chunkSize;
		}
		
		bool ShouldRemoveSubChunks() {
			return distanceFromCamera > planet->chunkSubdivisionDistanceFactor*chunkSize * 1.1 || (triangleSize < targetVertexSeparationInMeters);
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
			
			lowestAltitude = planet->solidRadius - planet->heightVariation;
			highestAltitude = planet->solidRadius + planet->heightVariation;
			
			centerPos = CubeToSphere::Spherify(center, face);
			heightAtCenter = planet->GetHeightMap(centerPos, triangleSize);
			centerPos = glm::round(centerPos * heightAtCenter);
			
			topLeftPos = CubeToSphere::Spherify(topLeft, face);
			topLeftPos *= planet->GetHeightMap(topLeftPos, triangleSize);
			topRightPos = CubeToSphere::Spherify(topRight, face);
			topRightPos *= planet->GetHeightMap(topRightPos, triangleSize);
			bottomLeftPos = CubeToSphere::Spherify(bottomLeft, face);
			bottomLeftPos *= planet->GetHeightMap(bottomLeftPos, triangleSize);
			bottomRightPos = CubeToSphere::Spherify(bottomRight, face);
			bottomRightPos *= planet->GetHeightMap(bottomRightPos, triangleSize);
			
			topLeftPosLowest = glm::normalize(topLeftPos) * lowestAltitude;
			topRightPosLowest = glm::normalize(topRightPos) * lowestAltitude;
			bottomLeftPosLowest = glm::normalize(bottomLeftPos) * lowestAltitude;
			bottomRightPosLowest = glm::normalize(bottomRightPos) * lowestAltitude;
			
			// topLeftPosHighest = glm::normalize(topLeftPos) * highestAltitude;
			// topRightPosHighest = glm::normalize(topRightPos) * highestAltitude;
			// bottomLeftPosHighest = glm::normalize(bottomLeftPos) * highestAltitude;
			// bottomRightPosHighest = glm::normalize(bottomRightPos) * highestAltitude;
		
			RefreshDistanceFromCamera();
			
			inverseTransform = glm::lookAt(centerPos, (topLeftPos + topRightPos)/2.0, glm::cross(glm::normalize(topRightPos-topLeftPos), glm::normalize(bottomLeftPos-topLeftPos)));
			transform = glm::inverse(inverseTransform);
			upDirTransform = glm::transpose(glm::inverse(glm::dmat3(transform)));
			
			aabb = PlanetTerrain::generateAabbChunks;
		}
		
		~Chunk() {
			Remove(true);
		}
		
		uint32_t topLeftVertexIndex = 0;
		uint32_t topRightVertexIndex = vertexSubdivisionsPerChunk;
		uint32_t bottomLeftVertexIndex = (vertexSubdivisionsPerChunk+1) * vertexSubdivisionsPerChunk;
		uint32_t bottomRightVertexIndex = (vertexSubdivisionsPerChunk+1) * vertexSubdivisionsPerChunk + vertexSubdivisionsPerChunk;
		
		std::string GetChunkId() const {
			return std::string(aabb?"aabb_":"mesh_") + std::to_string((uint32_t)level) + "_" + std::to_string((int64_t)glm::round(centerPos.x)) + "_" + std::to_string((int64_t)glm::round(centerPos.y)) + "_" + std::to_string((int64_t)glm::round(centerPos.z));
		}
		
		void Generate() {
			/* 
					Example using vertexSubdivisionsPerChunk = 4 (sub)
						[Vertex indices]
						{skirt vertex indices}
					
						col 0		col 1		col 2		col 3
						
									{15}		{14}		{13}
					{0}	 _______________________________________________	  {12}
			row 0		|\[0]		|\[1]		|\[2]		|\[3]		|[4]
						|  \		|  \		|  \		|  \		|
						|	 \		|	 \		|	 \		|	 \		|
						|	   \	|	   \	|	   \	|	   \	|
						|		 \	|		 \	|		 \	|		 \	|
						|__________\|__________\|__________\|__________\|
			row 1	{1}	|\[5]		|\[6]		|\[7]		|\[8]		|[9]	{11}
						|  \		|  \		|  \		|  \		|
						|	 \		|	 \		|	 \		|	 \		|
						|	   \	|	   \	|	   \	|	   \	|
						|		 \	|		 \	|		 \	|		 \	|
						|__________\|__________\|__________\|__________\|
			row 2	{2}	|\[10]		|\[11]		|\[12]		|\[13]		|[14]	{10}
						|  \		|  \		|  \		|  \		|
						|	 \		|	 \		|	 \		|	 \		|
						|	   \	|	   \	|	   \	|	   \	|
						|		 \	|		 \	|		 \	|		 \	|
						|__________\|__________\|__________\|__________\|
			row 3	{3}	|\[15]		|\[16]		|\[17]		|\[18]		|[19]	{9}
						|  \		|  \		|  \		|  \		|
						|	 \		|	 \		|	 \		|	 \		|
						|	   \	|	   \	|	   \	|	   \	|
						|		 \	|		 \	|		 \	|		 \	|
						|__________\|__________\|__________\|__________\|
						[20]		[21]		[22]		[23]		[24]
					{4}														  {8}
									{5}			{6}			{7}
				
				
			Vertex Index formulas :
				Per Col & Row (in the generate loop)
					VertexIndex = (sub + 1) * row + col
					TopLeft = VertexIndex
					TopRight = TopLeft + 1
					BottomLeft = (sub + 1) * (row + 1) + col
					BottomRight = BottomLeft + 1
				
				Per Chunk
					TopLeftMost = 0
					TopRightMost = sub
					BottomLeftMost = sub * (sub + 1)
					BottomRightMost = sub * (sub + 2)
			*/
		
			// #ifdef _DEBUG
				auto timer = v4d::Timer(true);
			// #endif
			
			if (!meshGenerating) return;
	
			// Prepare object for mesh generation
			auto entityLock = RenderableGeometryEntity::GetLock();
			if (entity) entity->Destroy();
			entity = RenderableGeometryEntity::Create(THIS_MODULE);
			
			if (aabb) {
				
				entity->Allocate(renderingDevice, "V4D_andromeda:planet.terrain.aabb");
				entity->rayTracingMask = RAY_TRACED_ENTITY_TERRAIN;
				auto buffersWriteLock = entity->GetBuffersWriteLock();
					auto aabbVertices = entity->Add_proceduralVertexAABB()->AllocateBuffersCount(renderingDevice, nbAabbPerChunk);
					auto vertexNormals = entity->Add_meshVertexNormal()->AllocateBuffersCount(renderingDevice, nbAabbPerChunk);
					auto vertexColors = entity->Add_meshVertexColorF32()->AllocateBuffersCount(renderingDevice, nbAabbPerChunk);
					entity->generator = [](auto* entity, Device*){entity->generated = false;};
					entity->SetWorldTransform(planet->matrix * transform);
				entityLock.unlock();
			
				#ifdef PLANET_CHUNK_CACHE_ENABLE
				// Cache file
				std::string chunkId = GetChunkId();
				v4d::io::BinaryFileStream cacheFile (std::string(V4D_MODULE_CACHE_PATH(THIS_MODULE, "chunks/")) + chunkId + ".binary", 1024*1024);
				constexpr size_t cacheFileSize 
									= sizeof(CHUNK_CACHE_VERSION)
									+ nbAabbPerChunk * sizeof(Mesh::ProceduralVertexAABB)
									+ nbAabbPerChunk * sizeof(Mesh::VertexNormal)
									+ nbAabbPerChunk * sizeof(Mesh::VertexColor<glm::f32>)
									// + 24 * sizeof(uint16_t) + sizeof(uint8_t) // geometry->simplifiedMeshIndices
									+ sizeof(lowestAltitude)
									+ sizeof(highestAltitude)
									+ sizeof(topLeftPosLowest)
									+ sizeof(topRightPosLowest)
									+ sizeof(bottomLeftPosLowest)
									+ sizeof(bottomRightPosLowest)
									// + sizeof(topLeftPosHighest)
									// + sizeof(topRightPosHighest)
									// + sizeof(bottomLeftPosHighest)
									// + sizeof(bottomRightPosHighest)
									+ sizeof(boundingDistance)
				;
				if (cacheFile.GetSize() == cacheFileSize && cacheFile.Read<uint32_t>() == CHUNK_CACHE_VERSION) {
					{// Load from cache file
						cacheFile.LockReadWrite();
							cacheFile.ReadBytes(reinterpret_cast<byte*>(aabbVertices), nbAabbPerChunk*sizeof(Mesh::ProceduralVertexAABB));
							cacheFile.ReadBytes(reinterpret_cast<byte*>(vertexNormals), nbAabbPerChunk*sizeof(Mesh::VertexNormal));
							cacheFile.ReadBytes(reinterpret_cast<byte*>(vertexColors), nbAabbPerChunk*sizeof(Mesh::VertexColor<glm::f32>));
							// cacheFile >> colliderIndices;
							cacheFile >> lowestAltitude;
							cacheFile >> highestAltitude;
							cacheFile >> topLeftPosLowest;
							cacheFile >> topRightPosLowest;
							cacheFile >> bottomLeftPosLowest;
							cacheFile >> bottomRightPosLowest;
							// cacheFile >> topLeftPosHighest;
							// cacheFile >> topRightPosHighest;
							// cacheFile >> bottomLeftPosHighest;
							// cacheFile >> bottomRightPosHighest;
							cacheFile >> boundingDistance;
						cacheFile.UnlockReadWrite();
					}
				} else 
				#endif
				{
					{// Generate
						glm::dvec4 vertexPositions[nbVerticesPerChunk]; // local vertex position and absolute altitude from center of planet
						
						int genRow = 0;
						int genCol = 0;
						
						// Fetch information for generating this chunk
						auto [faceDir, topDir, rightDir] = GetFaceVectors(face);
						double topSign = topDir.x + topDir.y + topDir.z;
						double rightSign = rightDir.x + rightDir.y + rightDir.z;
						
						// Generate terrain mesh
						while (genRow <= vertexSubdivisionsPerChunk) {
							while (genCol <= vertexSubdivisionsPerChunk) {
								uint32_t currentIndex = (vertexSubdivisionsPerChunk+1) * genRow + genCol;
								
								glm::dvec3 topOffset = glm::mix(topLeft - center, bottomLeft - center, double(genRow)/vertexSubdivisionsPerChunk);
								glm::dvec3 rightOffset = glm::mix(topLeft - center, topRight - center, double(genCol)/vertexSubdivisionsPerChunk);
								
								// position
								glm::dvec3 pos = CubeToSphere::Spherify(center + topDir*topOffset + rightDir*rightOffset, face);
								double altitude = planet->GetHeightMap(pos, triangleSize);
								glm::dvec3 posOnChunk = inverseTransform * glm::dvec4(pos * altitude, 1);
								vertexPositions[currentIndex] = glm::dvec4(posOnChunk, altitude);
								
								// Bounding distance
								boundingDistance = std::max(boundingDistance, glm::length(posOnChunk));
								
								// Altitude bounds
								lowestAltitude = std::min(lowestAltitude, altitude);
								highestAltitude = std::max(highestAltitude, altitude);

								++genCol;
								// if (!meshGenerating) return;
							}
							
							genCol = 0;
							++genRow;
						}
						
						for (int row = 0; row < vertexSubdivisionsPerChunk; ++row) {
							for (int col = 0; col < vertexSubdivisionsPerChunk; ++col) {
								const uint32_t aabbVertexIndex = vertexSubdivisionsPerChunk * row + col;
								ProceduralVertexAABB& aabbVertex = aabbVertices[aabbVertexIndex];
								VertexNormal& aabbVertexNormal = vertexNormals[aabbVertexIndex];
								VertexColorF32& aabbVertexColor = vertexColors[aabbVertexIndex];
								
								const uint32_t topLeftPositionIndex = (vertexSubdivisionsPerChunk + 1) * row + col;
								const uint32_t topRightPositionIndex = topLeftPositionIndex + 1;
								const uint32_t bottomLeftPositionIndex = (vertexSubdivisionsPerChunk + 1) * (row + 1) + col;
								const uint32_t bottomRightPositionIndex = bottomLeftPositionIndex + 1;
								glm::dvec4& top_left = vertexPositions[topLeftPositionIndex];
								glm::dvec4& top_right = vertexPositions[topRightPositionIndex];
								glm::dvec4& bottom_left = vertexPositions[bottomLeftPositionIndex];
								glm::dvec4& bottom_right = vertexPositions[bottomRightPositionIndex];
								
								aabbVertex.aabbMin.x = glm::min(top_left.x, top_right.x);
								aabbVertex.aabbMin.x = glm::min(aabbVertex.aabbMin.x, (float)bottom_left.x);
								aabbVertex.aabbMin.x = glm::min(aabbVertex.aabbMin.x, (float)bottom_right.x);
								aabbVertex.aabbMax.x = glm::max(top_left.x, top_right.x);
								aabbVertex.aabbMax.x = glm::max(aabbVertex.aabbMax.x, (float)bottom_left.x);
								aabbVertex.aabbMax.x = glm::max(aabbVertex.aabbMax.x, (float)bottom_right.x);
								
								aabbVertex.aabbMin.y = glm::min(top_left.y, top_right.y);
								aabbVertex.aabbMin.y = glm::min(aabbVertex.aabbMin.y, (float)bottom_left.y);
								aabbVertex.aabbMin.y = glm::min(aabbVertex.aabbMin.y, (float)bottom_right.y);
								aabbVertex.aabbMax.y = glm::max(top_left.y, top_right.y);
								aabbVertex.aabbMax.y = glm::max(aabbVertex.aabbMax.y, (float)bottom_left.y);
								aabbVertex.aabbMax.y = glm::max(aabbVertex.aabbMax.y, (float)bottom_right.y);
								
								aabbVertex.aabbMin.z = glm::min(top_left.z, top_right.z);
								aabbVertex.aabbMin.z = glm::min(aabbVertex.aabbMin.z, (float)bottom_left.z);
								aabbVertex.aabbMin.z = glm::min(aabbVertex.aabbMin.z, (float)bottom_right.z);
								aabbVertex.aabbMax.z = glm::max(top_left.z, top_right.z);
								aabbVertex.aabbMax.z = glm::max(aabbVertex.aabbMax.z, (float)bottom_left.z);
								aabbVertex.aabbMax.z = glm::max(aabbVertex.aabbMax.z, (float)bottom_right.z);
								
								// Normal
								auto tangentX = glm::normalize(glm::dvec3(top_right) - glm::dvec3(top_left));
								auto tangentY = glm::normalize(glm::dvec3(top_left) - glm::dvec3(bottom_left));
								aabbVertexNormal = glm::vec3(glm::normalize(glm::cross(tangentX, tangentY)));
								
								// Color
								if (PlanetTerrain::generateColor) {
									aabbVertexColor = glm::vec4(PlanetTerrain::generateColor(top_left.a - planet->solidRadius), 1.0f);
								}
							}
						}
						
						// {// Simplified mesh
						// 	const int sub = vertexSubdivisionsPerChunk;
						// 	const int tl = 0;
						// 	const int tr = sub;
						// 	const int bl = sub * (sub + 1);
						// 	const int br = sub * (sub + 2);
						// 	const int bm = (bl + br) / 2;
						// 	const int tm = (tl + tr) / 2;
						// 	const int lm = (tl + bl) / 2;
						// 	const int rm = (tr + br) / 2;
						// 	const int mm = (tl + br) / 2;
						// 	colliderIndices = {
						// 		// minimum (one quad, two triangles)
						// 			// tl,bl,br,  tl,br,tr,
						// 		// medium (4 quads, 8 triangles)
						// 			tl,lm,mm,  tl,mm,tm,
						// 			lm,bl,bm,  lm,bm,mm,
						// 			tm,mm,rm,  tm,rm,tr,
						// 			mm,bm,br,  mm,br,rm,
						// 	};
						// }

						// if (!meshGenerating) return;
						
						{// Adjust boundaries
							topLeftPosLowest = glm::normalize(topLeftPos) * lowestAltitude;
							topRightPosLowest = glm::normalize(topRightPos) * lowestAltitude;
							bottomLeftPosLowest = glm::normalize(bottomLeftPos) * lowestAltitude;
							bottomRightPosLowest = glm::normalize(bottomRightPos) * lowestAltitude;
							
							// topLeftPosHighest = glm::normalize(topLeftPos) * highestAltitude;
							// topRightPosHighest = glm::normalize(topRightPos) * highestAltitude;
							// bottomLeftPosHighest = glm::normalize(bottomLeftPos) * highestAltitude;
							// bottomRightPosHighest = glm::normalize(bottomRightPos) * highestAltitude;
						}
						
					}
					
					#ifdef PLANET_CHUNK_CACHE_ENABLE
					{// Store into cache file
						cacheFile.LockReadWrite();
						cacheFile.Truncate();
						cacheFile << CHUNK_CACHE_VERSION;
							cacheFile.WriteBytes(reinterpret_cast<byte*>(aabbVertices), nbAabbPerChunk*sizeof(Mesh::ProceduralVertexAABB));
							cacheFile.WriteBytes(reinterpret_cast<byte*>(vertexNormals), nbAabbPerChunk*sizeof(Mesh::VertexNormal));
							cacheFile.WriteBytes(reinterpret_cast<byte*>(vertexColors), nbAabbPerChunk*sizeof(Mesh::VertexColor<glm::f32>));
							// cacheFile << colliderIndices;
							cacheFile << lowestAltitude;
							cacheFile << highestAltitude;
							cacheFile << topLeftPosLowest;
							cacheFile << topRightPosLowest;
							cacheFile << bottomLeftPosLowest;
							cacheFile << bottomRightPosLowest;
							// cacheFile << topLeftPosHighest;
							// cacheFile << topRightPosHighest;
							// cacheFile << bottomLeftPosHighest;
							// cacheFile << bottomRightPosHighest;
							cacheFile << boundingDistance;
						cacheFile.Flush();
						cacheFile.UnlockReadWrite();
					}
					#endif
				}
				
			} else {
				RenderableGeometryEntity::Material material {};
				material.visibility.roughness = 180;
				material.visibility.metallic = 0;
				material.visibility.indexOfRefraction = 1.55 * 50;
				material.visibility.textures[0] = Renderer::sbtOffsets["call:tex_rough_normal"];
				material.visibility.texFactors[0] = 255;
				entity->Allocate(renderingDevice, "V4D_andromeda:planet.terrain")->material = material;
				entity->rayTracingMask = RAY_TRACED_ENTITY_TERRAIN;
				auto buffersWriteLock = entity->GetBuffersWriteLock();
					auto meshIndices = entity->Add_meshIndices16()->AllocateBuffersCount(renderingDevice, nbIndicesPerChunk);
					auto vertexPositions = entity->Add_meshVertexPosition()->AllocateBuffersCount(renderingDevice, nbVerticesPerChunk);
					auto vertexNormals = entity->Add_meshVertexNormal()->AllocateBuffersCount(renderingDevice, nbVerticesPerChunk);
					auto vertexColors = entity->Add_meshVertexColorF32()->AllocateBuffersCount(renderingDevice, nbVerticesPerChunk);
					auto vertexUVs = entity->Add_meshVertexUV()->AllocateBuffersCount(renderingDevice, nbVerticesPerChunk);
					entity->Add_meshCustomData()->AllocateBuffersFromList(renderingDevice, {uvMult, uvMult, uvOffsetX, uvOffsetY});
					entity->generator = [](auto* entity, Device*){entity->generated = false;};
					entity->SetWorldTransform(planet->matrix * transform);
				entityLock.unlock();
			
				#ifdef PLANET_CHUNK_CACHE_ENABLE
				// Cache file
				std::string chunkId = GetChunkId();
				v4d::io::BinaryFileStream cacheFile (std::string(V4D_MODULE_CACHE_PATH(THIS_MODULE, "chunks/")) + chunkId + ".binary", 1024*1024);
				constexpr size_t cacheFileSize 
									= sizeof(CHUNK_CACHE_VERSION)
									+ nbIndicesPerChunk * sizeof(Mesh::Index16)
									+ nbVerticesPerChunk * sizeof(Mesh::VertexPosition)
									+ nbVerticesPerChunk * sizeof(Mesh::VertexNormal)
									+ nbVerticesPerChunk * sizeof(Mesh::VertexColor<glm::f32>)
									+ nbVerticesPerChunk * sizeof(Mesh::VertexUV)
									+ 24 * sizeof(uint16_t) + sizeof(uint8_t) // geometry->simplifiedMeshIndices
									+ sizeof(lowestAltitude)
									+ sizeof(highestAltitude)
									+ sizeof(topLeftPosLowest)
									+ sizeof(topRightPosLowest)
									+ sizeof(bottomLeftPosLowest)
									+ sizeof(bottomRightPosLowest)
									// + sizeof(topLeftPosHighest)
									// + sizeof(topRightPosHighest)
									// + sizeof(bottomLeftPosHighest)
									// + sizeof(bottomRightPosHighest)
									+ sizeof(boundingDistance)
				;
				if (cacheFile.GetSize() == cacheFileSize && cacheFile.Read<uint32_t>() == CHUNK_CACHE_VERSION) {
					{// Load from cache file
						cacheFile.LockReadWrite();
							cacheFile.ReadBytes(reinterpret_cast<byte*>(meshIndices), nbIndicesPerChunk*sizeof(Mesh::Index16));
							cacheFile.ReadBytes(reinterpret_cast<byte*>(vertexPositions), nbVerticesPerChunk*sizeof(Mesh::VertexPosition));
							cacheFile.ReadBytes(reinterpret_cast<byte*>(vertexNormals), nbVerticesPerChunk*sizeof(Mesh::VertexNormal));
							cacheFile.ReadBytes(reinterpret_cast<byte*>(vertexColors), nbVerticesPerChunk*sizeof(Mesh::VertexColor<glm::f32>));
							cacheFile.ReadBytes(reinterpret_cast<byte*>(vertexUVs), nbVerticesPerChunk*sizeof(Mesh::VertexUV));
							cacheFile >> colliderIndices;
							cacheFile >> lowestAltitude;
							cacheFile >> highestAltitude;
							cacheFile >> topLeftPosLowest;
							cacheFile >> topRightPosLowest;
							cacheFile >> bottomLeftPosLowest;
							cacheFile >> bottomRightPosLowest;
							// cacheFile >> topLeftPosHighest;
							// cacheFile >> topRightPosHighest;
							// cacheFile >> bottomLeftPosHighest;
							// cacheFile >> bottomRightPosHighest;
							cacheFile >> boundingDistance;
						cacheFile.UnlockReadWrite();
					}
				} else 
				#endif
				{
					{// Generate
						
						int genRow = 0;
						int genCol = 0;
						int genVertexIndex = 0;
						int genIndexIndex = 0;
						
						// Fetch information for generating this chunk
						auto [faceDir, topDir, rightDir] = GetFaceVectors(face);
						double topSign = topDir.x + topDir.y + topDir.z;
						double rightSign = rightDir.x + rightDir.y + rightDir.z;
						
						// Generate terrain mesh
						while (genRow <= vertexSubdivisionsPerChunk) {
							while (genCol <= vertexSubdivisionsPerChunk) {
								uint32_t currentIndex = (vertexSubdivisionsPerChunk+1) * genRow + genCol;
								
								glm::dvec3 topOffset = glm::mix(topLeft - center, bottomLeft - center, double(genRow)/vertexSubdivisionsPerChunk);
								glm::dvec3 rightOffset = glm::mix(topLeft - center, topRight - center, double(genCol)/vertexSubdivisionsPerChunk);
								
								// position
								glm::dvec3 pos = CubeToSphere::Spherify(center + topDir*topOffset + rightDir*rightOffset, face);
								double altitude = planet->GetHeightMap(pos, triangleSize);
								glm::dvec3 posOnChunk = inverseTransform * glm::dvec4(pos * altitude, 1);
								vertexPositions[currentIndex] = glm::vec3(posOnChunk);
								
								// Color
								if (PlanetTerrain::generateColor) {
									vertexColors[currentIndex] = glm::vec4(PlanetTerrain::generateColor(altitude - planet->solidRadius), 1.0f);
								}
								
								// UV
								vertexUVs[currentIndex] = glm::vec2(
									(rightSign < 0 ? (vertexSubdivisionsPerChunk-genCol) : genCol),
									(topSign < 0 ? (vertexSubdivisionsPerChunk-genRow) : genRow)
								) / float(vertexSubdivisionsPerChunk);
								
								// Normal
								vertexNormals[currentIndex] = glm::vec3(pos); // already normalized
								
								genVertexIndex++;
								
								// Bounding distance
								boundingDistance = std::max(boundingDistance, glm::length(posOnChunk));
								
								// Altitude bounds
								lowestAltitude = std::min(lowestAltitude, altitude);
								highestAltitude = std::max(highestAltitude, altitude);

								// indices
								if (genRow < vertexSubdivisionsPerChunk) {
									uint32_t topLeftIndex = currentIndex;
									uint32_t topRightIndex = topLeftIndex+1;
									uint32_t bottomLeftIndex = (vertexSubdivisionsPerChunk+1) * (genRow+1) + genCol;
									uint32_t bottomRightIndex = bottomLeftIndex+1;
									if (genCol < vertexSubdivisionsPerChunk) {
										if (topSign == rightSign) {
											meshIndices[genIndexIndex++] = topLeftIndex;
											meshIndices[genIndexIndex++] = bottomLeftIndex;
											meshIndices[genIndexIndex++] = bottomRightIndex;
											meshIndices[genIndexIndex++] = topLeftIndex;
											meshIndices[genIndexIndex++] = bottomRightIndex;
											meshIndices[genIndexIndex++] = topRightIndex;
										} else {
											meshIndices[genIndexIndex++] = topLeftIndex;
											meshIndices[genIndexIndex++] = bottomRightIndex;
											meshIndices[genIndexIndex++] = bottomLeftIndex;
											meshIndices[genIndexIndex++] = topLeftIndex;
											meshIndices[genIndexIndex++] = topRightIndex;
											meshIndices[genIndexIndex++] = bottomRightIndex;
										}
									}
								}
								
								++genCol;
								// if (!meshGenerating) return;
							}
							
							genCol = 0;
							++genRow;
						}
						
						{// Simplified mesh
							const int sub = vertexSubdivisionsPerChunk;
							const int tl = 0;
							const int tr = sub;
							const int bl = sub * (sub + 1);
							const int br = sub * (sub + 2);
							const int bm = (bl + br) / 2;
							const int tm = (tl + tr) / 2;
							const int lm = (tl + bl) / 2;
							const int rm = (tr + br) / 2;
							const int mm = (tl + br) / 2;
							colliderIndices = {
								// minimum (one quad, two triangles)
									// tl,bl,br,  tl,br,tr,
								// medium (4 quads, 8 triangles)
									tl,lm,mm,  tl,mm,tm,
									lm,bl,bm,  lm,bm,mm,
									tm,mm,rm,  tm,rm,tr,
									mm,bm,br,  mm,br,rm,
							};
						}

						// if (!meshGenerating) return;
						
						{// Normals
							for (genRow = 0; genRow <= vertexSubdivisionsPerChunk; ++genRow) {
								for (genCol = 0; genCol <= vertexSubdivisionsPerChunk; ++genCol) {
									uint32_t currentIndex = (vertexSubdivisionsPerChunk+1) * genRow + genCol;
									
									glm::vec3 tangentX, tangentY;
									
									if (genRow < vertexSubdivisionsPerChunk && genCol < vertexSubdivisionsPerChunk) {
										// For full face (generate top left)
										uint32_t topLeftIndex = currentIndex;
										uint32_t topRightIndex = topLeftIndex+1;
										uint32_t bottomLeftIndex = (vertexSubdivisionsPerChunk+1) * (genRow+1) + genCol;
										
										tangentX = glm::normalize(glm::vec3(vertexPositions[topRightIndex]) - glm::vec3(vertexPositions[currentIndex]));
										tangentY = glm::normalize(glm::vec3(vertexPositions[currentIndex]) - glm::vec3(vertexPositions[bottomLeftIndex]));
										
									} else if (genCol == vertexSubdivisionsPerChunk && genRow == vertexSubdivisionsPerChunk) {
										// For right-most bottom-most vertex (generate bottom-most right-most)
										
										glm::vec3 bottomLeftPos {0};
										{
											glm::dvec3 topOffset = glm::mix(topLeft - center, bottomLeft - center, double(genRow+1)/vertexSubdivisionsPerChunk);
											glm::dvec3 rightOffset = glm::mix(topLeft - center, topRight - center, double(genCol)/vertexSubdivisionsPerChunk);
											glm::dvec3 pos = Spherify(center + topDir*topOffset + rightDir*rightOffset, face);
											bottomLeftPos = inverseTransform * glm::dvec4{pos * planet->GetHeightMap(pos, triangleSize), 1};
										}

										glm::vec3 topRightPos {0};
										{
											glm::dvec3 topOffset = glm::mix(topLeft - center, bottomLeft - center, double(genRow)/vertexSubdivisionsPerChunk);
											glm::dvec3 rightOffset = glm::mix(topLeft - center, topRight - center, double(genCol+1)/vertexSubdivisionsPerChunk);
											glm::dvec3 pos = Spherify(center + topDir*topOffset + rightDir*rightOffset, face);
											topRightPos = inverseTransform * glm::dvec4{pos * planet->GetHeightMap(pos, triangleSize), 1};
										}

										tangentX = glm::normalize(topRightPos - glm::vec3(vertexPositions[currentIndex]));
										tangentY = glm::normalize(glm::vec3(vertexPositions[currentIndex]) - bottomLeftPos);
										
									} else if (genCol == vertexSubdivisionsPerChunk) {
										// For others in right col (generate top right)
										uint32_t bottomRightIndex = currentIndex+vertexSubdivisionsPerChunk+1;
										
										glm::vec3 topRightPos {0};
										{
											glm::dvec3 topOffset = glm::mix(topLeft - center, bottomLeft - center, double(genRow)/vertexSubdivisionsPerChunk);
											glm::dvec3 rightOffset = glm::mix(topLeft - center, topRight - center, double(genCol+1)/vertexSubdivisionsPerChunk);
											glm::dvec3 pos = Spherify(center + topDir*topOffset + rightDir*rightOffset, face);
											topRightPos = inverseTransform * glm::dvec4{pos * planet->GetHeightMap(pos, triangleSize), 1};
										}

										tangentX = glm::normalize(topRightPos - glm::vec3(vertexPositions[currentIndex]));
										tangentY = glm::normalize(glm::vec3(vertexPositions[currentIndex]) - glm::vec3(vertexPositions[bottomRightIndex]));
										
									} else if (genRow == vertexSubdivisionsPerChunk) {
										// For others in bottom row (generate bottom left)
										
										glm::vec3 bottomLeftPos {0};
										{
											glm::dvec3 topOffset = glm::mix(topLeft - center, bottomLeft - center, double(genRow+1)/vertexSubdivisionsPerChunk);
											glm::dvec3 rightOffset = glm::mix(topLeft - center, topRight - center, double(genCol)/vertexSubdivisionsPerChunk);
											glm::dvec3 pos = Spherify(center + topDir*topOffset + rightDir*rightOffset, face);
											bottomLeftPos = inverseTransform * glm::dvec4{pos * planet->GetHeightMap(pos, triangleSize), 1};
										}

										tangentX = glm::normalize(glm::vec3(vertexPositions[currentIndex+1]) - glm::vec3(vertexPositions[currentIndex]));
										tangentY = glm::normalize(glm::vec3(vertexPositions[currentIndex]) - bottomLeftPos);
									}
									
									tangentX *= (float)rightSign;
									tangentY *= (float)topSign;
									
									glm::vec3 normal = glm::normalize(glm::cross(tangentX, tangentY));
									
									vertexNormals[currentIndex] = normal;
									
									// slope = (float) glm::max(0.0, dot(glm::dvec3(normal), glm::normalize(centerPos + glm::dvec3(vertexPositions[currentIndex]))));
									
									// if (!meshGenerating) return;
								}
							}
						}
						
						// if (!meshGenerating) return;
						
						// Skirts
						if (useSkirts) {
							int firstSkirtIndex = genVertexIndex;
							auto addSkirt = [this, &genVertexIndex, &genIndexIndex, firstSkirtIndex, topSign, rightSign, meshIndices, vertexPositions, vertexNormals, vertexColors, vertexUVs](int pointIndex, int nextPointIndex, bool firstPoint = false, bool lastPoint = false) {
								int skirtIndex = genVertexIndex++;
								{
									vertexPositions[skirtIndex] = glm::vec3(vertexPositions[pointIndex]) + glm::vec3(glm::normalize(glm::dvec3(0,1,0)) * (chunkSize / double(vertexSubdivisionsPerChunk) / 4.0));
								}
								vertexNormals[skirtIndex] = vertexNormals[pointIndex];
								vertexColors[skirtIndex] = vertexColors[pointIndex];
								vertexUVs[skirtIndex] = vertexUVs[pointIndex];
								
								if (topSign == rightSign) {
									meshIndices[genIndexIndex++] = pointIndex;
									meshIndices[genIndexIndex++] = skirtIndex;
									meshIndices[genIndexIndex++] = nextPointIndex;
									meshIndices[genIndexIndex++] = nextPointIndex;
									meshIndices[genIndexIndex++] = skirtIndex;
									if (lastPoint) {
										meshIndices[genIndexIndex++] = firstSkirtIndex;
									} else {
										meshIndices[genIndexIndex++] = skirtIndex + 1;
									}
								} else {
									meshIndices[genIndexIndex++] = pointIndex;
									meshIndices[genIndexIndex++] = nextPointIndex;
									meshIndices[genIndexIndex++] = skirtIndex;
									meshIndices[genIndexIndex++] = skirtIndex;
									meshIndices[genIndexIndex++] = nextPointIndex;
									if (lastPoint) {
										meshIndices[genIndexIndex++] = firstSkirtIndex;
									} else {
										meshIndices[genIndexIndex++] = skirtIndex + 1;
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
						
						{// Check for errors
							if (genVertexIndex != nbVerticesPerChunk) {
								INVALIDCODE("Problem with terrain mesh generation, generated vertices do not match array size " << genVertexIndex << " != " << nbVerticesPerChunk)
								return;
							}
							if (genIndexIndex != nbIndicesPerChunk) {
								INVALIDCODE("Problem with terrain mesh generation, generated indices do not match array size " << genIndexIndex << " != " << nbIndicesPerChunk)
								return;
							}
						}
						
						{// Adjust boundaries
							topLeftPosLowest = glm::normalize(topLeftPos) * lowestAltitude;
							topRightPosLowest = glm::normalize(topRightPos) * lowestAltitude;
							bottomLeftPosLowest = glm::normalize(bottomLeftPos) * lowestAltitude;
							bottomRightPosLowest = glm::normalize(bottomRightPos) * lowestAltitude;
							
							// topLeftPosHighest = glm::normalize(topLeftPos) * highestAltitude;
							// topRightPosHighest = glm::normalize(topRightPos) * highestAltitude;
							// bottomLeftPosHighest = glm::normalize(bottomLeftPos) * highestAltitude;
							// bottomRightPosHighest = glm::normalize(bottomRightPos) * highestAltitude;
						}
						
					}
					
					#ifdef PLANET_CHUNK_CACHE_ENABLE
					{// Store into cache file
						cacheFile.LockReadWrite();
						cacheFile.Truncate();
						cacheFile << CHUNK_CACHE_VERSION;
							cacheFile.WriteBytes(reinterpret_cast<byte*>(meshIndices), nbIndicesPerChunk*sizeof(Mesh::Index16));
							cacheFile.WriteBytes(reinterpret_cast<byte*>(vertexPositions), nbVerticesPerChunk*sizeof(Mesh::VertexPosition));
							cacheFile.WriteBytes(reinterpret_cast<byte*>(vertexNormals), nbVerticesPerChunk*sizeof(Mesh::VertexNormal));
							cacheFile.WriteBytes(reinterpret_cast<byte*>(vertexColors), nbVerticesPerChunk*sizeof(Mesh::VertexColor<glm::f32>));
							cacheFile.WriteBytes(reinterpret_cast<byte*>(vertexUVs), nbVerticesPerChunk*sizeof(Mesh::VertexUV));
							cacheFile << colliderIndices;
							cacheFile << lowestAltitude;
							cacheFile << highestAltitude;
							cacheFile << topLeftPosLowest;
							cacheFile << topRightPosLowest;
							cacheFile << bottomLeftPosLowest;
							cacheFile << bottomRightPosLowest;
							// cacheFile << topLeftPosHighest;
							// cacheFile << topRightPosHighest;
							// cacheFile << bottomLeftPosHighest;
							// cacheFile << bottomRightPosHighest;
							cacheFile << boundingDistance;
						cacheFile.Flush();
						cacheFile.UnlockReadWrite();
					}
					#endif
				}
				
			}
		
			// #ifdef _DEBUG
				planet->totalChunkTimeNb++;
				planet->totalChunkTime += (float)timer.GetElapsedMilliseconds();
			// #endif
			
			entityLock.lock();
				meshGenerated = true;
				entity->SetWorldTransform(planet->matrix * transform);
				entity->generator = [](auto* entity, Device*){};
			entityLock.unlock();
		}
		
		void AddSubChunks() {
			subChunks.reserve(4);
			
			{// Top Left
				Chunk* sub = subChunks.emplace_back(new Chunk{
					planet,
					face,
					level+1,
					topLeft,
					top,
					left,
					center
				});
				sub->uvMult = uvMult/2;
				sub->uvOffsetX = uvOffsetX;
				sub->uvOffsetY = uvOffsetY;
			}
			
			{// Top Right
				Chunk* sub = subChunks.emplace_back(new Chunk{
					planet,
					face,
					level+1,
					top,
					topRight,
					center,
					right
				});
				sub->uvMult = uvMult/2;
				sub->uvOffsetX = uvOffsetX+uvMult/2;
				sub->uvOffsetY = uvOffsetY;
			}
			
			{// Bottom Left
				Chunk* sub = subChunks.emplace_back(new Chunk{
					planet,
					face,
					level+1,
					left,
					center,
					bottomLeft,
					bottom
				});
				sub->uvMult = uvMult/2;
				sub->uvOffsetX = uvOffsetX;
				sub->uvOffsetY = uvOffsetY+uvMult/2;
			}
			
			{// Bottom Right
				Chunk* sub = subChunks.emplace_back(new Chunk{
					planet,
					face,
					level+1,
					center,
					right,
					bottom,
					bottomRight
				});
				sub->uvMult = uvMult/2;
				sub->uvOffsetX = uvOffsetX+uvMult/2;
				sub->uvOffsetY = uvOffsetY+uvMult/2;
			}
			
			SortSubChunks();
		}
		
		void Remove(bool recursive) {
			{
				std::unique_lock lock(/*stateMutex,*/ generatorMutex);
				if (meshGenerating) {
					meshGenerating = false;
					lock.unlock();
					// std::this_thread::yield();
					lock.lock();
				}
				if (meshEnqueuedForGeneration) {
					ChunkGeneratorCancel(this, recursive);
				}
				render = false;
				meshGenerated = false;
			}
			if (entity) {
				entity->Destroy();
				entity = nullptr;
			}
			if (recursive) {
				std::scoped_lock lock(subChunksMutex);
				if (subChunks.size() > 0) {
					for (auto* subChunk : subChunks) {
						subChunk->Remove(true);
						delete subChunk;
					}
					subChunks.clear();
				}
			}
		}
		
		bool Process() {
			RefreshDistanceFromCamera();
			
			// Angle Culling
			double angleThreshold = -(planet->heightVariation*2 / planet->solidRadius);
			bool chunkVisibleByAngle = glm::distance(planet->cameraPos, centerPos) < std::max(chunkSize, std::max(planet->heightVariation*2, highestAltitude - lowestAltitude))
									|| glm::dot(glm::normalize(planet->cameraPos - centerPos), glm::normalize(centerPos)) > angleThreshold
									// || glm::dot(glm::normalize(planet->cameraPos - topLeftPos), glm::normalize(topLeftPos)) > angleThreshold
									// || glm::dot(glm::normalize(planet->cameraPos - topRightPos), glm::normalize(topRightPos)) > angleThreshold
									// || glm::dot(glm::normalize(planet->cameraPos - bottomLeftPos), glm::normalize(bottomLeftPos)) > angleThreshold
									// || glm::dot(glm::normalize(planet->cameraPos - bottomRightPos), glm::normalize(bottomRightPos)) > angleThreshold
									|| glm::dot(glm::normalize(planet->cameraPos - topLeftPosLowest), glm::normalize(topLeftPos)) > angleThreshold
									|| glm::dot(glm::normalize(planet->cameraPos - topRightPosLowest), glm::normalize(topRightPos)) > angleThreshold
									|| glm::dot(glm::normalize(planet->cameraPos - bottomLeftPosLowest), glm::normalize(bottomLeftPos)) > angleThreshold
									|| glm::dot(glm::normalize(planet->cameraPos - bottomRightPosLowest), glm::normalize(bottomRightPos)) > angleThreshold
									// || glm::dot(glm::normalize(planet->cameraPos - topLeftPosHighest), glm::normalize(topLeftPosHighest)) > angleThreshold
									// || glm::dot(glm::normalize(planet->cameraPos - topRightPosHighest), glm::normalize(topRightPosHighest)) > angleThreshold
									// || glm::dot(glm::normalize(planet->cameraPos - bottomLeftPosHighest), glm::normalize(bottomLeftPosHighest)) > angleThreshold
									// || glm::dot(glm::normalize(planet->cameraPos - bottomRightPosHighest), glm::normalize(bottomRightPosHighest)) > angleThreshold
								// || true
			;
			
			bool allSubchunksRendered = false;
			
			active = chunkVisibleByAngle;
			render = active && meshGenerated && entity;
			
			if (active) {
				if (ShouldAddSubChunks()) {
					std::scoped_lock lock(subChunksMutex);
					if (subChunks.size() == 0) AddSubChunks();
					allSubchunksRendered = true;
					for (auto* subChunk : subChunks) {
						if (!subChunk->Process()) {
							allSubchunksRendered = false;
						}
					}
					if (allSubchunksRendered) {
						Remove(false);
					} else if (meshEnqueuedForGeneration) {
						ChunkGeneratorCancel(this);
					}
				} else {
					bool shouldRemoveSubChunks = ShouldRemoveSubChunks();
					if (shouldRemoveSubChunks && entity && entity->generated) {
						std::scoped_lock lock(subChunksMutex);
						for (auto* subChunk : subChunks) {
							subChunk->Remove(true);
						}
					} else {
						if (shouldRemoveSubChunks) {
							std::scoped_lock lock(subChunksMutex);
							for (auto* subChunk : subChunks) if (subChunk->meshEnqueuedForGeneration) {
								ChunkGeneratorCancel(subChunk, true);
							}
						}
						if (!entity || !entity->generated) {
							// std::scoped_lock lock(stateMutex);
							if (!meshGenerated && !meshGenerating) {
								ChunkGeneratorEnqueue(this);
							}
						}
					}
				}
				
			} else {
				Remove(true);
			}
			
			if (render) {
				entity->SetWorldTransform(planet->matrix * transform);
			}
			
			return (render && entity->generated) || allSubchunksRendered;
		}
		
		// void BeforeRender() {
		// 	{
		// 		// std::scoped_lock lock(stateMutex);
		// 		RefreshDistanceFromCamera();
				
		// 		if (meshGenerated && entity) {
		// 			if (entity->generated) {
		// 				if (render) {
		// 					entity->rayTracingMask = RAY_TRACED_ENTITY_TERRAIN;
		// 					entity->SetWorldTransform(planet->matrix * transform);
		// 				} else {
		// 					entity->rayTracingMask = 0;
		// 				}
		// 			}
		// 		}
		// 	}
		// 	std::scoped_lock lock(subChunksMutex);
		// 	for (auto* subChunk : subChunks) {
		// 		subChunk->BeforeRender();
		// 	}
		// }
		
		void SortSubChunks() {
			std::lock_guard lock(subChunksMutex);
			std::sort(subChunks.begin(), subChunks.end(), [](Chunk* a, Chunk* b) -> bool {return /*a->level > b->level ||*/ a->distanceFromCamera < b->distanceFromCamera;});
			for (auto* chunk : subChunks) {
				chunk->SortSubChunks();
			}
		}
		
	};
	
	// Chunk Generator
	static std::vector<Chunk*> chunkGeneratorQueue;
	static std::vector<std::thread> chunkGeneratorThreads;
	static std::mutex chunkGeneratorQueueMutex;
	static std::condition_variable chunkGeneratorEventVar;
	static std::atomic<bool> chunkGeneratorActive;
	static void StartChunkGenerator() {
		if (chunkGeneratorActive) return;
		chunkGeneratorActive = true;
		uint32_t nbThreads = std::max((uint32_t)1, std::thread::hardware_concurrency() - 4);
		chunkGeneratorThreads.reserve(nbThreads);
		LOG_VERBOSE("Using " << nbThreads << " threads to render planet terrain")
		for (int i = 0; i < nbThreads; ++i) {
			chunkGeneratorThreads.emplace_back([threadIndex=i](){
				
				// CPU Affinity
				if (std::thread::hardware_concurrency() > 4) UNSET_CPU_AFFINITY(0, 1, std::thread::hardware_concurrency()/2, std::thread::hardware_concurrency()/2+1)
				else SET_CPU_AFFINITY(0)
				
				while (chunkGeneratorActive) {
					Chunk* chunk = nullptr;
					double closestChunkDistance = std::numeric_limits<double>::max();
					{
						std::unique_lock lock(chunkGeneratorQueueMutex);
						chunkGeneratorEventVar.wait(lock, [] {
							return !chunkGeneratorActive || chunkGeneratorQueue.size() > 0;
						});
						if (!chunkGeneratorActive) return;
						int lastIndex = chunkGeneratorQueue.size()-1;
						int index = -1;
						for (int i = lastIndex; i >= 0; --i) {
							Chunk* c = chunkGeneratorQueue[i];
							if (c && c->distanceFromCamera < closestChunkDistance) {
								index = i;
								chunk = c;
								closestChunkDistance = c->distanceFromCamera;
							}
						}
						if (!chunk) {
							continue;
						}
						if (index != lastIndex) {
							chunkGeneratorQueue[index] = chunkGeneratorQueue[lastIndex];
						}
						chunkGeneratorQueue.pop_back();
						chunk->meshEnqueuedForGeneration = false;
					}
					std::lock_guard lock(chunk->generatorMutex);
					if (chunk->meshGenerating) {
						chunk->Generate();
						chunk->meshGenerating = false;
					}
				}
			});
		}
	}
	static void EndChunkGenerator() {
		if (!chunkGeneratorActive) return;
		chunkGeneratorActive = false;
		chunkGeneratorEventVar.notify_all();
		for (auto& thread : chunkGeneratorThreads) {
			if (thread.joinable()) thread.join();
		}
		std::lock_guard lock(chunkGeneratorQueueMutex);
		chunkGeneratorThreads.clear();
		chunkGeneratorQueue.clear();
	}
	static void ChunkGeneratorEnqueue(Chunk* chunk) {
		std::lock_guard lock(chunkGeneratorQueueMutex);
		chunkGeneratorQueue.push_back(chunk);
		chunk->meshGenerating = true;
		chunk->meshEnqueuedForGeneration = true;
		chunkGeneratorEventVar.notify_all();
	}
	static void ChunkGeneratorCancel(Chunk* chunk, bool recursive = false) {
		if (recursive) {
			std::lock_guard lock(chunk->subChunksMutex);
			for (auto* subChunk : chunk->subChunks) {
				if (subChunk->meshEnqueuedForGeneration) {
					ChunkGeneratorCancel(subChunk, true);
				}
			}
		}
		std::lock_guard lock(chunkGeneratorQueueMutex);
		int lastIndex = chunkGeneratorQueue.size()-1;
		for (int index = 0; index < chunkGeneratorQueue.size(); ++index) {
			if (chunkGeneratorQueue[index] == chunk) {
				if (index != lastIndex) {
					chunkGeneratorQueue[index] = chunkGeneratorQueue[lastIndex];
				}
				chunkGeneratorQueue.pop_back();
				break;
			}
		}
		chunk->meshEnqueuedForGeneration = false;
		chunk->meshGenerating = false;
	}

	std::recursive_mutex chunksMutex;
	std::mutex planetMutex;
	std::vector<Chunk*> chunks {};
	
	static double (*generatorFunction)(TERRAIN_GENERATOR_LIB_HEIGHTMAP_ARGS);
	static glm::vec3 (*generateColor)(double heightMap);
	
	double GetHeightMap(const glm::dvec3& normalizedPos, double triangleSize = 1.0) {
		double height = solidRadius;
		// height += v4d::noise::FastSimplexFractal((normalizedPos*solidRadius/100000.0), 2)*heightVariation;
		// height += v4d::noise::FastSimplexFractal((normalizedPos*solidRadius/20000.0), 5)*heightVariation/10.0;
		// if (triangleSize < 1000)
		// 	height += v4d::noise::FastSimplexFractal((normalizedPos*solidRadius/100.0), 3)*6.0;
		// if (triangleSize < 100)
		// 	height += v4d::noise::FastSimplexFractal(normalizedPos*solidRadius/10.0, 2);
		// if (triangleSize < 30)
		// 	height += v4d::noise::FastSimplexFractal(normalizedPos*solidRadius, 3)/5.0;
		
		if (generatorFunction) height += generatorFunction(normalizedPos, solidRadius, heightVariation);
		else {
			LOG_ERROR("Tried to generate heightmap but terrainGenerator dynamic library is not loaded")
		}
		
		return height;
	}
	
	glm::vec4 GetColorMap(glm::dvec3 normalizedPos, double triangleSize) {

		// const int nbPlates = 50;
		// static std::vector<glm::dvec3> plates {};
		// if (plates.size() == 0) {
		// 	plates.reserve(nbPlates);
		// 	for (int i = 0; i < nbPlates; ++i) {
		// 		plates.push_back(glm::normalize(glm::dvec3{
		// 			v4d::noise::QuickNoise(128.54+i*2.45),
		// 			v4d::noise::QuickNoise(982.1+i*1.87),
		// 			v4d::noise::QuickNoise(34.9+i*7.22),
		// 		}*2.0-1.0));
		// 	}
		// }
		
		// double minDist = 1e100;
		// for (int i = 0; i < nbPlates; ++i) {
		// 	minDist = std::min(minDist, glm::distance(normalizedPos, plates[i]));
		// }
		
		// return {minDist, minDist, minDist, 1.0f};
		
		// auto h = ((GetHeightMap(normalizedPos, triangleSize) - solidRadius) / heightVariation)/2.0+0.5;
		// return glm::vec4(h,h,h, 1.0);
		
		return {1.0f, 1.0f, 1.0f, 1.0f};
	}
	
	double GetSolidCirconference() {
		return solidRadius * 2.0 * 3.14159265359;
	}
	
	PlanetTerrain(
		  double radius
		, double solidRadius
		, double heightVariation
		, double atmosphereRadius
		, double atmosphereThickness
		, double visibilityDistance
		, double rayleighHeight
		, double mieHeight
	  ) : radius(radius)
		, solidRadius(solidRadius)
		, heightVariation(heightVariation)
	{
		AddBaseChunks();
		
		// Atmosphere
		auto atmosphereEntity = RenderableGeometryEntity::Create(THIS_MODULE);
		atmosphereEntity->generator = [this,radius,solidRadius,heightVariation,atmosphereRadius,atmosphereThickness,visibilityDistance,rayleighHeight,mieHeight](v4d::graphics::RenderableGeometryEntity* entity, v4d::graphics::vulkan::Device* renderingDevice){
			entity->Allocate(renderingDevice, "V4D_andromeda:atmosphere");
			entity->rayTracingMask = RAY_TRACED_ENTITY_ATMOSPHERE;
			entity->Add_proceduralVertexAABB()->AllocateBuffers(renderingDevice, {glm::vec3(-atmosphereRadius), glm::vec3(+atmosphereRadius)});
			entity->Add_meshVertexColorF32()->AllocateBuffers(renderingDevice, {atmosphereColor.r, atmosphereColor.g, atmosphereColor.b, atmosphereDensityFactor});
			entity->Add_meshCustomData()->AllocateBuffersFromList(renderingDevice, {
				/*planetSolidRadius*/float(solidRadius),
				/*visibilityDistance*/float(visibilityDistance),
				/*minStepSize*/float(visibilityDistance/1000.0),
				/*innerRadius*/float(solidRadius - heightVariation),
				/*outerRadius*/float(atmosphereRadius),
				/*atmosphereThickness*/float(atmosphereThickness),
				/*rayleighHeight*/float(rayleighHeight),
				/*mieHeight*/float(mieHeight),
			});
			entity->SetWorldTransform(matrix);
		};
		atmosphere = atmosphereEntity;
	}
	
	~PlanetTerrain() {
		std::lock_guard lock(planetMutex);
		RemoveBaseChunks();
		
		// Atmosphere
		if (auto atmosphereEntity = atmosphere.lock(); atmosphereEntity) {
			atmosphereEntity->Destroy();
		}
	}
	
	void Update() {
		std::lock_guard lock(chunksMutex);
		
		auto terrainHeightAtThisPosition = GetHeightMap(glm::normalize(cameraPos), 0.5);
		cameraAltitudeAboveTerrain = glm::length(cameraPos) - terrainHeightAtThisPosition;
		chunkSubdivisionDistanceFactor = glm::mix(1.0, 4.0, glm::pow(glm::clamp(cameraAltitudeAboveTerrain / (solidRadius / 8), 0.0, 1.0), 0.25));
		
		if (auto atmosphereEntity = atmosphere.lock(); atmosphereEntity) {
			atmosphereEntity->SetWorldTransform(matrix);
		}
		
		for (auto* chunk : chunks) {
			chunk->Process();
		}
		
		// for (auto* chunk : chunks) {
		// 	chunk->BeforeRender();
		// }
		
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
					
					auto* chunk = chunks.emplace_back(new Chunk{
						this,
						face,
						0,
						faceDir + topDir*topOffset 		+ rightDir*leftOffset,
						faceDir + topDir*topOffset 		+ rightDir*rightOffset,
						faceDir + topDir*bottomOffset 	+ rightDir*leftOffset,
						faceDir + topDir*bottomOffset 	+ rightDir*rightOffset
					});
					chunk->uvOffsetX = col/chunkSubdivisionsPerFace;
					chunk->uvOffsetY = row/chunkSubdivisionsPerFace;
					chunk->uvMult = 1.0f/chunkSubdivisionsPerFace;
				}
			}
		}
		
		SortChunks();
	}
	
	void RemoveBaseChunks() {
		std::lock_guard lock(chunksMutex);
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
	
	void Optimize() {
		// Optimize only when no chunk is being generated, camera moved at least x distance and not more than once every x seconds
		if ((PlanetTerrain::chunkGeneratorQueue.size() > 0) || glm::distance(cameraPos, lastOptimizePosition) < chunkOptimizationMinMoveDistance || lastOptimizeTime.GetElapsedSeconds() < chunkOptimizationMinTimeInterval)
			return;
		
		// reset 
		lastOptimizePosition = cameraPos;
		lastOptimizeTime.Reset();
		
		// Optimize
		SortChunks();
	}

	static void CollectGarbage(Device* renderingDevice) {
		// Collect garbage not more than once every x seconds
		if (lastGarbageCollectionTime.GetElapsedSeconds() < garbageCollectionInterval)
			return;
		
		// reset
		lastGarbageCollectionTime.Reset();
		
		// Collect garbage
		//...
	}
	
};
