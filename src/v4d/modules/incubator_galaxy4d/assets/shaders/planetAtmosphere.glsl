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
};

vec3 ViewSpaceNormal(vec3 normal) {
	return normalize(transpose(inverse(mat3(planetAtmosphere.modelViewMatrix))) * normal);
}

double GetTrueDistanceFromDepthBuffer(double depth) {
	return 2.0 * (camera.zfar * camera.znear) / (camera.znear + camera.zfar - (depth * 2.0 - 1.0) * (camera.znear - camera.zfar));
}

float linearstep(float a, float b, float x) {
	if (b == a) return (x >= a ? 1 : 0);
	return (x - a) / (b - a);
}
// Ease Functions	https://chicounity3d.wordpress.com/2014/05/23/how-to-lerp-like-a-pro/
// float smooth(float t) {
// 	return t * t * (3 - 2 * t);
// }
float smoother(float t) {
	return t * t * t * (t * (t * 6 - 15) + 10);
}
float easeIn(float t) {
	return 1 - cos(t * 3.141592654 * 0.5);
}
float easeOut(float t) {
	return sin(t * 3.141592654 * 0.5);
}


##################################################################
#shader vert

layout(location = 0) in vec4 pos;

layout(location = 0) out V2F v2f;

void main() {
	gl_Position = mat4(camera.projectionMatrix) * planetAtmosphere.modelViewMatrix * vec4(pos.xyz, 1);
	v2f.pos = (planetAtmosphere.modelViewMatrix * vec4(pos.xyz, 1)).xyz;
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

const int RAYMARCH_STEPS = 5; // minimum of 3

// To put in push constant
float density = 0.5;
float densityPow = 2.0;

void main() {
	float depthDistance = subpassLoad(gBuffer_position).w;
	if (depthDistance == 0) depthDistance = float(camera.zfar);
	vec3 p = -planetAtmosphere.modelViewMatrix[3].xyz; // equivalent to cameraPosition - spherePosition (or negative position of sphere in view space)
	vec3 d = normalize(v2f.pos); // direction from camera to hit points (or direction to hit point in view space)
	float r = planetAtmosphere.outerRadius;
	float b = -dot(p,d);
	float det = sqrt(b*b - dot(p,p) + r*r);
	
	float distBegin = max(0.1, min(b - det, length(v2f.pos)));
	float distEnd = max(min(depthDistance, b + det), distBegin + 0.01);
	// Position on sphere
	vec3 begin = (d * distBegin) + p;
	vec3 end = (d * distEnd) + p;
	
	// Ray Marching atmospheric density
	float atm = 0;
	float distanceBetweenSteps = distance(begin, end) / float(RAYMARCH_STEPS);
	float distanceDensity = pow(min(1.0, distanceBetweenSteps / (planetAtmosphere.outerRadius - planetAtmosphere.innerRadius)), 0.5);
	for (int i = 0; i < RAYMARCH_STEPS; ++i) {
		vec3 posOnSphere = mix(begin, end, float(i)/float(RAYMARCH_STEPS-1));
		float altitude = length(posOnSphere);
		float altitudeDensity = pow(min(1.0, mix(0.0, 1.0, smoothstep(planetAtmosphere.outerRadius, planetAtmosphere.innerRadius, altitude))), densityPow);
		atm += altitudeDensity * distanceDensity * density;
	}
	
	color = vec4(0.9,0.9,1.0, min(1.0, atm));
}
