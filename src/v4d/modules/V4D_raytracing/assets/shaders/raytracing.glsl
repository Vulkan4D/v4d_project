#define RAY_TRACING
#include "v4d/modules/V4D_raytracing/glsl_includes/set0.glsl"


#############################################################
#shader rgen

layout(set = 1, binding = 0, rgba16f) uniform image2D img_lit;
layout(set = 1, binding = 1, r32f) uniform image2D img_depth;
layout(set = 1, binding = 2) buffer writeonly RayCast {
	uint64_t moduleVen;
	uint64_t moduleId;
	uint64_t objId;
	uint64_t raycastCustomData;
	vec4 localSpaceHitPositionAndDistance; // w component is distance
	vec4 localSpaceHitSurfaceNormal; // w component is unused
} rayCast;

layout(location = 0) rayPayloadEXT RayTracingPayload ray;
layout(location = 1) rayPayloadEXT bool shadowed;

const int MAX_BOUNCES = 10; // 0 is infinite

#include "v4d/modules/V4D_raytracing/glsl_includes/core_pbr.glsl"

void main() {
	const ivec2 imgCoords = ivec2(gl_LaunchIDEXT.xy);
	const ivec2 pixelInMiddleOfScreen = ivec2(gl_LaunchSizeEXT.xy) / 2;
	const bool isMiddleOfScreen = (imgCoords == pixelInMiddleOfScreen);
	const vec2 pixelCenter = vec2(gl_LaunchIDEXT.xy) + vec2(0.5);
	const vec2 d = pixelCenter/vec2(gl_LaunchSizeEXT.xy) * 2.0 - 1.0;
	const vec3 origin = vec3(0);
	const vec3 direction = normalize(vec4(inverse(isMiddleOfScreen? camera.rawProjectionMatrix : camera.projectionMatrix) * dvec4(d.x, d.y, 1, 1)).xyz);
	vec3 litColor = vec3(0);
	int bounces = 0;

	// Trace Primary Ray
	traceRayEXT(topLevelAS, gl_RayFlagsOpaqueEXT, RAY_TRACE_MASK_PRIMARY, 0, 0, 0, origin, float(camera.znear), direction, float(camera.zfar), 0);
	
	// Store raycast info
	if (isMiddleOfScreen) {
		if (ray.distance > 0) {
			// write obj info in hit raycast
			RenderableEntityInstance entity = GetRenderableEntityInstance(ray.instanceCustomIndex);
			rayCast.moduleVen = entity.moduleVen;
			rayCast.moduleId = entity.moduleId;
			rayCast.objId = entity.objId;
			rayCast.raycastCustomData = ray.raycastCustomData;
			rayCast.localSpaceHitPositionAndDistance = vec4((inverse(GetModelViewMatrix(ray.instanceCustomIndex)) * vec4(ray.position, 1)).xyz, ray.distance);
			rayCast.localSpaceHitSurfaceNormal = vec4(normalize(inverse(GetModelNormalViewMatrix(ray.instanceCustomIndex)) * ray.normal), 0);
		} else {
			// write empty hit raycast
			rayCast.moduleVen = 0;
			rayCast.moduleId = 0;
			rayCast.objId = 0;
			rayCast.raycastCustomData = 0;
			rayCast.localSpaceHitPositionAndDistance = vec4(0);
			rayCast.localSpaceHitSurfaceNormal = vec4(0);
		}
	}
	
	// Debug / Render Modes = NOTHING
	if (camera.renderMode == RENDER_MODE_NOTHING) {
		imageStore(img_lit, imgCoords, vec4(0));
		imageStore(img_depth, imgCoords, vec4(0));
		return;
	}
	
	// Store depth and distance
	float primaryRayDistance = ray.distance;
	float depth = ray.distance==0?0:clamp(GetFragDepthFromViewSpacePosition(ray.position), 0, 1);
	imageStore(img_depth, imgCoords, vec4(depth, primaryRayDistance, 0,0));
	
	// Store background when primary ray missed
	if (primaryRayDistance == 0) {
		imageStore(img_lit, imgCoords, vec4(0)); //TODO sample Galaxy Background
		return;
	}

	// Other Render Modes
	switch (camera.renderMode) {
		case RENDER_MODE_STANDARD:
			
			// Refraction
			if (ray.refractionIndex >= 1.0) {
				vec3 glassColor = ray.albedo;
				float glassRoughness = ray.roughness;
				vec3 glassNormal = ray.normal;
				vec3 glassRefractionDirection = refract(direction, glassNormal, ray.refractionIndex);
				traceRayEXT(topLevelAS, gl_RayFlagsOpaqueEXT, RAY_TRACE_MASK_PRIMARY, 0, 0, 0, ray.position, float(camera.znear), glassRefractionDirection, float(camera.zfar), 0);
				litColor = mix(ApplyPBRShading(origin, ray.position, ray.albedo, ray.normal, /*bump*/vec3(0), ray.roughness, ray.metallic) + ray.emission, glassColor, glassRoughness);
			} else {
				litColor = ApplyPBRShading(origin, ray.position, ray.albedo, ray.normal, /*bump*/vec3(0), ray.roughness, ray.metallic) + ray.emission;
			}
			
			// Reflections
			float reflectivity = min(0.9, ray.metallic);
			vec3 reflectionOrigin = ray.position + ray.normal * GetOptimalBounceStartDistance(primaryRayDistance);
			vec3 viewDirection = normalize(ray.position);
			vec3 surfaceNormal = normalize(ray.normal);
			while (Reflections && reflectivity > 0.01) {
				vec3 reflectDirection = reflect(viewDirection, surfaceNormal);
				traceRayEXT(topLevelAS, gl_RayFlagsOpaqueEXT, RAY_TRACE_MASK_REFLECTION, 0, 0, 0, reflectionOrigin, GetOptimalBounceStartDistance(primaryRayDistance), reflectDirection, float(camera.zfar), 0);
				if (ray.distance > 0) {
					vec3 reflectColor = ApplyPBRShading(reflectionOrigin, ray.position, ray.albedo, ray.normal, /*bump*/vec3(0), ray.roughness, ray.metallic) + ray.emission;
					litColor = mix(litColor, reflectColor*litColor, reflectivity);
					reflectivity *= min(0.9, ray.metallic);
					if (reflectivity > 0) {
						reflectionOrigin = ray.position + ray.normal * GetOptimalBounceStartDistance(primaryRayDistance);
						viewDirection = normalize(ray.position);
						surfaceNormal = normalize(ray.normal);
					}
				} else {
					litColor = mix(litColor, litColor*vec3(0.5)/* fake atmosphere color (temporary) */, reflectivity);
					reflectivity = 0;
				}
				if (MAX_BOUNCES > 0 && ++bounces > MAX_BOUNCES) break;
			}
			
			// Store final color
			imageStore(img_lit, imgCoords, vec4(litColor,1));
			
			break;
		case RENDER_MODE_NORMALS:
			imageStore(img_lit, imgCoords, vec4(ray.normal*camera.renderDebugScaling,1));
			break;
		case RENDER_MODE_ALBEDO:
			imageStore(img_lit, imgCoords, vec4(ray.albedo*camera.renderDebugScaling,1));
			break;
		case RENDER_MODE_EMISSION:
			imageStore(img_lit, imgCoords, vec4(ray.emission*camera.renderDebugScaling,1));
			break;
		case RENDER_MODE_DEPTH:
			imageStore(img_lit, imgCoords, vec4(vec3(depth*pow(10, camera.renderDebugScaling*6)),1));
			break;
		case RENDER_MODE_DISTANCE:
			imageStore(img_lit, imgCoords, vec4(vec3(ray.distance/pow(10, camera.renderDebugScaling*4)),1));
			break;
		case RENDER_MODE_METALLIC:
			imageStore(img_lit, imgCoords, vec4(vec3(ray.metallic*camera.renderDebugScaling),1));
			break;
		case RENDER_MODE_ROUGNESS:
			imageStore(img_lit, imgCoords, vec4(ray.roughness >=0 ? vec3(ray.roughness*camera.renderDebugScaling) : vec3(-ray.roughness*camera.renderDebugScaling,0,0),1));
			break;
		case RENDER_MODE_REFRACTION:
			imageStore(img_lit, imgCoords, vec4(vec3(ray.refractionIndex*camera.renderDebugScaling/2),1));
			break;
	}
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

