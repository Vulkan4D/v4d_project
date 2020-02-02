#pragma once
#include <v4d.h>

#include "PlanetTerrain.hpp"

using namespace v4d::graphics;
using namespace v4d::graphics::vulkan;

class PlanetAtmosphereShaderPipeline : public RasterShaderPipeline {
public:
	using RasterShaderPipeline::RasterShaderPipeline;
	
	glm::dmat4 viewMatrix {1};
	
	std::vector<PlanetTerrain*>* planets = nullptr;

	struct PlanetAtmospherePushConstant { // max 128 bytes
		alignas(64) glm::mat4 modelViewMatrix;
		alignas(4) float radius;
		alignas(4) float solidRadius;
		alignas(4) float cameraAltitudeAboveTerrain;
		alignas(4) float cameraDistanceFromPlanet;
	} planetAtmospherePushConstant {};
	
	using RasterShaderPipeline::Execute;
	void Execute(Device* device, VkCommandBuffer cmdBuffer) override {
		Bind(device, cmdBuffer);
		if (planets) {
			for (auto* planet : *planets) if (planet->atmosphere) {
				
				planetAtmospherePushConstant.modelViewMatrix = viewMatrix * glm::translate(glm::dmat4(1), planet->absolutePosition);
				planetAtmospherePushConstant.radius = (float)planet->radius;
				planetAtmospherePushConstant.solidRadius = (float)planet->solidRadius;
				planetAtmospherePushConstant.cameraAltitudeAboveTerrain = (float)planet->cameraAltitudeAboveTerrain;
				planetAtmospherePushConstant.cameraDistanceFromPlanet = (float)glm::length(planet->cameraPos);
				
				PushConstant(device, cmdBuffer, &planetAtmospherePushConstant);
				SetData(
					&planet->atmosphere->vertexBuffer.deviceLocalBuffer,
					&planet->atmosphere->indexBuffer.deviceLocalBuffer,
					PlanetAtmosphere::nbIndices
				);
				// Render(device, cmdBuffer);
			}
		}
	}

};
