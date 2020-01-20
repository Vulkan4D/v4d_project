#version 460 core
#extension GL_ARB_enhanced_layouts : enable

precision highp int;
precision highp float;
precision highp sampler2D;

#include "Camera.glsl"

layout(std430, push_constant) uniform PlanetChunk {
	mat4 modelViewMatrix;
	vec4 testColor;
} planetChunk;

struct V2F {
	vec3 relPos;
	vec3 normal;
	vec2 uv;
};

##################################################################
#shader vert

layout(location = 0) in vec4 pos;
layout(location = 1) in vec4 normal;
layout(location = 2) in vec2 uv;

layout(location = 0) out V2F v2f;

void main() {
	v2f.relPos = (planetChunk.modelViewMatrix * vec4(pos.xyz, 1)).xyz;
	v2f.normal = normalize(transpose(inverse(mat3(planetChunk.modelViewMatrix))) * normal.xyz);
	v2f.uv = uv.st;
	gl_Position = mat4(camera.projectionMatrix) * vec4(v2f.relPos, 1);
}

##################################################################

#shader wireframe.geom
#include "wireframe.geom"

##################################################################

#shader surface.frag

layout(location = 0) in V2F v2f;

#include "gBuffers_out.glsl"

void main() {
	GBuffers gBuffers;
	
	gBuffers.albedo = planetChunk.testColor;
	gBuffers.normal = normalize(v2f.normal);
	gBuffers.roughness = 0;
	gBuffers.metallic = 0;
	gBuffers.scatter = 0;
	gBuffers.occlusion = 0;
	gBuffers.emission = vec3(0);
	gBuffers.position = v2f.relPos;
	
	WriteGBuffers(gBuffers);
}
