#pragma once
#include <v4d.h>

#include "PlanetTerrain.hpp"

using namespace v4d::graphics;
using namespace v4d::graphics::vulkan;

class PlanetTerrainShaderPipeline : public RasterShaderPipeline {
public:
	using RasterShaderPipeline::RasterShaderPipeline;
	
	glm::dmat4 viewMatrix {1};
	
	std::vector<PlanetTerrain*>* planets = nullptr;
	Camera* camera = nullptr;

	struct PlanetChunkPushConstant { // max 128 bytes
		alignas(64) glm::mat4 modelViewMatrix;
		alignas(16) glm::ivec3 chunkPos;
		alignas(4) float chunkSize;
		alignas(16) glm::vec3 northDir;
		alignas(4) int vertexSubdivisionsPerChunk;
		alignas(4) bool isLastLevel;
	} planetChunkPushConstant {};
	
	std::tuple<int/*totalChunks*/, int/*activeChunks*/, int/*renderedChunks*/> RenderChunk(Device* device, VkCommandBuffer cmdBuffer, PlanetTerrain::Chunk* chunk) {
		std::scoped_lock lock(chunk->stateMutex, chunk->subChunksMutex);
		if (chunk->active) {
			if (chunk->render) {
				
				// Frustum culling
				if (!camera->IsVisibleInScreen(chunk->planet->absolutePosition + chunk->centerPos, chunk->boundingDistance)) return {1,1,0};
				
				//TODO
				double planetRotationAngle = 0;
				glm::dvec3 planetRotationAxis {0,1,0};
				glm::dmat4 planetRotationMatrix = glm::rotate(glm::translate(glm::dmat4(1), chunk->planet->absolutePosition + chunk->centerPos), planetRotationAngle, planetRotationAxis);
				
				planetChunkPushConstant.modelViewMatrix = viewMatrix * planetRotationMatrix;
				planetChunkPushConstant.chunkPos = chunk->centerPos;
				planetChunkPushConstant.chunkSize = (float)chunk->chunkSize;
				planetChunkPushConstant.isLastLevel = chunk->IsLastLevel();
				planetChunkPushConstant.vertexSubdivisionsPerChunk = PlanetTerrain::vertexSubdivisionsPerChunk;
				planetChunkPushConstant.northDir = glm::normalize(glm::transpose(glm::inverse(glm::dmat3(planetRotationMatrix))) * glm::dvec3(0,1,0));
				
				PushConstant(device, cmdBuffer, &planetChunkPushConstant);
				SetData(
					PlanetTerrain::vertexBufferPool[chunk->vertexBufferAllocation],
					chunk->vertexBufferAllocation.bufferOffset,
					PlanetTerrain::indexBufferPool[chunk->indexBufferAllocation],
					chunk->indexBufferAllocation.bufferOffset,
					PlanetTerrain::nbIndicesPerChunk
				);
				Render(device, cmdBuffer);
				return {1,1,1};
			} else {
				int totalChunks = 1;
				int activeChunks = 0;
				int renderedChunks = 0;
				// Render subChunks recursively
				for (auto* subChunk : chunk->subChunks) {
					auto [a,b,c] = RenderChunk(device, cmdBuffer, subChunk);
					totalChunks += a;
					activeChunks += b;
					renderedChunks += c;
				}
				return {totalChunks, activeChunks, renderedChunks};
			}
		}
		return {1,0,0};
	}
	
	using RasterShaderPipeline::Execute;
	void Execute(Device* device, VkCommandBuffer cmdBuffer) override {
		Bind(device, cmdBuffer);
		if (planets) {
			for (auto* planet : *planets) {
				int totalChunks = 1;
				int activeChunks = 0;
				int renderedChunks = 0;
				std::lock_guard lock(planet->chunksMutex);
				for (auto* chunk : planet->chunks) {
					auto [a,b,c] = RenderChunk(device, cmdBuffer, chunk);
					totalChunks += a;
					activeChunks += b;
					renderedChunks += c;
				}
				planet->totalChunks = totalChunks;
				planet->activeChunks = activeChunks;
				planet->renderedChunks = renderedChunks;
			}
		}
	}

};
