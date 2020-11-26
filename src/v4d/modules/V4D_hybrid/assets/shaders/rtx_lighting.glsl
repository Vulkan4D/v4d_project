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
	if (RayTracedReflections && pbrGBuffers.metallic > 0.001) {
		const vec3 origin = pbrGBuffers.viewSpacePosition + pbrGBuffers.viewSpaceNormal * 0.01;
		const vec3 direction = normalize(pbrGBuffers.viewSpacePosition);
		const vec3 reflectDirection = reflect(direction, normalize(pbrGBuffers.viewSpaceNormal));
		traceRayEXT(topLevelAS, gl_RayFlagsOpaqueEXT, GEOMETRY_ATTR_REFLECTION_VISIBLE, 0, 0, 0, origin, float(camera.znear), reflectDirection, float(camera.zfar), 0);
		if (reflectionRay.distance > 0) {
			vec3 reflectColor = ApplyPBRShading(reflectionRay.viewSpacePosition, reflectionRay.albedo, reflectionRay.viewSpaceNormal, /*bump*/vec3(0), reflectionRay.roughness, reflectionRay.metallic);
			litColor = mix(litColor, reflectColor, pbrGBuffers.metallic);
		}
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

