#pragma once
#include <v4d.h>

#include "CubeToSphere.hpp"
#include "PlanetAtmosphere.hpp"
#include "Noise.hpp"

#define APP_CPU_AFFINITY_GENERATE 3

using namespace v4d::scene;
using namespace v4d::graphics;
using namespace v4d::graphics::vulkan;

struct PlanetTerrain {
	
	#pragma region Constructor arguments
	double radius; // top of atmosphere (maximum radius)
	double solidRadius; // standard radius of solid surface (AKA sea level, surface height can go below or above)
	double heightVariation; // half the total variation (surface height is +- heightVariation)
	glm::dvec3 absolutePosition; // position of planet relative to world (star system center)
	PlanetAtmosphere atmosphere;
	#pragma endregion
	
	glm::dvec3 rotationAxis {0,0,1};
	double rotationAngle = 0;
	glm::dmat4 matrix {1};
	
	#pragma region Graphics configuration
	static const int chunkSubdivisionsPerFace = 1;
	static const int vertexSubdivisionsPerChunk = 100; // low=40, medium=100, high=180,   extreme=250
	static constexpr float chunkSubdivisionDistanceFactor = 1.0f;
	static constexpr float targetVertexSeparationInMeters = 0.1f; // approximative vertex separation in meters for the most precise level of detail
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
	Scene* scene = nullptr;
	glm::dvec3 cameraPos {0};
	double cameraAltitudeAboveTerrain = 0;
	
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
		// glm::dvec3 centerPosLowest {0};
		// glm::dvec3 centerPosHighest {0};
		glm::dvec3 topLeftPosLowest {0};
		glm::dvec3 topRightPosLowest {0};
		glm::dvec3 bottomLeftPosLowest {0};
		glm::dvec3 bottomRightPosLowest {0};
		glm::dvec3 topLeftPosHighest {0};
		glm::dvec3 topRightPosHighest {0};
		glm::dvec3 bottomLeftPosHighest {0};
		glm::dvec3 bottomRightPosHighest {0};
		
		float uvMult = 1;
		float uvOffsetX = 0;
		float uvOffsetY = 0;
		double chunkSize = 0;
		double triangleSize = 0;
		double heightAtCenter = 0;
		double lowestAltitude = 0;
		double highestAltitude = 0;
		std::atomic<double> distanceFromCamera = 0;
		#pragma endregion
		
		#pragma region States
		std::atomic<bool> active = false;
		std::atomic<bool> render = false;
		std::atomic<int> computedLevel = 0;

		std::atomic<bool> meshEnqueuedForGeneration = false;
		std::atomic<bool> meshGenerating = false;
		std::atomic<bool> meshGenerated = false;

		// std::recursive_mutex stateMutex;
		std::recursive_mutex subChunksMutex;
		#pragma endregion
		
		#pragma region Data
		std::vector<Chunk*> subChunks {};
		ObjectInstancePtr obj = nullptr;
		std::shared_ptr<Geometry> geometry = nullptr;
		std::recursive_mutex generatorMutex;
		#pragma endregion
		
		bool IsLastLevel() {
			return (triangleSize < targetVertexSeparationInMeters*1.9);
		}
		
		bool ShouldAddSubChunks() {
			if (IsLastLevel()) return false;
			return distanceFromCamera < chunkSubdivisionDistanceFactor*chunkSize;
		}
		
		bool ShouldRemoveSubChunks() {
			return distanceFromCamera > chunkSubdivisionDistanceFactor*chunkSize * 1.1;
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
			
			topLeftPosHighest = glm::normalize(topLeftPos) * highestAltitude;
			topRightPosHighest = glm::normalize(topRightPos) * highestAltitude;
			bottomLeftPosHighest = glm::normalize(bottomLeftPos) * highestAltitude;
			bottomRightPosHighest = glm::normalize(bottomRightPos) * highestAltitude;
		
			RefreshDistanceFromCamera();
		}
		
		~Chunk() {
			Remove(true);
		}
		
		uint32_t topLeftVertexIndex = 0;
		uint32_t topRightVertexIndex = vertexSubdivisionsPerChunk;
		uint32_t bottomLeftVertexIndex = (vertexSubdivisionsPerChunk+1) * vertexSubdivisionsPerChunk;
		uint32_t bottomRightVertexIndex = (vertexSubdivisionsPerChunk+1) * vertexSubdivisionsPerChunk + vertexSubdivisionsPerChunk;
		
		// void RefreshVertices() {
		// 	if (obj && obj->IsGenerated()) {
		// 		topLeftPos = centerPos + glm::dvec3(obj->GetGeometries()[0].geometry->GetVertexPtr(topLeftVertexIndex)->pos);
		// 		topRightPos = centerPos + glm::dvec3(obj->GetGeometries()[0].geometry->GetVertexPtr(topRightVertexIndex)->pos);
		// 		bottomLeftPos = centerPos + glm::dvec3(obj->GetGeometries()[0].geometry->GetVertexPtr(bottomLeftVertexIndex)->pos);
		// 		bottomRightPos = centerPos + glm::dvec3(obj->GetGeometries()[0].geometry->GetVertexPtr(bottomRightVertexIndex)->pos);
		// 		topLeftPosLowest = glm::normalize(topLeftPos) * lowestAltitude;
		// 		topRightPosLowest = glm::normalize(topRightPos) * lowestAltitude;
		// 		bottomLeftPosLowest = glm::normalize(bottomLeftPos) * lowestAltitude;
		// 		bottomRightPosLowest = glm::normalize(bottomRightPos) * lowestAltitude;
		// 		topLeftPosHighest = glm::normalize(topLeftPos) * highestAltitude;
		// 		topRightPosHighest = glm::normalize(topRightPos) * highestAltitude;
		// 		bottomLeftPosHighest = glm::normalize(bottomLeftPos) * highestAltitude;
		// 		bottomRightPosHighest = glm::normalize(bottomRightPos) * highestAltitude;
		// 		RefreshDistanceFromCamera();
		// 	}
		// }
		
		
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
			int genRow = 0;
			int genCol = 0;
			int genVertexIndex = 0;
			int genIndexIndex = 0;
			
			if (!meshGenerating) return;
			if (!planet->scene) return;
			planet->scene->Lock();
			{
				if (!obj) {
					obj = planet->scene->AddObjectInstance();
				}
				obj->Disable();
				obj->physicsActive = false;
				if (!geometry || obj->CountGeometries() == 0) {
					geometry = obj->AddGeometry("planet_terrain", nbVerticesPerChunk, nbIndicesPerChunk /* , material */ );
				}
				geometry->active = false;
			}
			planet->scene->Unlock();
			
			geometry->custom3f = {uvOffsetX, uvOffsetY, uvMult};
			
			auto [faceDir, topDir, rightDir] = GetFaceVectors(face);
			double topSign = topDir.x + topDir.y + topDir.z;
			double rightSign = rightDir.x + rightDir.y + rightDir.z;
			
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
					vertex->pos = posOnChunk;
					// Color
					// vertex->SetColor(planet->GetColorMap(pos, triangleSize));
					if (PlanetTerrain::generateColor) {
						vertex->SetColor(glm::vec4(PlanetTerrain::generateColor(altitude - planet->solidRadius), 1.0f));
					}
					// UV
					vertex->SetUV(glm::vec2(
						rightSign < 0 ? (vertexSubdivisionsPerChunk-genCol) : genCol,
						topSign < 0 ? (vertexSubdivisionsPerChunk-genRow) : genRow
					) / float(vertexSubdivisionsPerChunk));
					// Normal
					vertex->normal = pos; // already normalized
					
					geometry->isDirty = true;
					genVertexIndex++;
					
					// Bounding distance
					geometry->boundingDistance = std::max(geometry->boundingDistance, glm::length(vertex->pos));
					
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
					if (!meshGenerating) return;
				}
				
				genCol = 0;
				++genRow;
			}
			
			// Simplified mesh
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
			geometry->simplifiedMeshIndices = {
				// minimum (one quad, two triangles)
					// tl,bl,br,  tl,br,tr,
				// medium (4 quads, 8 triangles)
					tl,lm,mm,  tl,mm,tm,
					lm,bl,bm,  lm,bm,mm,
					tm,mm,rm,  tm,rm,tr,
					mm,bm,br,  mm,br,rm,
			};

			if (!meshGenerating) return;
			
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
							
					if (!meshGenerating) return;
				}
			}
			
			if (!meshGenerating) return;
			
			// Skirts
			if (useSkirts) {
				int firstSkirtIndex = genVertexIndex;
				auto addSkirt = [this, &genVertexIndex, &genIndexIndex, firstSkirtIndex, topSign, rightSign](int pointIndex, int nextPointIndex, bool firstPoint = false, bool lastPoint = false) {
					int skirtIndex = genVertexIndex++;
					
					auto* skirtVertex = geometry->GetVertexPtr(skirtIndex);
					*skirtVertex = *geometry->GetVertexPtr(pointIndex);
					skirtVertex->pos -= glm::normalize(centerPos) * (chunkSize / double(vertexSubdivisionsPerChunk) / 2.0);
					
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
			
			topLeftPosLowest = glm::normalize(topLeftPos) * lowestAltitude;
			topRightPosLowest = glm::normalize(topRightPos) * lowestAltitude;
			bottomLeftPosLowest = glm::normalize(bottomLeftPos) * lowestAltitude;
			bottomRightPosLowest = glm::normalize(bottomRightPos) * lowestAltitude;
			
			topLeftPosHighest = glm::normalize(topLeftPos) * highestAltitude;
			topRightPosHighest = glm::normalize(topRightPos) * highestAltitude;
			bottomLeftPosHighest = glm::normalize(bottomLeftPos) * highestAltitude;
			bottomRightPosHighest = glm::normalize(bottomRightPos) * highestAltitude;
			
			// std::scoped_lock lock(stateMutex);
			if (meshGenerating) {
				obj->SetGeometriesDirty(false);
				computedLevel = 0;
				meshGenerated = true;
			}
			
			// #ifdef _DEBUG
				planet->totalChunkTimeNb++;
				planet->totalChunkTime += (float)timer.GetElapsedMilliseconds();
			// #endif
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
				computedLevel = 0;
				meshGenerated = false;
			}
			if (obj) {
				if (geometry) geometry->active = false;
				obj->Disable();
				if (planet->scene) {
					planet->scene->RemoveObjectInstance(obj);
					obj = nullptr;
					geometry = nullptr;
				}
			}
			std::scoped_lock lock(subChunksMutex);
			if (recursive && subChunks.size() > 0) {
				for (auto* subChunk : subChunks) {
					subChunk->Remove(true);
					delete subChunk;
				}
				subChunks.clear();
			}
		}
		
		bool Process() {
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
			;
			
			bool allSubchunksRendered = false;
			
			active = chunkVisibleByAngle;
			render = active && meshGenerated && obj && obj->IsGenerated() && geometry->active;
			
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
					if (ShouldRemoveSubChunks() && obj && obj->IsGenerated() && geometry->active) {
						std::scoped_lock lock(subChunksMutex);
						for (auto* subChunk : subChunks) {
							subChunk->Remove(true);
						}
					} else {
						if (ShouldRemoveSubChunks()) {
							std::scoped_lock lock(subChunksMutex);
							for (auto* subChunk : subChunks) if (subChunk->meshEnqueuedForGeneration) {
								ChunkGeneratorCancel(subChunk, true);
							}
						}
						if (!obj || !obj->IsGenerated()) {
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
			
			return render || allSubchunksRendered;
		}
		
		void BeforeRender() {
			{
				// std::scoped_lock lock(stateMutex);
				RefreshDistanceFromCamera();
				if (meshGenerated && obj && obj->IsGenerated()) {
					if (render) {
						obj->Enable();
						obj->SetWorldTransform(planet->matrix * glm::translate(glm::dmat4(1), centerPos));
					} else {
						obj->Disable();
					}
				}
			}
			std::scoped_lock lock(subChunksMutex);
			for (auto* subChunk : subChunks) {
				subChunk->BeforeRender();
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
	
	// Chunk Generator
	static std::vector<Chunk*> chunkGeneratorQueue;
	static std::vector<std::thread> chunkGeneratorThreads;
	static std::mutex chunkGeneratorQueueMutex;
	static std::condition_variable chunkGeneratorEventVar;
	static std::atomic<bool> chunkGeneratorActive;
	static void StartChunkGenerator() {
		if (chunkGeneratorActive) return;
		chunkGeneratorActive = true;
		uint32_t nbThreads = std::max((uint32_t)1, std::thread::hardware_concurrency() - APP_CPU_AFFINITY_GENERATE);
		chunkGeneratorThreads.reserve(nbThreads);
		LOG_VERBOSE("Using " << nbThreads << " threads to render planet terrain")
		for (int i = 0; i < nbThreads; ++i) {
			chunkGeneratorThreads.emplace_back([threadIndex=i](){
				int cpuIndex = APP_CPU_AFFINITY_GENERATE + threadIndex;
				if (cpuIndex < std::thread::hardware_concurrency()) SET_CPU_AFFINITY(cpuIndex)
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
	
	static double (*generatorFunction)(glm::dvec3* const);
	static glm::vec3 (*generateColor)(double heightMap);
	
	double GetHeightMap(glm::dvec3 normalizedPos, double triangleSize) {
		double height = solidRadius;
		// height += v4d::noise::FastSimplexFractal((normalizedPos*solidRadius/100000.0), 2)*heightVariation;
		// height += v4d::noise::FastSimplexFractal((normalizedPos*solidRadius/20000.0), 5)*heightVariation/10.0;
		// if (triangleSize < 1000)
		// 	height += v4d::noise::FastSimplexFractal((normalizedPos*solidRadius/100.0), 3)*6.0;
		// if (triangleSize < 100)
		// 	height += v4d::noise::FastSimplexFractal(normalizedPos*solidRadius/10.0, 2);
		// if (triangleSize < 30)
		// 	height += v4d::noise::FastSimplexFractal(normalizedPos*solidRadius, 3)/5.0;
		
		if (generatorFunction) height += generatorFunction(&normalizedPos);
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
		
		return {0.5f, 0.4f, 0.3f, 1.0f};
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
		absolutePosition(absolutePosition),
		atmosphere(radius)
		// atmosphere(0)
	{
		AddBaseChunks();
	}
	
	~PlanetTerrain() {
		std::lock_guard lock(planetMutex);
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

	static void CollectGarbage(Device* device) {
		// Collect garbage not more than once every x seconds
		if (lastGarbageCollectionTime.GetElapsedSeconds() < garbageCollectionInterval)
			return;
		
		// reset
		lastGarbageCollectionTime.Reset();
		
		// Collect garbage
		//...
	}
	
	void RefreshMatrix() {
		matrix = glm::rotate(glm::translate(glm::dmat4(1), absolutePosition), rotationAngle, rotationAxis);
	}
	
};

// Buffer pools
v4d::Timer PlanetTerrain::lastGarbageCollectionTime {true};

// Chunk Generator
std::vector<PlanetTerrain::Chunk*> PlanetTerrain::chunkGeneratorQueue {};
std::vector<std::thread> PlanetTerrain::chunkGeneratorThreads {};
std::mutex PlanetTerrain::chunkGeneratorQueueMutex {};
std::condition_variable PlanetTerrain::chunkGeneratorEventVar {};
std::atomic<bool> PlanetTerrain::chunkGeneratorActive = false;

double (*PlanetTerrain::generatorFunction)(glm::dvec3* const) = nullptr;
glm::vec3 (*PlanetTerrain::generateColor)(double heightMap) = nullptr;
