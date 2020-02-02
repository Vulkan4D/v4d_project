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

	struct PlanetChunkPushConstant { // max 128 bytes
		alignas(64) glm::mat4 modelViewMatrix;
		alignas(16) glm::ivec3 chunkPos;
		alignas(4) float chunkSize;
		alignas(4) float radius;
		alignas(4) float solidRadius;
		alignas(4) int level;
		alignas(4) bool isLastLevel;
		alignas(4) int vertexSubdivisionsPerChunk;
		alignas(4) float cameraAltitudeAboveTerrain;
		alignas(4) float cameraDistanceFromPlanet;
		// alignas(4) float ???;
		alignas(16) glm::vec3 northDir;
		// alignas(4) float ???;
	} planetChunkPushConstant {};
	
	void RenderChunk(Device* device, VkCommandBuffer cmdBuffer, PlanetTerrain::Chunk* chunk) {
		std::scoped_lock lock(chunk->stateMutex, chunk->subChunksMutex);
		if (chunk->active) {
			if (chunk->render) {
				
				//TODO
				double planetRotationAngle = 0;
				glm::dvec3 planetRotationAxis {0,1,0};
				glm::dmat4 planetRotationMatrix = glm::rotate(glm::translate(glm::dmat4(1), chunk->planet->absolutePosition + chunk->centerPos), planetRotationAngle, planetRotationAxis);
				
				planetChunkPushConstant.modelViewMatrix = viewMatrix * planetRotationMatrix;
				planetChunkPushConstant.chunkPos = chunk->centerPos;
				planetChunkPushConstant.chunkSize = (float)chunk->chunkSize;
				planetChunkPushConstant.radius = (float)chunk->planet->radius;
				planetChunkPushConstant.solidRadius = (float)chunk->planet->solidRadius;
				planetChunkPushConstant.level = chunk->level;
				planetChunkPushConstant.isLastLevel = chunk->IsLastLevel();
				planetChunkPushConstant.vertexSubdivisionsPerChunk = PlanetTerrain::vertexSubdivisionsPerChunk;
				planetChunkPushConstant.cameraAltitudeAboveTerrain = (float)chunk->planet->cameraAltitudeAboveTerrain;
				planetChunkPushConstant.cameraDistanceFromPlanet = (float)glm::length(chunk->planet->cameraPos);
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
				std::lock_guard lock(planet->chunksMutex);
				for (auto* chunk : planet->chunks) {
					RenderChunk(device, cmdBuffer, chunk);
				}
			}
		}
	}

};
