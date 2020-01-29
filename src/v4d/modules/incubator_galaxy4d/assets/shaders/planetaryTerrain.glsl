#version 460 core
#extension GL_ARB_enhanced_layouts : enable

precision highp int;
precision highp float;
precision highp sampler2D;

#include "Camera.glsl"

layout(std430, push_constant) uniform PlanetChunk {
	mat4 modelViewMatrix;
	ivec3 chunkPos;
	float chunkSize;
	float radius;
	float solidRadius;
	int level;
	bool isLastLevel;
	int vertexSubdivisionsPerChunk;
	float cameraAltitudeAboveTerrain;
	float cameraDistanceFromPlanet;
} planetChunk;

struct V2F {
	vec3 pos;
	float altitude;
	vec3 normal;
	float slope;
	vec3 tangentX;
	// float ???;
	vec2 uv;
	// vec2 ???;
};

##################################################################
#shader vert

layout(location = 0) in vec4 pos;
layout(location = 1) in vec4 uv;
layout(location = 2) in vec4 tangentX;
layout(location = 3) in vec4 normal;

layout(location = 0) out V2F v2f;

void main() {
	gl_Position = mat4(camera.projectionMatrix) * planetChunk.modelViewMatrix * vec4(pos.xyz, 1);
	
	v2f.pos = pos.xyz;
	v2f.altitude = pos.a;
	v2f.normal = normalize(transpose(inverse(mat3(planetChunk.modelViewMatrix))) * normal.xyz);
	v2f.slope = normal.w;
	v2f.tangentX = tangentX.xyz;
	// v2f.??? = tangentX.w;
	v2f.uv = uv.st;
	// v2f.??? = uv.pq;
}

##################################################################

#shader surface.frag

#include "incubator_rendering/assets/shaders/_v4dnoise.glsl"
#include "gBuffers_out.glsl"

layout(location = 0) in V2F v2f;

void main() {
	dvec3 posOnPlanet = dvec3(planetChunk.chunkPos) + dvec3(v2f.pos);
	
	vec3 normalNoise = normalize(
		vec3(
			Noise(v2f.uv*100.0+0.865)*0.4 + Noise(v2f.uv*800.0+0.2685)*0.5 + Noise(v2f.uv*2000.0+20.85)*0.4 + Noise(v2f.uv*5000.0+201.85)*0.2, 
			Noise(v2f.uv*100.0+24.5)*0.4 + Noise(v2f.uv*800.0+21.5)*0.5 + Noise(v2f.uv*2000.0+150.5)*0.4 + Noise(v2f.uv*5000.0+300.5)*0.2, 
			Noise(v2f.uv*100.0-41.12)*0.4 + Noise(v2f.uv*800.0-50.12)*0.5 + Noise(v2f.uv*2000.0-140.12)*0.4 + Noise(v2f.uv*5000.0-1402.12)*0.2
		)
	);
	
	vec3 color = vec3(1)*smoothstep(0.85, 0.98, v2f.slope) + vec3(0.6,0.4,0.2)*smoothstep(0.02, 0.15, 1 - v2f.slope);
	color = vec3(1);
	
	GBuffers gBuffers;
	
	gBuffers.albedo = vec4(color, 1);//planetChunk.isLastLevel? vec4(0,1,0,1) : vec4(1);
	gBuffers.normal = normalize(mix(v2f.normal, v2f.normal * normalNoise, 0.5));
	gBuffers.roughness = 0;
	gBuffers.metallic = 0;
	gBuffers.scatter = 0;
	gBuffers.occlusion = 0;
	gBuffers.emission = vec3(0);
	gBuffers.position = (planetChunk.modelViewMatrix * vec4(v2f.pos, 1)).xyz;
	
	WriteGBuffers(gBuffers);
}
