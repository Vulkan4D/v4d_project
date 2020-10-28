#include "core.glsl"

#common rtx.*
#include "v4d/modules/V4D_hybrid/glsl_includes/rtx.glsl"

#common .*frag
#include "v4d/modules/V4D_hybrid/glsl_includes/pl_lighting_raster.glsl"
#include "v4d/modules/V4D_hybrid/glsl_includes/raster_pbr.glsl"
void StandardPbrFragmentLightingShader() {
	float depth = GetDepth();
	if (depth == 0) {
		// There is nothing here
		WriteLitImage(vec4(0));
		discard;
	}
	
	ReadPbrGBuffers();
	
	if (DebugWireframe) {
		WriteLitImage(vec4(pbrGBuffers.viewSpaceNormal, 1));
		return;
	}
	
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


#################################################################################
#shader vert

#include "v4d/modules/V4D_hybrid/glsl_includes/pl_lighting_raster.glsl"

void main() {
	vec2 uv = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
	gl_Position = vec4(uv * 2.0f + -1.0f, 0.0f, 1.0f);
}

#################################################################################

#shader frag
void main() {
	StandardPbrFragmentLightingShader();
}

#shader rtx.frag
void main() {
	StandardPbrFragmentLightingShader();
}

