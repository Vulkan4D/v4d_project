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

layout(location = RAY_PAYLOAD_LOCATION_RENDERING) rayPayloadEXT RayTracingPayload ray;
layout(location = 1) rayPayloadEXT bool shadowed;

#include "v4d/modules/V4D_raytracing/glsl_includes/core_pbr.glsl"

vec4 SampleBackground(vec3 direction) {
	return vec4(0.5,0.5,0.7,1);
}

void main() {
	const ivec2 imgCoords = ivec2(gl_LaunchIDEXT.xy);
	const ivec2 pixelInMiddleOfScreen = ivec2(gl_LaunchSizeEXT.xy) / 2;
	const bool isMiddleOfScreen = (imgCoords == pixelInMiddleOfScreen);
	const vec2 pixelCenter = vec2(gl_LaunchIDEXT.xy) + vec2(0.5);
	const vec2 d = pixelCenter/vec2(gl_LaunchSizeEXT.xy) * 2.0 - 1.0;
	
	vec3 litColor = vec3(0);
	float opacity = 0;
	int bounces = 0;
	
	vec3 rayOrigin = vec3(0);
	vec3 rayDirection = normalize(vec4(inverse(isMiddleOfScreen? camera.rawProjectionMatrix : camera.projectionMatrix) * dvec4(d.x, d.y, 1, 1)).xyz);
	uint rayMask = RAY_TRACE_MASK_VISIBLE;
	float rayMinDistance = float(camera.znear);
	float rayMaxDistance = float(camera.zfar);

	// Trace Primary Ray
	InitRayPayload(ray);
	do {
		traceRayEXT(topLevelAS, 0, rayMask, RAY_SBT_OFFSET_RENDERING, 0, 0, rayOrigin, rayMinDistance, rayDirection, rayMaxDistance, RAY_PAYLOAD_LOCATION_RENDERING);
	} while (ray.passthrough && ray.recursions++ < 100 && (rayMinDistance = ray.distance) > 0);
	
	// Store raycast info
	if (isMiddleOfScreen) {
		if (ray.distance > 0 && ray.entityInstanceIndex != -1 && ray.geometryIndex != -1) {
			// write obj info in hit raycast
			RenderableEntityInstance entity = GetRenderableEntityInstance(ray.entityInstanceIndex);
			rayCast.moduleVen = entity.moduleVen;
			rayCast.moduleId = entity.moduleId;
			rayCast.objId = entity.objId;
			rayCast.raycastCustomData = ray.raycastCustomData;
			rayCast.localSpaceHitPositionAndDistance = vec4((inverse(GetModelViewMatrix(ray.entityInstanceIndex, ray.geometryIndex)) * vec4(ray.position, 1)).xyz, ray.distance);
			rayCast.localSpaceHitSurfaceNormal = vec4(normalize(inverse(GetModelNormalViewMatrix(ray.entityInstanceIndex, ray.geometryIndex)) * ray.normal), 0);
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
		
			litColor = ApplyPBRShading(rayOrigin, ray.position, ray.albedo, ray.normal, /*bump*/vec3(0), ray.roughness, ray.metallic, ray.rim) + ray.emission;
			float attenuation = 1;
		
			vec3 rayAlbedo = vec3(1);
			
			while (bounces++ < camera.maxBounces || camera.maxBounces == -1) { // camera.maxBounces(-1) = infinite bounces
				
				// Fresnel effect
				float NdotV = max(dot(ray.normal,normalize(rayOrigin - ray.position)), 0.000001);
				float reflectionStrength = min(0.95, max(0, ray.metallic));
				vec3 baseReflectivity = rayAlbedo * mix(fresnelSchlick(NdotV, mix(vec3(0.04), ray.albedo, reflectionStrength)), vec3(reflectionStrength*ray.albedo), max(0, ray.roughness));
				float fresnelReflectionAmount = FresnelReflectAmount(1.0, ray.indexOfRefraction, ray.normal, rayDirection, reflectionStrength);
				
				rayAlbedo *= ray.albedo;
				vec3 rayNormal = ray.normal;
				
				// Prepare next ray for either reflection or refraction
				bool refraction = Refraction && ray.opacity < 1.0 && opacity < 0.99;
				bool reflection = Reflections && reflectionStrength > 0.01;
				vec3 refractionDirection = refract(rayDirection, rayNormal, ray.indexOfRefraction);
				vec3 reflectionDirection = reflect(rayDirection, rayNormal);
				if (refraction && refractionDirection == vec3(0)) {
					refraction = false;
					reflection = true;
					reflectionStrength = 0.95;
					reflectionDirection = reflect(rayDirection, -rayNormal);
				}
				opacity = min(1, opacity + max(0.01, ray.opacity));
				if (refraction) {
					rayOrigin = ray.position - rayNormal /* ray.nextRayStartOffset*/;
					rayDirection = refractionDirection;
					rayMinDistance = float(camera.znear);
					rayMaxDistance = float(camera.zfar);
					rayMask = 0xff;
				} else if (reflection) {
					rayOrigin = ray.position;
					rayDirection = reflectionDirection;
					rayMinDistance = GetOptimalBounceStartDistance(primaryRayDistance);
					rayMaxDistance = float(camera.zfar);
					rayMask = RAY_TRACE_MASK_REFLECTION;
				} else break;
				
				do {
					traceRayEXT(topLevelAS, 0, rayMask, RAY_SBT_OFFSET_RENDERING, 0, 0, rayOrigin, rayMinDistance, rayDirection, rayMaxDistance, RAY_PAYLOAD_LOCATION_RENDERING);
				} while (ray.passthrough && ray.recursions++ < 100 && (rayMinDistance = ray.distance) > 0);
				vec3 color;
				if (ray.distance == 0) {
					color = SampleBackground(rayDirection).rgb;
				} else {
					color = ApplyPBRShading(rayOrigin, ray.position, ray.albedo, ray.normal, /*bump*/vec3(0), ray.roughness, ray.metallic, ray.rim) + ray.emission;
				}
				
				if (refraction) {
					litColor = mix(color*litColor, rayAlbedo, opacity);
					attenuation *= (1-opacity);
				} else if (reflection) {
					litColor = mix(litColor, color*baseReflectivity, fresnelReflectionAmount);
					attenuation *= reflectionStrength;
				}
				
				if (attenuation < 0.01) break;
				if (ray.distance == 0) break;
			}
			
			litColor *= attenuation;
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
			litColor = vec3(ray.indexOfRefraction*camera.renderDebugScaling/2);
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
	ray.entityInstanceIndex = -1;
	ray.primitiveID = -1;
	ray.geometryIndex = -1;
	ray.raycastCustomData = 0;
	ray.passthrough = false;
}


#############################################################
#shader shadow.rmiss

layout(location = 1) rayPayloadInEXT bool shadowed;

void main() {
	shadowed = false;
}


#############################################################
#shader void.rmiss

void main() {}

