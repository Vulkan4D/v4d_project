#include "core.glsl"
#include "v4d/modules/V4D_hybrid/glsl_includes/rtx.glsl"
#include "v4d/modules/V4D_hybrid/glsl_includes/pl_lighting_rays.glsl"

#############################################################
#shader rgen

layout(location = 0) rayPayloadEXT RayPayload_visibility reflectionRay;
layout(location = 1) rayPayloadEXT bool shadowed;

#include "v4d/modules/V4D_hybrid/glsl_includes/rtx_pbr.glsl"

void main() {
	float depth = GetDepth();
	if (depth == 0) {
		// There is nothing here
		WriteLitImage(vec4(0));
		return;
	}
	
	ReadPbrGBuffers();
	
	if (DebugNormals) {
		WriteLitImage(vec4(pbrGBuffers.viewSpaceNormal, 1));
		return;
	}
	
	vec3 litColor = vec3(0);
	
	// PBR
	if (dot(pbrGBuffers.viewSpaceNormal, pbrGBuffers.viewSpaceNormal) > 0) {
		litColor += ApplyPBRShading(pbrGBuffers.viewSpacePosition, pbrGBuffers.albedo, pbrGBuffers.viewSpaceNormal, /*bump*/vec3(0), pbrGBuffers.roughness, pbrGBuffers.metallic);
	}
	
	// Reflection
	float reflectivity = min(0.9, pbrGBuffers.metallic);
	vec3 reflectionOrigin = pbrGBuffers.viewSpacePosition + pbrGBuffers.viewSpaceNormal * 0.01;
	vec3 viewDirection = normalize(pbrGBuffers.viewSpacePosition);
	vec3 surfaceNormal = normalize(pbrGBuffers.viewSpaceNormal);
	const int MAX_BOUNCES = 5; // 0 is infinite
	int bounces = 0;
	while (RayTracedReflections && reflectivity > 0.01) {
		vec3 reflectDirection = reflect(viewDirection, surfaceNormal);
		traceRayEXT(topLevelAS, gl_RayFlagsOpaqueEXT, GEOMETRY_ATTR_REFLECTION_VISIBLE, 0, 0, 1, reflectionOrigin, float(camera.znear), reflectDirection, float(camera.zfar), 0);
		if (reflectionRay.distance > 0) {
			vec3 reflectColor = ApplyPBRShading(reflectionRay.viewSpacePosition, reflectionRay.albedo, reflectionRay.viewSpaceNormal, /*bump*/vec3(0), reflectionRay.roughness, reflectionRay.metallic);
			litColor = mix(litColor, reflectColor*litColor, reflectivity);
			reflectivity *= min(0.9, reflectionRay.metallic);
			if (reflectivity > 0) {
				reflectionOrigin = reflectionRay.viewSpacePosition + reflectionRay.viewSpaceNormal * 0.01;
				viewDirection = normalize(reflectionRay.viewSpacePosition);
				surfaceNormal = normalize(reflectionRay.viewSpaceNormal);
			}
		} else {
			litColor = mix(litColor, litColor*vec3(0.5)/* fake atmosphere color (temporary) */, reflectivity);
			reflectivity = 0;
		}
		if (MAX_BOUNCES > 0 && ++bounces > MAX_BOUNCES) break;
	}
	
	// Emitter
	if (pbrGBuffers.emit > 0) {
		litColor += pbrGBuffers.albedo * pbrGBuffers.emit;
	}
	
	WriteLitImage(vec4(litColor, 1));
}


#############################################################
#shader shadow.rmiss

layout(location = 1) rayPayloadInEXT bool shadowed;

void main() {
	shadowed = false;
}


#############################################################
#shader reflection.rmiss

layout(location = 0) rayPayloadInEXT RayPayload_visibility ray;

void main() {
	ray.distance = 0;
}

