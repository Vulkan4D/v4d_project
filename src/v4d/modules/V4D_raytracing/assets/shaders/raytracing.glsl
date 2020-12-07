#define RAY_TRACING
#include "v4d/modules/V4D_raytracing/glsl_includes/set0.glsl"


#############################################################
#shader rgen

layout(set = 1, binding = 0, rgba16f) uniform image2D img_lit;
layout(set = 1, binding = 1, r32f) uniform image2D img_depth;

layout(location = 0) rayPayloadEXT RayTracingPayload ray;
layout(location = 1) rayPayloadEXT bool shadowed;

const int MAX_BOUNCES = 10; // 0 is infinite

#include "v4d/modules/V4D_raytracing/glsl_includes/core_pbr.glsl"

void main() {
	const ivec2 imgCoords = ivec2(gl_LaunchIDEXT.xy);
	const vec2 pixelCenter = vec2(gl_LaunchIDEXT.xy) + vec2(0.5);
	const vec2 d = pixelCenter/vec2(gl_LaunchSizeEXT.xy) * 2.0 - 1.0;
	const vec3 origin = vec3(0);
	const vec3 direction = normalize(vec4(inverse(camera.projectionMatrix) * dvec4(d.x, d.y, 1, 1)).xyz);
	vec3 litColor = vec3(0);
	int bounces = 0;

	// Trace Primary Rays
	traceRayEXT(topLevelAS, gl_RayFlagsOpaqueEXT, GEOMETRY_ATTR_PRIMARY_VISIBLE, 0, 0, 0, origin, float(camera.znear), direction, float(camera.zfar), 0);
	
	if (ray.distance > 0) {
		litColor = ApplyPBRShading(ray.position, ray.albedo, ray.normal, /*bump*/vec3(0), ray.roughness, ray.metallic) + ray.emission;
		
		// // Refraction
		// if (ray.refractionIndex >= 1.0) {
		// 	vec3 glassColor = ray.albedo;
		// 	float glassRoughness = ray.roughness;
		// 	//TODO
		// }
		
		float reflectivity = min(0.9, ray.metallic);
		vec3 reflectionOrigin = ray.position + ray.normal * 0.01;
		vec3 viewDirection = normalize(ray.position);
		vec3 surfaceNormal = normalize(ray.normal);
		while (Reflections && reflectivity > 0.01) {
			vec3 reflectDirection = reflect(viewDirection, surfaceNormal);
			traceRayEXT(topLevelAS, gl_RayFlagsOpaqueEXT, GEOMETRY_ATTR_REFLECTION_VISIBLE, 0, 0, 0, reflectionOrigin, float(camera.znear), reflectDirection, float(camera.zfar), 0);
			if (ray.distance > 0) {
				vec3 reflectColor = ApplyPBRShading(ray.position, ray.albedo, ray.normal, /*bump*/vec3(0), ray.roughness, ray.metallic) + ray.emission;
				litColor = mix(litColor, reflectColor*litColor, reflectivity);
				reflectivity *= min(0.9, ray.metallic);
				if (reflectivity > 0) {
					reflectionOrigin = ray.position + ray.normal * 0.01;
					viewDirection = normalize(ray.position);
					surfaceNormal = normalize(ray.normal);
				}
			} else {
				litColor = mix(litColor, litColor*vec3(0.5)/* fake atmosphere color (temporary) */, reflectivity);
				reflectivity = 0;
			}
			if (MAX_BOUNCES > 0 && ++bounces > MAX_BOUNCES) break;
		}
		
	} else {
		vec3(0); // background
	}
	
	float depth = clamp(GetFragDepthFromViewSpacePosition(ray.position), 0, 1);
	imageStore(img_depth, imgCoords, vec4(depth, 0,0,0));
	imageStore(img_lit, imgCoords, vec4(litColor,1));
}


#############################################################
#shader rmiss

layout(location = 0) rayPayloadInEXT RayTracingPayload ray;

void main() {
	ray.distance = 0;
}


#############################################################
#shader shadow.rmiss

layout(location = 1) rayPayloadInEXT bool shadowed;

void main() {
	shadowed = false;
}

