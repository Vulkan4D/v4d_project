#version 460 core
#extension GL_ARB_enhanced_layouts : enable

precision highp int;
precision highp float;
precision highp sampler2D;

#include "Camera.glsl"

layout(std430, push_constant) uniform PlanetAtmosphere{
	mat4 modelViewMatrix;
	float radius;
	float solidRadius;
	float cameraAltitudeAboveTerrain;
	float cameraDistanceFromPlanet;
} planetAtmosphere;

struct V2F {
	vec3 pos;
	vec3 normal;
};

vec3 ViewSpaceNormal(vec3 normal) {
	return normalize(transpose(inverse(mat3(planetAtmosphere.modelViewMatrix))) * normal);
}

##################################################################
#shader vert

layout(location = 0) in vec4 pos;
layout(location = 1) in vec4 normal;

layout(location = 0) out V2F v2f;

void main() {
	gl_Position = mat4(camera.projectionMatrix) * planetAtmosphere.modelViewMatrix * vec4(pos.xyz, 1);
	v2f.pos = pos.xyz;
	v2f.normal = normal.xyz;
}

##################################################################
#shader surface.transparent.frag

#include "gBuffers_out.glsl"

layout(location = 0) in V2F v2f;

void main() {
	GBuffers gBuffers;
	
	gBuffers.albedo = vec4(1);
	gBuffers.normal = ViewSpaceNormal(normalize(v2f.normal));
	gBuffers.roughness = 0;
	gBuffers.metallic = 0;
	gBuffers.scatter = 0;
	gBuffers.occlusion = 0;
	gBuffers.emission = vec3(0);
	gBuffers.position = (planetAtmosphere.modelViewMatrix * vec4(v2f.pos, 1)).xyz;
	
	WriteGBuffers(gBuffers);
}
