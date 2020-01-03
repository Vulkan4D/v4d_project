#version 460 core

precision highp int;
precision highp float;
precision highp sampler2D;

struct PlanetInfo {
	double radius;
};

layout(set = 1, binding = 0) uniform Planets {
	PlanetInfo planets[255];
};

layout(std430, push_constant) uniform PlanetChunk {
	dvec3 planetPosition;
	int planetIndex;
	int chunkIndex;
	dvec3 chunkPosOnPlanet;
	float chunkSize;
} chunk;

##################################################################
#shader vert

#include "incubator_rendering/assets/shaders/_v4d_baseDescriptorSet.glsl"
#include "incubator_rendering/assets/shaders/_cube.glsl"

layout(location = 0) out vec3 posOnChunk;
layout(location = 1) out vec3 posRelativeToCamera;
layout(location = 2) out flat int inside;

void main() 
{
	// PlanetInfo planet = planets[chunk.planetIndex];
	posOnChunk = GetVertexPosOnCube() * chunk.chunkSize;
	dvec3 chunkAbsolutePosition = chunk.planetPosition + chunk.chunkPosOnPlanet;
	dvec3 absolutePos = chunkAbsolutePosition + dvec3(posOnChunk);
	posRelativeToCamera = vec3(absolutePos - cameraUBO.absolutePosition.xyz);
	gl_Position = cameraUBO.projection * vec4(cameraUBO.origin * dmat4(cameraUBO.relativeView) * dvec4(absolutePos, 1));
	
	inside = (
		cameraUBO.absolutePosition.x >= (chunkAbsolutePosition.x - chunk.chunkSize/1.99) && 
		cameraUBO.absolutePosition.x <= (chunkAbsolutePosition.x + chunk.chunkSize/1.99) && 
		cameraUBO.absolutePosition.y >= (chunkAbsolutePosition.y - chunk.chunkSize/1.99) && 
		cameraUBO.absolutePosition.y <= (chunkAbsolutePosition.y + chunk.chunkSize/1.99) && 
		cameraUBO.absolutePosition.z >= (chunkAbsolutePosition.z - chunk.chunkSize/1.99) && 
		cameraUBO.absolutePosition.z <= (chunkAbsolutePosition.z + chunk.chunkSize/1.99) 
	)? 1:0;
}

##################################################################

#shader frag

layout(location = 0) in vec3 posOnChunk;
layout(location = 1) in vec3 posRelativeToCamera;
layout(location = 2) in flat int inside;

layout(location = 0) out vec4 out_color;

#include "incubator_rendering/assets/shaders/_v4dnoise.glsl"

const int MAX_STEPS = 1000;
const float stepFactor = 0.01;
const float insidePlanetSphereThreshold = 0.001;

double GetHeightMap(PlanetInfo planet, dvec3 pos) {
	// return -0.2;
	return double((FastSimplexFractal(vec3(pos/1.5), 12)/2) - 5);
	// return Simplex(pos)/10 - 3 + Simplex(pos*10)/20 + Simplex(pos*25)/50;
}

float GetAtmosphereDensity(PlanetInfo planet, dvec3 pos) {
	return float(smoothstep(planet.radius, 0, length(pos))) * 30;
}

vec3 GetTerrainColor(PlanetInfo planet, dvec3 pos) {
	return vec3(0.5,0.35,0.1) * float(smoothstep(planet.radius * .94, planet.radius * .96, length(pos)));
}

void main() {
	const PlanetInfo planet = planets[chunk.planetIndex];
	dvec3 posOnPlanet = chunk.chunkPosOnPlanet + dvec3(posOnChunk);
	float distanceFromCamera = length(posRelativeToCamera);
	const float maxDistanceFromCamera = distanceFromCamera + chunk.chunkSize;
	vec3 rayDirection = normalize(posRelativeToCamera);
	
	// "Cull" face depending on inside or outside
	if (dot(rayDirection, normalize(posOnChunk)) > 0) {
		if (inside > 0) {
			posOnPlanet -= rayDirection * distanceFromCamera;
			distanceFromCamera = 0;
		} else {
			out_color = vec4(0);
			return;
		}
	} else if (inside > 0) {
		out_color = vec4(0);
		return;
	}
	
	
	// Inside Planet Sphere
	bool insideSphere = false;
	for (int i = 0; i < MAX_STEPS; i++) {
		float stepSize = max(0.01, distanceFromCamera * stepFactor);
		if (distanceFromCamera > maxDistanceFromCamera) break;
		if (length(posOnPlanet) < planet.radius) {
			insideSphere = true;
			break;
		}
		distanceFromCamera += stepSize;
		posOnPlanet += rayDirection * stepSize;
	}
	if (!insideSphere) {
		out_color = vec4(0);
		return;
	}
	
	vec3 color = vec3(0);
	vec3 atmosphereColor = vec3(0);
	
	// Terrain/Atmosphere
	for (int i = 0; i < MAX_STEPS; i++) {
		if (distanceFromCamera > maxDistanceFromCamera) break;
		if (length(posOnPlanet) > planet.radius) break;
		double heightMap = GetHeightMap(planet, posOnPlanet);
		double dist = length(posOnPlanet) - (planet.radius + heightMap);
		float stepSize = max(0.001, distanceFromCamera * stepFactor);
		float threshold = max(insidePlanetSphereThreshold, insidePlanetSphereThreshold * distanceFromCamera);
		
		if (length(posOnPlanet) < planet.radius) {
			atmosphereColor += GetAtmosphereDensity(planet, posOnPlanet) * normalize(vec3(.1,.1,.13)) * stepSize;
			if (length(atmosphereColor) > 1.9) break;
		}
		if (dist < threshold) {
			// for (int j = 0; j < 10 && dist < -threshold; j++) {
			// 	distanceFromCamera -= stepSize / 10;
			// 	posOnPlanet -= rayDirection * stepSize / 10;
			// 	heightMap = GetHeightMap(planet, posOnPlanet);
			// 	dist = length(posOnPlanet) - (planet.radius + heightMap);
			// }
			color = GetTerrainColor(planet, posOnPlanet);
			break;
		}
		distanceFromCamera += stepSize;
		posOnPlanet += rayDirection * stepSize;
	}
	
	out_color = vec4(color + pow(min(vec3(1), atmosphereColor), vec3(2)), 1);
}

