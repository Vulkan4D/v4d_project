#version 460 core
#extension GL_ARB_enhanced_layouts : enable

precision highp int;
precision highp float;
precision highp sampler2D;

// struct PlanetInfo {
// 	double radius;
// };

// layout(set = 1, binding = 0) uniform Planets {
// 	PlanetInfo planets[255];
// };

layout(std430, push_constant) uniform Planet {
	dvec3 absolutePosition;
	float radius;
} planet;

#include "incubator_rendering/assets/shaders/_v4d_baseDescriptorSet.glsl"

##################################################################
#shader vert

// #include "incubator_rendering/assets/shaders/_cube.glsl"

layout(location = 0) in double posX;
layout(location = 1) in double posY;
layout(location = 2) in double posZ;
layout(location = 3) in vec4 pos;

layout(location = 0) out V2F {
	flat dvec3 posOnPlanet;
	smooth vec3 posOnTriangle;
};

void main() {
	// // PlanetInfo planet = planets[chunk.planetIndex];
	// posOnChunk = dvec3(GetVertexPosOnCube() * chunk.chunkSize);
	// dvec3 chunkAbsolutePosition = chunk.planetPosition + chunk.chunkPosOnPlanet;
	// dvec3 absolutePos = chunkAbsolutePosition + posOnChunk;
	// posRelativeToCamera = vec3(absolutePos - cameraUBO.absolutePosition.xyz);
	// gl_Position = cameraUBO.projection * vec4(cameraUBO.origin * dmat4(cameraUBO.relativeView) * dvec4(absolutePos, 1));
	
	gl_Position = cameraUBO.projection * vec4(cameraUBO.origin * dmat4(cameraUBO.relativeView) * (dvec4(planet.absolutePosition, 1) + dvec4(posX+pos.x, posY+pos.y, posZ+pos.z, 0)));
	posOnPlanet = dvec3(posX, posY, posZ);
	posOnTriangle = pos.xyz;
}

// ##################################################################

#shader geom

layout(triangles) in;
layout(line_strip, max_vertices = 6) out;

void main() {
	gl_Position = gl_in[0].gl_Position;
	EmitVertex();
	gl_Position = gl_in[1].gl_Position;
	EmitVertex();
	EndPrimitive();
	
	gl_Position = gl_in[1].gl_Position;
	EmitVertex();
	gl_Position = gl_in[2].gl_Position;
	EmitVertex();
	EndPrimitive();
	
	gl_Position = gl_in[2].gl_Position;
	EmitVertex();
	gl_Position = gl_in[0].gl_Position;
	EmitVertex();
	EndPrimitive();
}

##################################################################

#shader frag

#include "incubator_rendering/assets/shaders/_v4dnoise.glsl"

layout(location = 0) in V2F {
	flat dvec3 posOnPlanet;
	smooth vec3 posOnTriangle;
};

layout(location = 0) out vec4 out_color;

const int MAX_STEPS = 1000;
const float stepFactor = 0.01;
const float insidePlanetSphereThreshold = 0.01;

double GetHeightMap(dvec3 pos) {
	return double((FastSimplexFractal(vec3(pos/4000), 12)*5000) - 40000);
}

float GetAtmosphereDensity(dvec3 pos) {
	return float(smoothstep(planet.radius, planet.radius*.7, length(pos))) * 0.1;
}

vec3 GetTerrainColor(dvec3 pos) {
	return vec3(0.5,0.35,0.1) * float(smoothstep(planet.radius * .998, planet.radius * .999, length(pos)));
}

void main() {
	//TODO consider planet rotation
	
	const dvec3 hitPosOnPlanet = posOnPlanet + dvec3(posOnTriangle);

	const dvec3 c = cameraUBO.absolutePosition.xyz;
	const double r = planet.radius;
	const dvec3 s = planet.absolutePosition;
	const dvec3 dir = normalize(s + hitPosOnPlanet - c);
	const bool cameraIsOutsideOfRadius = distance(s, c) > r;
	
	dvec3 rayStart = c - s;
	dvec3 rayEnd = hitPosOnPlanet;
	double rayMaxDistance = distance(rayStart, rayEnd);
	double distanceFromCamera = 0.01;
	
	if (cameraIsOutsideOfRadius) {
		rayStart = mix(rayStart, rayEnd - dir * r * 2.0 * dot(normalize(rayEnd), dir), 0.9);
		distanceFromCamera = distance(rayStart+s, c);
		rayMaxDistance = distance(rayStart, rayEnd);
	}

	vec3 color = vec3(0);
	dvec3 atmosphereColor = vec3(0);
	
	double currentDistance = 0.01;
	
	int nbSteps = 0;
	
	// Terrain/Atmosphere
	for (int i = 0; i < MAX_STEPS; i++) {
		nbSteps++;
		
		double stepSize = max(10, distanceFromCamera * stepFactor);
		currentDistance += stepSize;
		// if (currentDistance > rayMaxDistance) break;
		distanceFromCamera += stepSize;
		
		dvec3 pos = rayStart + dir*currentDistance;
		
		double heightMap = GetHeightMap(pos);
		
		double dist = length(pos) - (r + heightMap);
		
		double threshold = max(insidePlanetSphereThreshold, insidePlanetSphereThreshold * distanceFromCamera);
		
		atmosphereColor += GetAtmosphereDensity(pos) * normalize(vec3(.1,.1,.13)) * stepSize;
		// if (length(atmosphereColor) > 1.9) break;
		
		if (dist < threshold) {
			// for (int j = 0; j < 10 && dist < -threshold; j++) {
			// 	distanceFromCamera -= stepSize / 10;
			// 	pos -= dir * stepSize / 10;
			// 	heightMap = GetHeightMap(pos);
			// 	dist = length(pos) - (planet.radius + heightMap);
			// }
			color = GetTerrainColor(pos);
			break;
		}
	}
	
	out_color = vec4(color + pow(min(vec3(1), vec3(atmosphereColor)), vec3(2)), 1);
	
	// out_color = mix(vec4(0,1,0,1), vec4(1,0,0,1), float(nbSteps)/MAX_STEPS);
	// out_color = mix(vec4(0,1,0,1), vec4(1,0,0,1), float(distanceFromCamera/100000));
}

