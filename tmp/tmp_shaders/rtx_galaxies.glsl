#version 460
#extension GL_NV_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : enable

precision highp int;
precision highp float;
precision highp sampler2D;

#include "_noise.glsl"
#include "_v4dnoise.glsl"

// Ray Tracing Payload
struct RayPayload {
	vec3 color;
	float density;
	vec3 endPoint;
};

struct GalaxyAttrib {
	vec4 posr;
};

layout(std430, push_constant) uniform GalaxyGenPushConstant {
	dvec3 cameraPosition;
	int frameIndex;
	int resolution;
} galaxyGen;

const float MIN_VIEW_DISTANCE = 0.0;
const float MAX_VIEW_DISTANCE = 2000.0;

// Layout Bindings
layout(set = 0, binding = 0) uniform accelerationStructureNV topLevelAS;
layout(set = 0, binding = 1, rgba32f) uniform /*readonly|writeonly*/ imageCube galaxyCubeMap;
layout(set = 0, binding = 2) readonly buffer Galaxies { vec4 galaxies[]; };

#############################################################

#shader rgen

layout(location = 0) rayPayloadNV RayPayload ray;

void main() {
	const vec2 pixelCenter = vec2(gl_LaunchIDNV.xy) + vec2(0.5);
	const vec2 inUV = pixelCenter/vec2(gl_LaunchSizeNV.xy);
	const vec2 d = inUV * 2.0 - 1.0;
	vec3 direction;
	
	switch (int(gl_LaunchIDNV.z)) {
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
	
	vec3 origin = vec3(galaxyGen.cameraPosition);
	vec3 color = vec3(0);
	float density = 0;
	
	if (density >= 1.0) return;
	
	for (int i = 0; i < 10; i++) {
		traceNV(topLevelAS, 0, 0xff, 0, 0, 0, origin, MIN_VIEW_DISTANCE, normalize(direction), MAX_VIEW_DISTANCE, 0);
		color += ray.color;
		if (ray.density > 0) density += ray.density;
		if (ray.density >= 1 || ray.density < 0) break;
		origin = ray.endPoint;
	}
	
	imageStore(galaxyCubeMap, ivec3(gl_LaunchIDNV.xyz), vec4(color / 2, 1));
}


#############################################################

#shader rint

hitAttributeNV GalaxyAttrib attribs;

void main() {
	const vec4 posr = galaxies[gl_PrimitiveID * 3 + 2];
	const vec3 oc = gl_WorldRayOriginNV - posr.xyz;
	
	const float a = dot(gl_WorldRayDirectionNV, gl_WorldRayDirectionNV);
	const float b = dot(oc, gl_WorldRayDirectionNV);
	const float c = dot(oc, oc) - posr.w * posr.w;
	const float discriminant = b * b - a * c;

	if (discriminant >= 0) {
		const float discriminantSqrt = sqrt(discriminant);
		const float t1 = (-b - discriminantSqrt) / a;
		const float t2 = (-b + discriminantSqrt) / a;

		if ((gl_RayTminNV <= t1 && t1 < gl_RayTmaxNV) || (gl_RayTminNV <= t2 && t2 < gl_RayTmaxNV)) {
			attribs.posr = posr;
			reportIntersectionNV((gl_RayTminNV <= t1 && t1 < gl_RayTmaxNV) ? t1 : t2, 0);
		}
	}
}


#############################################################

#shader rchit

hitAttributeNV GalaxyAttrib attribs;

layout(location = 0) rayPayloadInNV RayPayload ray;

float linearstep(float a, float b, float x) {
	return (x - a) / (b - a);
}

void main() {
	const float distCenter = distance(vec3(galaxyGen.cameraPosition), attribs.posr.xyz);
	const float sizeInScreen = attribs.posr.w / distCenter * float(galaxyGen.resolution);
	// const float maxSteps = max(1, min(100, sizeInScreen / 10.0));
	const float maxSteps = 500;
	
	vec3 hitPoint = gl_WorldRayOriginNV + gl_WorldRayDirectionNV * gl_HitTNV;
	
	if (distCenter < gl_HitTNV) {
		// We are in the sphere
		hitPoint = vec3(galaxyGen.cameraPosition);
	}
	
	vec3 endPoint = hitPoint + gl_WorldRayDirectionNV * attribs.posr.w * 2.0;
	
	const float brightnessBasedOnDistance = linearstep(MAX_VIEW_DISTANCE, MIN_VIEW_DISTANCE, distCenter);
	if (brightnessBasedOnDistance < 0.01) return;
	
	GalaxyInfo info = GetGalaxyInfo(attribs.posr.xyz);
	
	ray.color = vec3(0);
	ray.density = 0;
	ray.endPoint = endPoint;
	
	vec3 start = (hitPoint - attribs.posr.xyz) / attribs.posr.w;
	vec3 end = (endPoint - attribs.posr.xyz) / attribs.posr.w;
	
	for (int i = 1; i <= maxSteps; i++) {
		vec3 starPos = mix(start, end, float(i)/(maxSteps+1.0));
		float starDensity = GalaxyStarDensity(starPos, info, int(max(1.0, min(3.0, sizeInScreen/100.0))));
		if (starDensity == 0.0) continue;
		vec3 color = GalaxyStarColor(starPos, info);
		ray.color = max(ray.color, color * starDensity);
		// ray.density = max(ray.density, abs(brightnessBasedOnDistance * starDensity));
		ray.density += abs(starDensity) / maxSteps * 10.0;
		if (ray.density >= 1.0) break;
	}
	
	// ray.density = 1;
	// ray.color = vec3(0.1);
}


#############################################################

#shader rmiss

layout(location = 0) rayPayloadInNV RayPayload ray;

void main() {
	ray.color = vec3(0,0,0);
	ray.density = -1;
}
