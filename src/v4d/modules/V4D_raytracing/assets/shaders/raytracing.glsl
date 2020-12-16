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

#include "v4d/modules/V4D_raytracing/glsl_includes/core_pbr.glsl"

vec4 SampleBackground(vec3 direction) {
	return vec4(0.5);
}


float linstep(float low, float high, float value){
	return clamp((value-low)/(high-low), 0.0, 1.0);
}
float fade(float low, float high, float value){
	float mid = (low+high)*0.5;
	float range = (high-low)*0.5;
	float x = 1.0 - clamp(abs(mid-value)/range, 0.0, 1.0);
	return smoothstep(0.0, 1.0, x);
}
vec3 getHeatMap(float intensity){
	vec3 blue = vec3(0.0, 0.0, 1.0);
	vec3 cyan = vec3(0.0, 1.0, 1.0);
	vec3 green = vec3(0.0, 1.0, 0.0);
	vec3 yellow = vec3(1.0, 1.0, 0.0);
	vec3 red = vec3(1.0, 0.0, 0.0);
	vec3 color = (
		fade(-0.25, 0.25, intensity)*blue +
		fade(0.0, 0.5, intensity)*cyan +
		fade(0.25, 0.75, intensity)*green +
		fade(0.5, 1.0, intensity)*yellow +
		smoothstep(0.75, 1.0, intensity)*red
	);
	return color;
}

void main() {
	const ivec2 imgCoords = ivec2(gl_LaunchIDEXT.xy);
	const ivec2 pixelInMiddleOfScreen = ivec2(gl_LaunchSizeEXT.xy) / 2;
	const bool isMiddleOfScreen = (imgCoords == pixelInMiddleOfScreen);
	const vec2 pixelCenter = vec2(gl_LaunchIDEXT.xy) + vec2(0.5);
	const vec2 d = pixelCenter/vec2(gl_LaunchSizeEXT.xy) * 2.0 - 1.0;
	
	vec3 litColor = vec3(0);
	float opacity = 0;
	float reflectivity = 1.0;
	int bounces = 0;
	
	vec3 rayOrigin = vec3(0);
	vec3 rayDirection = normalize(vec4(inverse(isMiddleOfScreen? camera.rawProjectionMatrix : camera.projectionMatrix) * dvec4(d.x, d.y, 1, 1)).xyz);
	uint rayMask = RAY_TRACE_MASK_PRIMARY;
	float rayMinDistance = float(camera.znear);
	float rayMaxDistance = float(camera.zfar);

	// Trace Primary Ray
	traceRayEXT(topLevelAS, 0, rayMask, 0, 0, 0, rayOrigin, rayMinDistance, rayDirection, rayMaxDistance, 0);
	
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
	
	bool preferRefraction = true;//int(camera.time*100) % 2 > 0;
	
	// Other Render Modes
	switch (camera.renderMode) {
		case RENDER_MODE_STANDARD:
			// Store background when primary ray missed
			if (primaryRayDistance == 0) {
				litColor = SampleBackground(rayDirection).rgb;
				break;
			}
			// Fallthrough
		case RENDER_MODE_BOUNCES:
			if (primaryRayDistance == 0) break;
		
			litColor = ApplyPBRShading(rayOrigin, ray.position, ray.albedo, ray.normal, /*bump*/vec3(0), ray.roughness, ray.metallic) + ray.emission;
			
			while (bounces++ < camera.maxBounces || camera.maxBounces == -1) { // camera.maxBounces(-1) = infinite bounces
			
				reflectivity *= min(0.95, ray.metallic); // this prevents infinite loop
				opacity += min(0.01, ray.alpha); // this prevents infinite loop
				
				// Prepare next ray for either reflection or refraction
				bool refraction = Refraction && ray.refractionIndex >= 1.0 && opacity < 0.99;
				bool reflection = Reflections && reflectivity > 0.01;
				if (refraction && reflection) {// Pick either randomly, not both
					if (preferRefraction) {
						refraction = true;
						reflection = false;
					} else {
						refraction = false;
						reflection = true;
					}
				}
				if (reflection) {
					rayOrigin = ray.position + normalize(ray.normal) * ray.nextRayStartOffset;
					rayDirection = reflect(normalize(ray.position), normalize(ray.normal));
					rayMinDistance = GetOptimalBounceStartDistance(primaryRayDistance);
					rayMaxDistance = float(camera.zfar);
					rayMask = RAY_TRACE_MASK_REFLECTION;
				} else if (refraction) {
					rayOrigin = ray.position + normalize(ray.normal) * ray.nextRayStartOffset;
					rayDirection = refract(rayDirection, ray.normal, ray.refractionIndex);
					rayMinDistance = float(camera.znear);
					rayMaxDistance = float(camera.zfar);
					rayMask = 0xff;
				} else break;
				
				ray.refractionIndex = 0;
				ray.metallic = 0;
				ray.alpha = 1;
				ray.nextRayStartOffset = 0;
				traceRayEXT(topLevelAS, 0, rayMask, 0, 0, 0, rayOrigin, rayMinDistance, rayDirection, rayMaxDistance, 0);
				vec3 color;
				if (ray.distance == 0) {
					color = SampleBackground(rayDirection).rgb;
				} else {
					color = ApplyPBRShading(rayOrigin, ray.position, ray.albedo, ray.normal, /*bump*/vec3(0), ray.roughness, ray.metallic) + ray.emission;
				}
				
				if (reflection) {
					litColor = mix(litColor, color*litColor, reflectivity);
				} else if (refraction) {
					litColor = mix(color, litColor, opacity);
				}
				
				if (ray.distance == 0) break;
			}
			break;
		case RENDER_MODE_NORMALS:
			litColor = vec3(ray.normal*camera.renderDebugScaling);
			if (primaryRayDistance == 0) litColor = vec3(0);
			opacity = 1;
			break;
		case RENDER_MODE_ALBEDO:
			litColor = vec3(ray.albedo*camera.renderDebugScaling);
			if (primaryRayDistance == 0) litColor = vec3(0);
			opacity = 1;
			break;
		case RENDER_MODE_EMISSION:
			litColor = vec3(ray.emission*camera.renderDebugScaling);
			if (primaryRayDistance == 0) litColor = vec3(0);
			opacity = 1;
			break;
		case RENDER_MODE_DEPTH:
			litColor = vec3(depth*pow(10, camera.renderDebugScaling*6));
			if (primaryRayDistance == 0) litColor = vec3(1,0,1);
			opacity = 1;
			break;
		case RENDER_MODE_DISTANCE:
			litColor = vec3(ray.distance/pow(10, camera.renderDebugScaling*4));
			if (primaryRayDistance == 0) litColor = vec3(1,0,1);
			opacity = 1;
			break;
		case RENDER_MODE_METALLIC:
			litColor = vec3(ray.metallic*camera.renderDebugScaling);
			if (primaryRayDistance == 0) litColor = vec3(1,0,1);
			opacity = 1;
			break;
		case RENDER_MODE_ROUGNESS:
			litColor = ray.roughness >=0 ? vec3(ray.roughness*camera.renderDebugScaling) : vec3(-ray.roughness*camera.renderDebugScaling,0,0);
			if (primaryRayDistance == 0) litColor = vec3(1,0,1);
			opacity = 1;
			break;
		case RENDER_MODE_REFRACTION:
			litColor = vec3(ray.refractionIndex*camera.renderDebugScaling/2);
			if (primaryRayDistance == 0) litColor = vec3(1,0,1);
			opacity = 1;
			break;
	}
	if (camera.renderMode == RENDER_MODE_BOUNCES) {
		litColor = bounces==0? vec3(0) : getHeatMap(float(bounces-1)/(camera.renderDebugScaling*5));
		opacity = 1;
	}
	
	// Store final color
	imageStore(img_lit, imgCoords, vec4(litColor, opacity));
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

