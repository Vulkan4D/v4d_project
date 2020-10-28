#include "core.glsl"
#include "v4d/modules/V4D_hybrid/glsl_includes/rtx.glsl"
#include "v4d/modules/V4D_hybrid/glsl_includes/pl_lighting_rays.glsl"

#############################################################
#shader rgen

layout(location = 0) rayPayloadEXT bool shadowed;
// layout(location = 1) rayPayloadEXT RayPayload_lighting ray;

#include "v4d/modules/V4D_hybrid/glsl_includes/rtx_pbr.glsl"

void main() {
	float depth = GetDepth();
	if (depth == 0) {
		// There is nothing here
		WriteLitImage(vec4(0));
		return;
	}
	
	ReadPbrGBuffers();
	
	vec3 litColor = vec3(0);
	
	// Emitter
	if (pbrGBuffers.emit > 0) {
		litColor += pbrGBuffers.albedo * pbrGBuffers.emit;
	}
	
	// PBR
	if (dot(pbrGBuffers.viewSpaceNormal, pbrGBuffers.viewSpaceNormal) > 0) {
		litColor += ApplyPBRShading(pbrGBuffers.viewSpacePosition, pbrGBuffers.albedo, pbrGBuffers.viewSpaceNormal, /*bump*/vec3(0), pbrGBuffers.roughness, pbrGBuffers.metallic);
	}
	
	WriteLitImage(vec4(litColor, 1));
}


#############################################################
#shader shadow.rmiss

layout(location = 0) rayPayloadInEXT bool shadowed;

void main() {
	shadowed = false;
}

