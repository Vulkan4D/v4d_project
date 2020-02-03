#version 460 core
#extension GL_ARB_enhanced_layouts : enable

precision highp int;
precision highp float;
precision highp sampler2D;

#include "Camera.glsl"

layout(std430, push_constant) uniform PlanetAtmosphere{
	mat4 modelViewMatrix;
	dvec3 absolutePosition;
	float innerRadius;
	float outerRadius;
	float cameraAltitudeAboveTerrain;
	float cameraDistanceFromPlanet;
} planetAtmosphere;

struct V2F {
	vec3 pos;
	// vec3 normal;
};

vec3 ViewSpaceNormal(vec3 normal) {
	return normalize(transpose(inverse(mat3(planetAtmosphere.modelViewMatrix))) * normal);
}

double GetTrueDistanceFromDepthBuffer(double depth) {
	return 2.0 * (camera.zfar * camera.znear) / (camera.znear + camera.zfar - (depth * 2.0 - 1.0) * (camera.znear - camera.zfar));
}

##################################################################
#shader vert

layout(location = 0) in vec4 pos;

layout(location = 0) out V2F v2f;

void main() {
	gl_Position = mat4(camera.projectionMatrix) * planetAtmosphere.modelViewMatrix * vec4(pos.xyz, 1);
	
	v2f.pos = (planetAtmosphere.modelViewMatrix * vec4(pos.xyz, 1)).xyz;
	// v2f.pos = pos.xyz;
	
	// v2f.normal = ViewSpaceNormal(normalize(pos.xyz));
}

##################################################################
#shader frag

vec4 SampleTexture(sampler2D tex) {
	return texture(tex, gl_FragCoord.st / vec2(textureSize(tex, 0)));
}

// layout(set = 1, binding = 0) uniform highp sampler2D depthImage;
layout(set = 1, input_attachment_index = 1, binding = 1) uniform highp subpassInput gBuffer_position;
layout(location = 0) in V2F v2f;

layout(location = 0) out vec4 color;

void main() {
	float depthDistance = subpassLoad(gBuffer_position).w;
	vec3 p = -planetAtmosphere.modelViewMatrix[3].xyz;
	vec3 d = normalize(v2f.pos);
	float r = planetAtmosphere.outerRadius;
	float b = -dot(p,d);
	float det = sqrt(b*b - dot(p,p) + r*r);
	float diff = det * 2;
	float dist = b - det;
	// float dist2 = b + det;
		
	dist = max(0.0, dist);
	
	float atm = (depthDistance - dist) / (planetAtmosphere.outerRadius-planetAtmosphere.innerRadius);
	
	color = vec4(1,1,1, atm);
	
	// if (depthDistance < 0) color = vec4(1,0,0,1);
}
