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
	
	const static int NB_SUNS = 3;

	struct PlanetAtmospherePushConstant { // max of 128 bytes reached, no more space in this struct
		alignas(64) glm::mat4 modelViewMatrix;
		alignas(64) glm::vec4 suns[NB_SUNS];
		alignas(4) uint color;
		alignas(4) float innerRadius;
		alignas(4) float outerRadius;
		alignas(4) float cameraDistanceFromPlanet;
	} planetAtmospherePushConstant {};
	
	static glm::vec4 CompactSunInfo(glm::vec3 sunDir, float sunIntensity, glm::vec3 sunColor) {
		return glm::vec4(sunDir*sunIntensity, CompactVec3ToFloat(sunColor.r, sunColor.g, sunColor.b));
	}
	
	using RasterShaderPipeline::Execute;
	void Execute(Device* device, VkCommandBuffer cmdBuffer) override {
		Bind(device, cmdBuffer);
		if (planets) {
			for (auto* planet : *planets) if (planet->atmosphere) {
				
				planetAtmospherePushConstant.modelViewMatrix = viewMatrix * glm::translate(glm::dmat4(1), planet->absolutePosition);
				planetAtmospherePushConstant.innerRadius = (float)(planet->solidRadius - planet->heightVariation);
				planetAtmospherePushConstant.outerRadius = (float)planet->radius;
				planetAtmospherePushConstant.cameraDistanceFromPlanet = (float)glm::length(planet->cameraPos);
				
				planetAtmospherePushConstant.color = CompactIVec4ToUint(255,255,255,   255/*unused*/  );
				
				for (int i = 0; i < NB_SUNS; ++i) {
					if (planet->suns.size() > i) {
						auto* sun = planet->suns[i];
						planetAtmospherePushConstant.suns[i] = CompactSunInfo(
							// Sun direction is from planet center to sun, in view space
							glm::normalize(glm::dvec3(viewMatrix * glm::dvec4(sun->worldPosition, 1)) - glm::dvec3(planetAtmospherePushConstant.modelViewMatrix[3])), 
							sun->intensity, 
							sun->color
						);
					} else {
						planetAtmospherePushConstant.suns[i] = {0,0,0,0};
					}
				}
				
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
