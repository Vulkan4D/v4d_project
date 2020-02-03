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
		alignas(32) glm::dvec3 absolutePosition;
		alignas(4) float innerRadius;
		alignas(4) float outerRadius;
		alignas(4) float cameraAltitudeAboveTerrain;
		alignas(4) float cameraDistanceFromPlanet;
	} planetAtmospherePushConstant {};
	
	using RasterShaderPipeline::Execute;
	void Execute(Device* device, VkCommandBuffer cmdBuffer) override {
		Bind(device, cmdBuffer);
		if (planets) {
			for (auto* planet : *planets) if (planet->atmosphere) {
				
				planetAtmospherePushConstant.modelViewMatrix = viewMatrix * glm::translate(glm::dmat4(1), planet->absolutePosition);
				planetAtmospherePushConstant.absolutePosition = planet->absolutePosition;
				planetAtmospherePushConstant.innerRadius = (float)(planet->solidRadius - planet->heightVariation);
				planetAtmospherePushConstant.outerRadius = (float)planet->radius;
				planetAtmospherePushConstant.cameraAltitudeAboveTerrain = (float)planet->cameraAltitudeAboveTerrain;
				planetAtmospherePushConstant.cameraDistanceFromPlanet = (float)glm::length(planet->cameraPos);
				
				PushConstant(device, cmdBuffer, &planetAtmospherePushConstant);
				SetData(
					&planet->atmosphere->vertexBuffer.deviceLocalBuffer,
					&planet->atmosphere->indexBuffer.deviceLocalBuffer,
					PlanetAtmosphere::nbIndices
				);
				Render(device, cmdBuffer);
			}
		}
	}

};
