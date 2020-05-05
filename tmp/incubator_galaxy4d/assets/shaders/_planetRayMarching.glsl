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

#common .*frag

#include "incubator_rendering/assets/shaders/_v4dnoise.glsl"

layout(location = 0) in V2F {
	flat dvec3 posOnPlanet;
	smooth vec3 posOnTriangle;
};

layout(location = 0) out vec4 out_color;

double GetHeightMap(dvec3 pos) {
	// return -40000;
	return double((FastSimplexFractal(vec3(pos/4000), 12)*5000) - 100000);
}

float GetAtmosphereDensity(dvec3 pos) {
	return float(smoothstep(planet.radius, planet.radius*.5, length(pos))) * 0.1;
}

vec3 GetTerrainColor(dvec3 pos) {
	return vec3(0.03) + vec3(0.5,0.35,0.1) * float(smoothstep(planet.radius * .9957, planet.radius * .996, length(pos)));
}

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

// #shader geom

// layout(triangles) in;
// layout(line_strip, max_vertices = 6) out;

// void main() {
// 	gl_Position = gl_in[0].gl_Position;
// 	EmitVertex();
// 	gl_Position = gl_in[1].gl_Position;
// 	EmitVertex();
// 	EndPrimitive();
	
// 	gl_Position = gl_in[1].gl_Position;
// 	EmitVertex();
// 	gl_Position = gl_in[2].gl_Position;
// 	EmitVertex();
// 	EndPrimitive();
	
// 	gl_Position = gl_in[2].gl_Position;
// 	EmitVertex();
// 	gl_Position = gl_in[0].gl_Position;
// 	EmitVertex();
// 	EndPrimitive();
// }

##################################################################

#shader distancefield.frag

const int MAX_STEPS = 1000;
const float stepFactor = 0.01;

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
	
	// if (cameraIsOutsideOfRadius) {
	// 	rayStart = rayEnd - dir * r * 2.0 * dot(normalize(rayEnd), dir);
	// 	distanceFromCamera = distance(rayStart+s, c);
	// 	rayMaxDistance = distance(rayStart, rayEnd);
	// }

	vec3 color = vec3(0);
	dvec3 atmosphereColor = vec3(0);
	
	double currentDistance = 0.01;
	
	int nbSteps = 0;
	
	double stepSize = 0.1;
	
	// Terrain/Atmosphere
	for (int i = 0; i < MAX_STEPS; i++) {
		nbSteps++;
		
		stepSize = max(stepSize, distanceFromCamera * stepFactor);
		currentDistance += stepSize;
		if (currentDistance > rayMaxDistance) break;
		distanceFromCamera += stepSize;
		
		dvec3 pos = rayStart + dir*currentDistance;
		
		double heightMap = GetHeightMap(pos);
		
		double dist = length(pos) - (r + heightMap);
		
		if (dist < stepSize*0.5) {
			break;
		}
	}
	
	out_color = vec4(stepSize, 0, rayMaxDistance, currentDistance);
}


##################################################################

#shader frag

const int MAX_STEPS = 1000;
const float stepFactor = 0.01;

layout(set = 1, binding = 0) uniform sampler2D distanceField;

void main() {
	//TODO consider planet rotation
	
	const dvec3 hitPosOnPlanet = posOnPlanet + dvec3(posOnTriangle);
	const dvec3 c = cameraUBO.absolutePosition.xyz;
	const double r = planet.radius;
	const dvec3 s = planet.absolutePosition;
	const dvec3 dir = normalize(s + hitPosOnPlanet - c);
	const dvec3 rayStart = c - s;
	dvec3 rayEnd = hitPosOnPlanet;
	double rayMaxDistance = distance(rayStart, rayEnd);
	
	vec2 uv = vec2(
		gl_FragCoord.s / cameraUBO.screenWidth,
		gl_FragCoord.t / cameraUBO.screenHeight
	);
	ivec2 texSize = textureSize(distanceField, 0);
	vec4 tex = vec4(0,0,0,1e100);
	for (int i = -1; i <= 1; i++) {
		for (int j = -1; j <= 1; j++) {
			vec4 texTmp = texture(distanceField, uv + vec2(1./texSize.s * i, 1./texSize.t * j));
			if (tex.a >= texTmp.a) tex = texTmp;
		}
	}
	double currentDistance = max(0.01, tex.a);
	double distanceFromCamera = currentDistance + 0.01;
	
	// out_color = vec4(currentDistance/40000);
	// out_color = vec4(tex.r/10000000);
	// return;
	
	int nbSteps = 0;
	
	vec3 color = vec3(0);
	dvec3 atmosphereColor = vec3(0);
	
	// Terrain/Atmosphere
	for (int i = 0; i < MAX_STEPS; i++) {
		nbSteps++;
		
		double stepSize = max(0.01, distanceFromCamera * stepFactor - 0.01);
		currentDistance += stepSize;
		if (currentDistance > rayMaxDistance) break;
		distanceFromCamera += stepSize;
		
		dvec3 pos = rayStart + dir*currentDistance;
		
		double heightMap = GetHeightMap(pos);
		
		double dist = length(pos) - (r + heightMap);
		
		atmosphereColor += GetAtmosphereDensity(pos) * normalize(vec3(.1,.1,.13)) * stepSize;
		
		if (dist < stepSize) {
			color = GetTerrainColor(pos);
			break;
		}
	}
	
	out_color = vec4(color + pow(min(vec3(1), vec3(atmosphereColor)), vec3(2)), 1);
	
	// out_color = mix(vec4(0,1,0,1), vec4(1,0,0,1), float(nbSteps)/MAX_STEPS);
	// out_color = vec4(currentDistance/100000);
}

