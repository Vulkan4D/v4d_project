#pragma once
#include <v4d.h>

using namespace v4d::scene;
using namespace v4d::graphics;
using namespace v4d::graphics::vulkan;

class PlanetAtmosphereShaderPipeline : public RasterShaderPipeline {
public:
	using RasterShaderPipeline::RasterShaderPipeline;
	
	Camera* camera = nullptr;
	std::vector<LightSource*> lightSources {};
	std::vector<PlanetTerrain*> planets {};
	int atmospherePushConstantIndex = 0;
	
	const static int NB_SUNS = 3;

	struct PlanetAtmospherePushConstant { // max of 128 bytes reached, no more space in this struct
		alignas(64) glm::mat4 modelViewMatrix;
		alignas(64) glm::vec4 suns[NB_SUNS];
		alignas(4) uint color;
		alignas(4) float innerRadius;
		alignas(4) float outerRadius;
		alignas(4) float densityFactor;
	} planetAtmospherePushConstant {};
	
	static glm::vec4 CompactSunInfo(glm::vec3 sunDir, float sunIntensity, glm::vec3 sunColor) {
		return glm::vec4(sunDir*sunIntensity, CompactVec3ToFloat(sunColor.r, sunColor.g, sunColor.b));
	}
	
	using RasterShaderPipeline::Execute;
	void Execute(Device* device, VkCommandBuffer cmdBuffer) override {
		if (!camera || planets.size() == 0) return;
		
		Bind(device, cmdBuffer);
		
		for (auto* planet : planets) if (planet->atmosphere.radius > 0) {
			
			planetAtmospherePushConstant.modelViewMatrix = camera->viewMatrix * planet->matrix;
			planetAtmospherePushConstant.innerRadius = (float)(planet->solidRadius - planet->heightVariation);
			planetAtmospherePushConstant.outerRadius = (float)planet->radius;
			planetAtmospherePushConstant.densityFactor = planet->atmosphere.densityFactor;
			
			planetAtmospherePushConstant.color = CompactIVec4ToUint(
				(uint)(planet->atmosphere.color.r*255),
				(uint)(planet->atmosphere.color.g*255),
				(uint)(planet->atmosphere.color.b*255),
				0 // ambient
			);
			
			int i = 0;
			for (auto* lightSource : lightSources) {
				auto planetViewSpacePosition = glm::dvec3(planetAtmospherePushConstant.modelViewMatrix[3]);
				double lightDist = glm::distance(glm::dvec3(lightSource->viewSpacePosition), planetViewSpacePosition);
				planetAtmospherePushConstant.suns[i] = CompactSunInfo(
					// Sun direction is from planet center to sun, in view space
					glm::normalize(glm::dvec3(lightSource->viewSpacePosition) - planetViewSpacePosition), 
					lightSource->intensity * float(1.0 / (lightDist*lightDist)), 
					lightSource->color
				);
				if (i++ == 3) break;
			}
			while (i<3) {
				planetAtmospherePushConstant.suns[i++] = {0,0,0,0};
			}
			
			PushConstant(device, cmdBuffer, &planetAtmospherePushConstant, atmospherePushConstantIndex);
			
			SetData(
				&planet->atmosphere.vertexBuffer.deviceLocalBuffer,
				&planet->atmosphere.indexBuffer.deviceLocalBuffer,
				PlanetAtmosphere::nbIndices
			);
			
			Render(device, cmdBuffer);
		}
	}

};
