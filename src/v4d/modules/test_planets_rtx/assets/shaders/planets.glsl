#version 460 core
#define MAX_PLANETS 1

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
	const vec2 d = uv * 2.0 - 1.0;
	vec3 direction;
	switch (int(gl_GlobalInvocationID.z)) {
		case 0 : // Right
			direction = vec3( 1, -d.y, -d.x);
		break;
		case 1 : // Left
			direction = vec3(-1, -d.y, d.x);
		break;
		case 2 : // Front
			direction = vec3(d.x, 1, d.y);
		break;
		case 3 : // Back
			direction = vec3(d.x,-1, -d.y);
		break;
		case 4 : // Top
			direction = vec3(d.x,-d.y, 1);
		break;
		case 5 : // Bottom
			direction = vec3(-d.x,-d.y, -1);
		break;
	}
	return direction;
}


#############################################################

#shader mantle.map.comp
void main() {
	imageStore(mantleMap[planetIndex], ivec3(gl_GlobalInvocationID), vec4(0));
}

#shader tectonics.map.comp
void main() {
	imageStore(tectonicsMap[planetIndex], ivec3(gl_GlobalInvocationID), vec4(0));
}

#shader height.map.comp
void main() {
	vec3 dir = GetCubeDirection(heightMap[planetIndex]);
	
	float height = FastSimplexFractal(dir*10, 5) / 2 + 0.5;
	
	imageStore(heightMap[planetIndex], ivec3(gl_GlobalInvocationID), vec4(height));
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
#shader rint

#include "rtx_base.glsl"
hitAttributeNV ProceduralGeometry sphereGeomAttr;

void main() {
	ProceduralGeometry geom = GetProceduralGeometry(gl_InstanceCustomIndexNV);
	vec3 spherePosition = geom.objectInstance.position;
	float sphereRadius = geom.aabbMax.x;
	
	const vec3 origin = gl_WorldRayOriginNV;
	const vec3 direction = gl_WorldRayDirectionNV;
	const float tMin = gl_RayTminNV;
	const float tMax = gl_RayTmaxNV;

	const vec3 oc = origin - spherePosition;
	const float a = dot(direction, direction);
	const float b = dot(oc, direction);
	const float c = dot(oc, oc) - sphereRadius*sphereRadius;
	const float discriminant = b * b - a * c;

	if (discriminant >= 0) {
		const float discriminantSqrt = sqrt(discriminant);
		const float t1 = (-b - discriminantSqrt) / a;
		const float t2 = (-b + discriminantSqrt) / a;

		if ((tMin <= t1 && t1 < tMax) || (tMin <= t2 && t2 < tMax)) {
			sphereGeomAttr = geom;
			reportIntersectionNV((tMin <= t1 && t1 < tMax) ? t1 : t2, 0);
		}
	}
}

#############################################################
#shader rchit

#include "rtx_base.glsl"
hitAttributeNV ProceduralGeometry sphereGeomAttr;
layout(location = 0) rayPayloadInNV RayPayload ray;
layout(location = 2) rayPayloadNV bool shadowed;
#include "rtx_pbr.glsl"

layout(set = 2, binding = 0) uniform samplerCube mantleMap[MAX_PLANETS];
layout(set = 2, binding = 1) uniform samplerCube tectonicsMap[MAX_PLANETS];
layout(set = 2, binding = 2) uniform samplerCube heightMap[MAX_PLANETS];
layout(set = 2, binding = 3) uniform samplerCube volcanoesMap[MAX_PLANETS];
layout(set = 2, binding = 4) uniform samplerCube liquidsMap[MAX_PLANETS];

void main() {
	vec3 spherePosition = sphereGeomAttr.objectInstance.position;
	float sphereRadius = sphereGeomAttr.aabbMax.x;
	
	// Hit World Position
	const vec3 hitPoint = gl_WorldRayOriginNV + gl_WorldRayDirectionNV * gl_HitTNV;
	const vec3 normal = normalize(hitPoint - spherePosition);
	
	// vec3 color = ApplyPBRShading(hitPoint, sphereGeomAttr.color.rgb, normal, /*roughness*/0.5, /*metallic*/0.0);
	
	vec4 color = vec4(texture(heightMap[sphereGeomAttr.material], vec3(normalize(hitPoint - spherePosition))).r);
	
	ray.color = color.rgb;
	ray.distance = gl_HitTNV;
}

