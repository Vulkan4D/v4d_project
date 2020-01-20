#pragma once
#include <v4d.h>

#include "PlanetaryTerrain.hpp"

using namespace v4d::graphics;
using namespace v4d::graphics::vulkan;

class PlanetShaderPipeline : public RasterShaderPipeline {
public:
	using RasterShaderPipeline::RasterShaderPipeline;
	
	glm::dmat4 viewMatrix {1};
	
	std::vector<PlanetaryTerrain*>* planets = nullptr;

	struct PlanetChunkPushConstant { // max 128 bytes
		alignas(64) glm::mat4 modelViewMatrix;
		alignas(16) glm::vec4 testColor;
	} planetChunkPushConstant {};
	
	void RenderChunk(Device* device, VkCommandBuffer cmdBuffer, PlanetaryTerrain::Chunk* chunk) {
		if (chunk->active) {
			if (chunk->render) {
				
				//TODO
				double planetRotationAngle = 0;
				glm::dvec3 planetRotationAxis {0,1,0};
				
				planetChunkPushConstant.modelViewMatrix = viewMatrix * glm::rotate(glm::translate(glm::dmat4(1), chunk->planet->absolutePosition + chunk->centerPos), planetRotationAngle, planetRotationAxis);
				planetChunkPushConstant.testColor = chunk->testColor;
				PushConstant(device, cmdBuffer, &planetChunkPushConstant);
				SetData(
					PlanetaryTerrain::vertexBufferPool[chunk->vertexBufferAllocation.bufferIndex],
					chunk->vertexBufferAllocation.bufferOffset,
					PlanetaryTerrain::indexBufferPool[chunk->indexBufferAllocation.bufferIndex],
					chunk->indexBufferAllocation.bufferOffset,
					PlanetaryTerrain::nbIndicesPerChunk
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
		if (planets) {
			for (auto* planet : *planets) {
				for (auto* chunk : planet->chunks) {
					RenderChunk(device, cmdBuffer, chunk);
				}
			}
		}
	}

};
