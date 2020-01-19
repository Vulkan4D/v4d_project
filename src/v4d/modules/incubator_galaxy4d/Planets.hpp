#pragma once
#include <v4d.h>

#include "../../incubator_galaxy4d/Noise.hpp"

using namespace v4d::graphics;
using namespace v4d::graphics::vulkan;

// #define SPHERIFY_CUBE_BY_NORMALIZE // otherwise use technique shown here : http://mathproofs.blogspot.com/2005/07/mapping-cube-to-sphere.html

#pragma region Graphics configuration
const int chunkSubdivisionsPerFace = 8;
const int vertexSubdivisionsPerChunk = 32;
const float targetVertexSeparationInMeters = 1.0; // approximative vertex separation in meters for the most precise level of detail
#pragma endregion

#pragma region Calculated constants
const int nbChunksPerFace = chunkSubdivisionsPerFace * chunkSubdivisionsPerFace;
const int nbBaseChunksPerPlanet = nbChunksPerFace * 6;
const int nbVerticesPerChunk = (vertexSubdivisionsPerChunk+1) * (vertexSubdivisionsPerChunk+1);
const int nbIndicesPerChunk = (vertexSubdivisionsPerChunk) * (vertexSubdivisionsPerChunk) * 6;
#pragma endregion

struct Vertex {
	glm::vec4 pos;
	glm::vec4 normal;
	glm::vec2 uv;
};

enum FACE : int {
	FRONT,
	BACK,
	RIGHT,
	LEFT,
	TOP,
	BOTTOM
};

struct BufferPoolAllocation {
	int bufferIndex;
	int allocationIndex;
	int bufferOffset;
};
template<VkBufferUsageFlags usage, size_t size, int n = 1>
class DeviceLocalBufferPool {
	struct MultiBuffer {
		Buffer buffer {VK_BUFFER_USAGE_TRANSFER_DST_BIT | usage, size * n};
		std::array<bool, n> allocations {false};
	};
	std::vector<MultiBuffer> buffers {};
	Buffer stagingBuffer {VK_BUFFER_USAGE_TRANSFER_SRC_BIT, size};
	bool stagingBufferAllocated = false;
	void AllocateStagingBuffer(Device* device) {
		if (!stagingBufferAllocated) {
			stagingBufferAllocated = true;
			stagingBuffer.Allocate(device, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, false);
			stagingBuffer.MapMemory(device);
		}
	}
	void FreeStagingBuffer(Device* device) {
		if (stagingBufferAllocated) {
			stagingBufferAllocated = false;
			stagingBuffer.UnmapMemory(device);
			stagingBuffer.Free(device);
		}
	}
public:

	Buffer* operator[](int bufferIndex) {
		if (bufferIndex > buffers.size() - 1) return nullptr;
		return &buffers[bufferIndex].buffer;
	}
	
	BufferPoolAllocation Allocate(Renderer* renderer, Device* device, Queue* queue, void* data) {
		int bufferIndex = 0;
		int allocationIndex = 0;
		for (auto& multiBuffer : buffers) {
			for (bool& offsetUsed : multiBuffer.allocations) {
				if (!offsetUsed) {
					offsetUsed = true;
					goto CopyData;
				}
				++allocationIndex;
			}
			++bufferIndex;
			allocationIndex = 0;
		}
		goto AllocateNewBuffer;
		
		AllocateNewBuffer: {
			auto& multiBuffer = buffers.emplace_back();
			auto cmdBuffer = renderer->BeginSingleTimeCommands(*queue);
			AllocateStagingBuffer(device);
			stagingBuffer.WriteToMappedData(device, data);
			multiBuffer.buffer.Allocate(device, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, false);
			Buffer::Copy(device, cmdBuffer, stagingBuffer.buffer, multiBuffer.buffer.buffer, size, 0, allocationIndex * size);
			renderer->EndSingleTimeCommands(*queue, cmdBuffer);
			multiBuffer.allocations[allocationIndex] = true;
			goto Return;
		}
		
		CopyData: {
			auto& multiBuffer = buffers[bufferIndex];
			auto cmdBuffer = renderer->BeginSingleTimeCommands(*queue);
			AllocateStagingBuffer(device);
			stagingBuffer.WriteToMappedData(device, data);
			Buffer::Copy(device, cmdBuffer, stagingBuffer.buffer, multiBuffer.buffer.buffer, size, 0, allocationIndex * size);
			renderer->EndSingleTimeCommands(*queue, cmdBuffer);
		}
		
		Return: return {bufferIndex, allocationIndex, (int)(allocationIndex * size)};
	}
	
	void Free(BufferPoolAllocation allocation) {
		buffers[allocation.bufferIndex].allocations[allocation.allocationIndex] = false;
	}
	
	void FreePool(Device* device) {
		FreeStagingBuffer(device);
		for (auto& bufferAllocation : buffers) {
			bufferAllocation.buffer.Free(device);
		}
		buffers.clear();
	}
};

// Buffer pools
DeviceLocalBufferPool<VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, nbVerticesPerChunk*sizeof(Vertex), 128/*chunks*/> vertexBufferPool {};
DeviceLocalBufferPool<VK_BUFFER_USAGE_INDEX_BUFFER_BIT, nbIndicesPerChunk*sizeof(uint32_t), 128/*chunks*/> indexBufferPool {};

struct Planet {
	#pragma region Constructor arguments
	double radius; // top of atmosphere (maximum radius)
	double solidRadius; // standard radius of solid surface (AKA sea level, surface height can go below or above)
	double heightVariation; // half the total variation (surface height is +- heightVariation)
	glm::dvec3 absolutePosition; // position of planet relative to world (star system center)
	#pragma endregion
	
	glm::dvec3 cameraPos {0};
	
	struct Chunk {
	
		#pragma region Constructor arguments
		Planet* planet;
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
		double distanceFromCamera = 0;
		#pragma endregion
		
		#pragma region States
		std::atomic<bool> active = false;
		std::atomic<bool> render = false;
		std::atomic<bool> allocated = false;
		std::atomic<bool> shouldBeGenerated = false;
		std::atomic<bool> meshGenerated = false;
		std::atomic<bool> meshGenerating = false;
		// mutable std::mutex stateMutex;
		#pragma endregion
		
		#pragma region Data
		std::array<Vertex, nbVerticesPerChunk> vertices {};
		std::array<uint32_t, nbIndicesPerChunk> indices {};
		std::vector<Chunk*> subChunks {};
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
				distanceFromCamera = glm::min(distanceFromCamera, glm::distance(planet->cameraPos, topLeftPos));
			if (distanceFromCamera > chunkSize/2.0)
				distanceFromCamera = glm::min(distanceFromCamera, glm::distance(planet->cameraPos, topRightPos));
			if (distanceFromCamera > chunkSize/2.0)
				distanceFromCamera = glm::min(distanceFromCamera, glm::distance(planet->cameraPos, bottomLeftPos));
			if (distanceFromCamera > chunkSize/2.0)
				distanceFromCamera = glm::min(distanceFromCamera, glm::distance(planet->cameraPos, bottomRightPos));
			if (distanceFromCamera < chunkSize/2.0)
				distanceFromCamera = glm::max(distanceFromCamera, chunkSize/2.0);
		}
		
		Chunk(Planet* planet, int face, int level, glm::dvec3 topLeft, glm::dvec3 topRight, glm::dvec3 bottomLeft, glm::dvec3 bottomRight)
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
			for (auto* subChunk : subChunks) delete subChunk;
		}
	
		void GenerateAsync() {
			meshGenerating = true;
			static v4d::processing::ThreadPoolPriorityQueue<Chunk*> chunkGenerator ([](Chunk* chunk){
				chunk->Generate();
			}, [](Chunk* a, Chunk* b) {
				return a->distanceFromCamera > b->distanceFromCamera;
			});
			chunkGenerator.RunThreads(4);
			chunkGenerator.Enqueue(this);
		}
		
		void CancelMeshGeneration() {
			shouldBeGenerated = false;
			meshGenerating = false;
		}
		
		int genRow = 0;
		int genCol = 0;
		int genIndexIndex = 0;
		void Generate() {
			if (!shouldBeGenerated) return;
			
			auto [faceDir, topDir, rightDir] = GetFaceVectors(face);
			
			for (; genRow <= vertexSubdivisionsPerChunk; ++genRow) {
				for (; genCol <= vertexSubdivisionsPerChunk; ++genCol) {
					
					uint32_t topLeftIndex = (vertexSubdivisionsPerChunk+1) * genRow + genCol;
					
					glm::dvec3 topOffset = glm::mix(topLeft - center, bottomLeft - center, double(genRow)/vertexSubdivisionsPerChunk);
					glm::dvec3 leftOffset =	glm::mix(topLeft - center, topRight - center, double(genCol)/vertexSubdivisionsPerChunk);
					
					glm::dvec3 pos = Spherify(center + topDir*topOffset + rightDir*leftOffset);
					
					// std::lock_guard lock(stateMutex);
					
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
					
					if (!shouldBeGenerated) return;
				}
				genCol = 0;
			}
			
			// std::lock_guard lock(stateMutex);
			meshGenerated = true;
		}
		
		void Process() {
			
		}
		
		void FreeBuffers() {
			if (allocated) {
				vertexBufferPool.Free(vertexBufferAllocation);
				indexBufferPool.Free(indexBufferAllocation);
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
			CancelMeshGeneration();
			active = false;
			render = false;
			FreeBuffers();
			if (subChunks.size() > 0) {
				for (auto* subChunk : subChunks) {
					subChunk->Remove();
				}
			}
		}
		
		void BeforeRender(Renderer* renderer, Device* device, Queue* queue) {
			RefreshDistanceFromCamera();
			
			// Angle Culling
			bool chunkVisibleByAngle = 
				glm::dot(planet->cameraPos - centerPosLowestPoint, centerPos) > 0.0 ||
				glm::dot(planet->cameraPos - topLeftPosLowestPoint, topLeftPos) > 0.0 ||
				glm::dot(planet->cameraPos - topRightPosLowestPoint, topRightPos) > 0.0 ||
				glm::dot(planet->cameraPos - bottomLeftPosLowestPoint, bottomLeftPos) > 0.0 ||
				glm::dot(planet->cameraPos - bottomRightPosLowestPoint, bottomRightPos) > 0.0 ;
			
			// std::lock_guard lock(stateMutex);
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
						subChunk->BeforeRender(renderer, device, queue);
						if (subChunk->shouldBeGenerated && !subChunk->meshGenerated) {
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
						shouldBeGenerated = true;
						if (!meshGenerated && !meshGenerating) GenerateAsync();
						if (meshGenerated) {
							vertexBufferAllocation = vertexBufferPool.Allocate(renderer, device, queue, vertices.data());
							indexBufferAllocation = indexBufferPool.Allocate(renderer, device, queue, indices.data());
							allocated = true;
							render = true;
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
	
	Planet(
		double radius,
		double solidRadius,
		double heightVariation,
		glm::dvec3 absolutePosition
	) : radius(radius),
		solidRadius(solidRadius),
		heightVariation(heightVariation),
		absolutePosition(absolutePosition)
	{
		GenerateBaseChunks();
	}
	
	~Planet() {
		for (auto* chunk : chunks) delete chunk;
	}
	
	void GenerateBaseChunks() {
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
	
};

std::vector<Planet*> planets {};

class PlanetShaderPipeline : public RasterShaderPipeline {
public:
	using RasterShaderPipeline::RasterShaderPipeline;
	
	glm::dmat4 viewMatrix {1};

	struct PlanetChunkPushConstant { // max 128 bytes
		glm::mat4 modelView; // 64
		glm::vec4 testColor; // 16
	} planetChunkPushConstant {};
	
	void RenderChunk(Device* device, VkCommandBuffer cmdBuffer, Planet::Chunk* chunk) {
		// std::lock_guard lock(chunk->stateMutex);
		if (chunk->active) {
			if (chunk->render) {
				planetChunkPushConstant.modelView = viewMatrix * glm::translate(glm::dmat4(1), chunk->planet->absolutePosition + chunk->centerPos);
				planetChunkPushConstant.testColor = chunk->testColor;
				PushConstant(device, cmdBuffer, &planetChunkPushConstant);
				SetData(
					vertexBufferPool[chunk->vertexBufferAllocation.bufferIndex],
					chunk->vertexBufferAllocation.bufferOffset,
					indexBufferPool[chunk->indexBufferAllocation.bufferIndex],
					chunk->indexBufferAllocation.bufferOffset,
					nbIndicesPerChunk
				);
				Render(device, cmdBuffer);
			} else {
				// Render subChunks recursively
				for (auto* subChunk : chunk->subChunks) {
					RenderChunk(device, cmdBuffer, subChunk);
				}
			}
		}
	}
	
	using RasterShaderPipeline::Execute;
	void Execute(Device* device, VkCommandBuffer cmdBuffer) override {
		Bind(device, cmdBuffer);
		for (auto* planet : planets) {
			for (auto* chunk : planet->chunks) {
				RenderChunk(device, cmdBuffer, chunk);
			}
		}
	}

};

class Planets : public v4d::modules::Rendering {
	
	#pragma region Graphics
	PipelineLayout planetPipelineLayout;
	PlanetShaderPipeline planetShader {planetPipelineLayout, {
		"modules/incubator_galaxy4d/assets/shaders/planets.vert",
		"modules/incubator_galaxy4d/assets/shaders/planets.wireframe.geom",
		"modules/incubator_galaxy4d/assets/shaders/planets.surface.frag",
	}};
	#pragma endregion
	
public:

	// // Executed when calling InitRenderer() on the main Renderer
	// void Init() override {}
	
	void InitLayouts(std::vector<DescriptorSet*>& descriptorSets) override {
		auto* planetDescriptorSet_1 = descriptorSets.emplace_back(new DescriptorSet(1));
		planetPipelineLayout.AddDescriptorSet(descriptorSets[0]);
		planetPipelineLayout.AddDescriptorSet(planetDescriptorSet_1);
		planetPipelineLayout.AddPushConstant<PlanetShaderPipeline::PlanetChunkPushConstant>(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
	}
	void ConfigureShaders(std::unordered_map<std::string, std::vector<RasterShaderPipeline*>>& shaders) override {
		shaders["opaqueRasterization"].push_back(&planetShader);
		
		planetShader.rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
		planetShader.inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		planetShader.depthStencilState.depthWriteEnable = VK_TRUE;
		planetShader.depthStencilState.depthTestEnable = VK_TRUE;
		planetShader.AddVertexInputBinding(sizeof(Vertex), VK_VERTEX_INPUT_RATE_VERTEX, {
			{0, offsetof(Vertex, Vertex::pos), VK_FORMAT_R32G32B32A32_SFLOAT},
			{1, offsetof(Vertex, Vertex::normal), VK_FORMAT_R32G32B32A32_SFLOAT},
			{2, offsetof(Vertex, Vertex::uv), VK_FORMAT_R32G32_SFLOAT},
		});
	}
	
	// // Executed when calling their respective methods on the main Renderer
	// void ReadShaders() override {}
	
	void LoadScene() override {
		planets.push_back(new Planet{
			24000000,
			23900000,
			10000,
			// {0, 21740000, -10000000}
			{0, 28000000, -10000000}
			// {20000000, 20000000, -20000000}
		});
	}
	
	void UnloadScene() override {
		for (auto* planet : planets) delete planet;
	}
	
	// // Executed when calling LoadRenderer()
	// void ScorePhysicalDeviceSelection(int& score, PhysicalDevice*) override {}
	// // after selecting rendering device and queues
	// void Info() override {}
	// void CreateResources() {}
	// void DestroyResources() override {}
	// void AllocateBuffers() override {}
	
	void FreeBuffers() override {
		for (auto* planet : planets) {
			for (auto* chunk : planet->chunks) {
				chunk->Remove();
			}
		}
		vertexBufferPool.FreePool(renderingDevice);
		indexBufferPool.FreePool(renderingDevice);
	}
	
	void CreatePipelines() override {
		planetPipelineLayout.Create(renderingDevice);
	}
	void DestroyPipelines() override {
		planetPipelineLayout.Destroy(renderingDevice);
	}
	
	// // Rendering methods potentially executed on each frame
	// void RunDynamicGraphicsTop(VkCommandBuffer) override {}
	// void RunDynamicGraphicsBottom(VkCommandBuffer) override {}
	// void RunDynamicLowPriorityCompute(VkCommandBuffer) override {}
	// void RunDynamicLowPriorityGraphics(VkCommandBuffer) override {}
	
	// Executed before each frame
	void FrameUpdate(uint imageIndex, glm::dmat4& projection, glm::dmat4& view) override {
		glm::dvec4 cameraAbsolutePosition = glm::inverse(view)[3];
		planetShader.viewMatrix = view;
		
		// Planets
		for (auto* planet : planets) {
			planet->cameraPos = glm::dvec3(cameraAbsolutePosition)/cameraAbsolutePosition.w - planet->absolutePosition;
			for (auto* chunk : planet->chunks) {
				chunk->BeforeRender(renderer, renderingDevice, transferQueue);
			}
		}
	}
	void LowPriorityFrameUpdate() override {
		for (auto* planet : planets) {
			for (auto* chunk : planet->chunks) {
				chunk->Process();
			}
		}
	}
	
};
