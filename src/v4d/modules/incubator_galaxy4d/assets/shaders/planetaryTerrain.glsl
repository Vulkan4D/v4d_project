#version 460 core
#extension GL_ARB_enhanced_layouts : enable

precision highp int;
precision highp float;
precision highp sampler2D;

#include "Camera.glsl"

layout(std430, push_constant) uniform PlanetChunk {
	mat4 modelViewMatrix;
	vec4 testColor;
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
	vec3 relPos;
	vec3 normal;
	vec2 uv;
	vec4 color;
};

##################################################################
#shader vert

layout(location = 0) in vec4 pos;
layout(location = 1) in vec4 uv;
layout(location = 2) in vec4 tangentX;
layout(location = 3) in vec4 tangentY;

layout(location = 0) out V2F v2f;

void main() {
	// float altitude = pos.a;
	// float slope = tangentX.w;
	vec3 normal = cross(tangentX.xyz, tangentY.xyz);
	
	v2f.relPos = (planetChunk.modelViewMatrix * vec4(pos.xyz, 1)).xyz;
	v2f.normal = normalize(transpose(inverse(mat3(planetChunk.modelViewMatrix))) * normal);
	v2f.uv = uv.st;
	v2f.color = planetChunk.testColor;
	gl_Position = mat4(camera.projectionMatrix) * vec4(v2f.relPos, 1);
}

##################################################################

#shader wireframe.geom
bool debugMesh = true;
vec4 meshColor = vec4(0);
bool debugNormals = true;
vec4 normalsColor = vec4(0,1,1,1);
float normalsLength = planetChunk.chunkSize / float(planetChunk.vertexSubdivisionsPerChunk) / 2.0;
#include "wireframe.geom"

##################################################################

#shader surface.frag

#include "incubator_rendering/assets/shaders/_v4dnoise.glsl"

#include "gBuffers_out.glsl"

layout(location = 0) in V2F v2f;

void main() {
	
	vec3 normalNoise = normalize(
		vec3(
			Noise(v2f.uv*60.0+0.865) + Noise(v2f.uv*400.0+0.2685)*0.7 + Noise(v2f.uv*1000.0+20.85)*0.4 + Noise(v2f.uv*2500.0+201.85)*0.2, 
			Noise(v2f.uv*60.0+24.5) + Noise(v2f.uv*400.0+21.5)*0.7 + Noise(v2f.uv*1000.0+150.5)*0.4 + Noise(v2f.uv*2500.0+300.5)*0.2, 
			Noise(v2f.uv*60.0-41.12) + Noise(v2f.uv*400.0-50.12)*0.7 + Noise(v2f.uv*1000.0-140.12)*0.4 + Noise(v2f.uv*2500.0-1402.12)*0.2
		)
	);
	
	GBuffers gBuffers;
	
	gBuffers.albedo = v2f.color;
	gBuffers.normal = normalize(mix(v2f.normal, v2f.normal * normalNoise, 0.5));
	gBuffers.roughness = 0;
	gBuffers.metallic = 0;
	gBuffers.scatter = 0;
	gBuffers.occlusion = 0;
	gBuffers.emission = vec3(0);
	gBuffers.position = v2f.relPos;
	
	WriteGBuffers(gBuffers);
}
