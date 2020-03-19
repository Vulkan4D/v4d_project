#version 460 core
// #extension GL_EXT_ray_tracing : enable

#define MAX_PLANETS 1

layout(set = 3, binding = 0) readonly buffer PlanetBuffer {dmat4 planets[];};

float planetHeightVariation = 60000;

#############################################################

#common .*map.comp

#include "incubator_rendering/assets/shaders/_noise.glsl"
#include "incubator_rendering/assets/shaders/_v4dnoise.glsl"

layout(set = 1, binding = 0) uniform writeonly imageCube mantleMap[MAX_PLANETS];
layout(set = 1, binding = 1) uniform writeonly imageCube tectonicsMap[MAX_PLANETS];
layout(set = 1, binding = 2) uniform writeonly imageCube heightMap[MAX_PLANETS];
layout(set = 1, binding = 3) uniform writeonly imageCube volcanoesMap[MAX_PLANETS];
layout(set = 1, binding = 4) uniform writeonly imageCube liquidsMap[MAX_PLANETS];

layout(std430, push_constant) uniform Planet{
	int planetIndex;
};

vec3 GetCubeDirection(writeonly imageCube image) {
	const vec2 pixelCenter = vec2(gl_GlobalInvocationID.xy) + vec2(0.5);
	const vec2 uv = pixelCenter / imageSize(image).xy;
	vec2 d = uv * 2.0 - 1.0;
	
	vec3 direction = vec3(0);
	switch (int(gl_GlobalInvocationID.z)) {
		case 0 : // Right (POSITIVE_X)
			direction = vec3( 1, -d.y, -d.x);
		break;
		case 1 : // Left (NEGATIVE_X)
			direction = vec3(-1, -d.y, d.x);
		break;
		case 2 : // Front (POSITIVE_Y)
			direction = vec3(d.x, 1, d.y);
		break;
		case 3 : // Back (NEGATIVE_Y)
			direction = vec3(d.x,-1, -d.y);
		break;
		case 4 : // Top (POSITIVE_Z)
			direction = vec3(d.x,-d.y, 1);
		break;
		case 5 : // Bottom (NEGATIVE_Z)
			direction = vec3(-d.x,-d.y, -1);
		break;
	}
	
	return normalize(direction);
}


#############################################################

#shader mantle.map.comp
void main() {
	vec3 dir = GetCubeDirection(mantleMap[planetIndex]);
	imageStore(mantleMap[planetIndex], ivec3(gl_GlobalInvocationID), vec4(dir,0));
}

#shader tectonics.map.comp
void main() {
	imageStore(tectonicsMap[planetIndex], ivec3(gl_GlobalInvocationID), vec4(0));
}

#shader height.map.comp
void main() {
	vec3 dir = GetCubeDirection(heightMap[planetIndex]);
	float height = FastSimplexFractal(dir*20, 6);
	imageStore(heightMap[planetIndex], ivec3(gl_GlobalInvocationID), vec4(height*planetHeightVariation));
}

#shader volcanoes.map.comp
void main() {
	imageStore(volcanoesMap[planetIndex], ivec3(gl_GlobalInvocationID), vec4(0));
}

#shader liquids.map.comp
void main() {
	imageStore(liquidsMap[planetIndex], ivec3(gl_GlobalInvocationID), vec4(0));
}

#############################################################
#shader raymarching.rint

#include "rtx_base.glsl"
hitAttributeNV ProceduralGeometry geom;

layout(set = 2, binding = 2) uniform samplerCube heightMap[MAX_PLANETS];

double dlength(dvec3 v) {
	return sqrt(v.x*v.x + v.y*v.y + v.z*v.z);
}

double dlengthSquared(dvec3 v) {
	return (v.x*v.x + v.y*v.y + v.z*v.z);
}

void main() {
	geom = GetProceduralGeometry(gl_InstanceCustomIndexNV);
	vec3 spherePosition = geom.objectInstance.position;
	float sphereRadius = geom.aabbMax.x;
	uint planetIndex = geom.material;
	
	const vec3 direction = gl_WorldRayDirectionNV;
	const float tMin = gl_RayTminNV;
	const float tMax = gl_RayTmaxNV;

	const vec3 oc = -spherePosition;
	const float a = dot(direction, direction);
	const float b = dot(oc, direction);
	const float c = dot(oc, oc) - sphereRadius*sphereRadius;
	const float discriminant = b * b - a * c;

	if (discriminant >= 0) {
		const float discriminantSqrt = sqrt(discriminant);
		const float t1 = (-b - discriminantSqrt) / a;
		const float t2 = (-b + discriminantSqrt) / a;
		if ((tMin <= t1 && t1 < tMax)    ||(tMin <= t2 && t2 < tMax)   ) {
			vec3 start = gl_WorldRayDirectionNV * t1;
			// if (!(tMin <= t1 && t1 < tMax)) start = vec3(0);
			double dist = 0;
			if (!(tMin <= t1 && t1 < tMax)) dist = length(start);
			double stepSize = max(0.01, t1 * 0.001);
			for (int i = 0; i < 200; ++i) {
				dist += stepSize;
				dvec3 hitPoint = dvec3(start) + (dvec3(gl_WorldRayDirectionNV) * dist);
				dvec3 planetPos = (planets[planetIndex] * dvec4(hitPoint, 1)).xyz;
				// vec3 planetPos = vec4(inverse(geom.objectInstance.modelViewMatrix) * vec4(hitPoint, 1)).xyz;
				if (length(vec3(planetPos)) > sphereRadius) return;
				double sampledTexture = double(texture(heightMap[planetIndex], vec3(planetPos)).r);
				double heightMap = double(sphereRadius) - double(planetHeightVariation) + sampledTexture;
				double alt = dlength(planetPos) - heightMap;
				if (alt < 0.01) {
					break;
				} else {
					stepSize = clamp(alt * 0.5, t1 * 0.01, planetHeightVariation/4);
				}
			}
			reportIntersectionNV(float(double(t1)+dist), 0);
		}
	}
}

#############################################################
#shader raymarching.rchit

#include "rtx_base.glsl"
hitAttributeNV ProceduralGeometry geom;
layout(location = 0) rayPayloadInNV RayPayload ray;
layout(location = 2) rayPayloadNV bool shadowed;
#include "rtx_pbr.glsl"

#include "incubator_rendering/assets/shaders/_noise.glsl"
#include "incubator_rendering/assets/shaders/_v4dnoise.glsl"

uint planetIndex = geom.material;

layout(set = 2, binding = 0) uniform samplerCube mantleMap[MAX_PLANETS];
layout(set = 2, binding = 1) uniform samplerCube tectonicsMap[MAX_PLANETS];
layout(set = 2, binding = 2) uniform samplerCube heightMap[MAX_PLANETS];
layout(set = 2, binding = 3) uniform samplerCube volcanoesMap[MAX_PLANETS];
layout(set = 2, binding = 4) uniform samplerCube liquidsMap[MAX_PLANETS];

void main() {
	// vec3 spherePosition = geom.objectInstance.position;
	// float sphereRadius = geom.aabbMax.x;
	
	// Hit World Position
	const vec3 hitPoint = gl_WorldRayDirectionNV * gl_HitTNV;
	const dvec3 planetPos = (planets[planetIndex] * dvec4(hitPoint, 1)).xyz;
	// const vec3 planetPos = vec4(inverse(geom.objectInstance.modelViewMatrix) * vec4(hitPoint, 1)).xyz;
	
	
	vec3 pos0 = vec3(planetPos);
	vec3 tangentX = normalize(cross(vec3(0,0,1), normalize(pos0)));
	vec3 tangentY = normalize(cross(normalize(pos0), tangentX));
	vec3 posR = normalize(pos0+tangentX*10);
	vec3 posL = normalize(pos0-tangentX*10);
	vec3 posU = normalize(pos0+tangentY*10);
	vec3 posD = normalize(pos0-tangentY*10);
	
	float height = texture(heightMap[planetIndex], pos0).r;
	posR *= texture(heightMap[planetIndex], posR).r;
	posL *= texture(heightMap[planetIndex], posL).r;
	posU *= texture(heightMap[planetIndex], posU).r;
	posD *= texture(heightMap[planetIndex], posD).r;
	
	vec3 line1 = posR - posL;
	vec3 line2 = posU - posD;
	
	vec3 normal = geom.objectInstance.normalMatrix * normalize(mix(normalize(pos0), normalize(cross(line1, line2)),0.2));
	
	vec3 c = vec3(height / planetHeightVariation / 2.0 + 0.5);
	
	vec3 color = ApplyPBRShading(hitPoint, c, normal, /*roughness*/0.5, /*metallic*/0.0);
	
	
	// vec4 color = vec4(texture(heightMap[planetIndex], vec3(planetPos)).r / planetHeightVariation / 2.0 + 0.5);
	// vec4 color = texture(mantleMap[planetIndex], normalize(planetPos));
	
	// if (gl_HitTNV < 10000) color = mix(color, vec4(float(FastSimplexFractal(planetPos/50.0, 2)/2.0+0.5)), smoothstep(10000, 0, gl_HitTNV));
	
	// if (gl_HitTNV < 1.0) color = vec4(1,0,0,1);
	
	ray.color = color.rgb;
	ray.distance = gl_HitTNV;
}


#############################################################
#shader terrain.rchit

#include "rtx_base.glsl"
hitAttributeNV vec3 hitAttribs;
layout(location = 0) rayPayloadInNV RayPayload ray;
layout(location = 2) rayPayloadNV bool shadowed;
#include "rtx_pbr.glsl"
#include "rtx_fragment.glsl"

// #include "incubator_rendering/assets/shaders/_noise.glsl"
// #include "incubator_rendering/assets/shaders/_v4dnoise.glsl"

// layout(set = 2, binding = 0) uniform samplerCube mantleMap[MAX_PLANETS];
// layout(set = 2, binding = 1) uniform samplerCube tectonicsMap[MAX_PLANETS];
// layout(set = 2, binding = 2) uniform samplerCube heightMap[MAX_PLANETS];
// layout(set = 2, binding = 3) uniform samplerCube volcanoesMap[MAX_PLANETS];
// layout(set = 2, binding = 4) uniform samplerCube liquidsMap[MAX_PLANETS];

void main() {
	Fragment fragment = GetHitFragment(true);
	vec3 color = ApplyPBRShading(fragment.hitPoint, fragment.color.rgb, fragment.normal, /*roughness*/0.5, /*metallic*/0.0);
	ray.color = color;
	ray.distance = gl_HitTNV;
	
	// ray.color = vec3(0,1,0);
	// ray.distance = gl_HitTNV;
}

