#include "core.glsl"
#include "v4d/modules/V4D_hybrid/glsl_includes/pl_lighting_raster.glsl"

#################################################################################
#shader vert

void main() {
	vec2 uv = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
	gl_Position = vec4(uv * 2.0f + -1.0f, 0.0f, 1.0f);
}

#################################################################################
#shader frag

#include "raster_pbr.glsl"

void main() {
	vec2 pbr = subpassLoad(in_img_gBuffer_0).rg;
	float pbr_metallic = pbr.r;
	float pbr_roughness = pbr.g;
	vec4 normal_uv = subpassLoad(in_img_gBuffer_1);
	vec4 position_dist = subpassLoad(in_img_gBuffer_2);
	vec4 albedo_emit = subpassLoad(in_img_gBuffer_3);
	vec3 albedo = albedo_emit.rgb;
	float emit = albedo_emit.a;
	vec3 viewSpaceNormal = normal_uv.xyz;
	vec2 viewSpaceUV = UnpackUVfromFloat(normal_uv.w);
	
	if (camera.debug) {
		out_img_lit = vec4(viewSpaceNormal, 1);
		return;
	}
	
	out_img_lit = vec4(ApplyPBRShading(position_dist.xyz, albedo, viewSpaceNormal, vec3(0), pbr_roughness, pbr_metallic), 1);
}
