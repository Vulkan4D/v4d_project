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
		
		Return: return {bufferIndex, allocationIndex, allocationIndex * size};
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

// Buffer pool
DeviceLocalBufferPool<VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, nbVerticesPerChunk*sizeof(Vertex), 256/*chunks*/> vertexBufferPool {};
DeviceLocalBufferPool<VK_BUFFER_USAGE_INDEX_BUFFER_BIT, nbIndicesPerChunk*sizeof(uint32_t), 256/*chunks*/> indexBufferPool {};

struct Chunk {
	std::atomic<bool> visible = false;
	
	glm::dvec3 centerPos {0};
	glm::vec4 testColor {1,1,1,1};
	
	// Buffers
	std::atomic<bool> allocated = false;
	std::mutex allocationMutex;
	BufferPoolAllocation vertexBufferAllocation;
	BufferPoolAllocation indexBufferAllocation;
	
};

struct Planet {
	glm::dvec3 absolutePosition {0};
	std::vector<Chunk> chunks {};
};

std::vector<Planet> planets {};

class PlanetShaderPipeline : public RasterShaderPipeline {
public:
	using RasterShaderPipeline::RasterShaderPipeline;
	
	glm::dmat4 viewMatrix {1};

	struct PlanetChunkPushConstant { // max 128 bytes
		glm::mat4 modelView; // 64
		glm::vec4 testColor; // 16
	} planetChunkPushConstant {};
	
	void RenderChunk(Device* device, VkCommandBuffer cmdBuffer, Planet& planet, Chunk& chunk) {
		if (chunk.visible && chunk.allocated) {
			planetChunkPushConstant.modelView = viewMatrix * glm::translate(glm::dmat4(1), planet.absolutePosition + chunk.centerPos);
			planetChunkPushConstant.testColor = chunk.testColor;
			PushConstant(device, cmdBuffer, &planetChunkPushConstant);
			SetData(
				vertexBufferPool[chunk.vertexBufferAllocation.bufferIndex],
				chunk.vertexBufferAllocation.bufferOffset,
				indexBufferPool[chunk.indexBufferAllocation.bufferIndex],
				chunk.indexBufferAllocation.bufferOffset,
				nbIndicesPerChunk
			);
			Render(device, cmdBuffer, 1);
		}
		//TODO render subChunks recursively
	}
	
	using RasterShaderPipeline::Execute;
	void Execute(Device* device, VkCommandBuffer cmdBuffer) override {
		Bind(device, cmdBuffer);
		for (auto& planet : planets) {
			for (auto& chunk : planet.chunks) {
				RenderChunk(device, cmdBuffer, planet, chunk);
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
		
		planetShader.rasterizer.cullMode = VK_CULL_MODE_NONE;
		planetShader.inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		planetShader.depthStencilState.depthWriteEnable = VK_FALSE;
		planetShader.depthStencilState.depthTestEnable = VK_FALSE;
		planetShader.AddVertexInputBinding(sizeof(Vertex), VK_VERTEX_INPUT_RATE_VERTEX, {
			{0, offsetof(Vertex, Vertex::pos), VK_FORMAT_R32G32B32A32_SFLOAT},
			{1, offsetof(Vertex, Vertex::normal), VK_FORMAT_R32G32B32A32_SFLOAT},
		});
	}
	
	// // Executed when calling their respective methods on the main Renderer
	// void ReadShaders() override {}
	// void LoadScene() override {}
	// void UnloadScene() override {}
	
	// // Executed when calling LoadRenderer()
	// void ScorePhysicalDeviceSelection(int& score, PhysicalDevice*) override {}
	// // after selecting rendering device and queues
	// void Info() override {}
	// void CreateResources() {}
	// void DestroyResources() override {}
	// void AllocateBuffers() override {}
	
	void FreeBuffers() override {
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
		planetShader.viewMatrix = view;
	}
	void LowPriorityFrameUpdate() override {
		
	}
	
};
