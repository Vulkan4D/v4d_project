#define GEOMETRY_BUFFERS_ACCESS readonly
#include "set0_base.glsl"

#ifdef SHADER_FRAG
	#include "set1_lighting_raster.glsl"
	layout(location = 0) out vec4 out_img_lit;

	struct GBuffersPbr {
		vec3 viewSpacePosition;
		vec3 viewSpaceNormal;
		vec2 uv;
		vec3 albedo;
		float emit;
		float metallic;
		float roughness;
		float realDistanceFromCamera;
	} pbrGBuffers;

	void ReadPbrGBuffers() {
		vec2 pbr = subpassLoad(in_img_gBuffer_0).rg;
		vec4 normal_uv = subpassLoad(in_img_gBuffer_1);
		vec4 position_dist = subpassLoad(in_img_gBuffer_2);
		vec4 albedo_emit = subpassLoad(in_img_gBuffer_3);
		
		pbrGBuffers.viewSpacePosition = position_dist.xyz;
		pbrGBuffers.viewSpaceNormal = normal_uv.xyz;
		pbrGBuffers.uv = UnpackUVfromFloat(normal_uv.w);
		pbrGBuffers.albedo = albedo_emit.rgb;
		pbrGBuffers.emit = albedo_emit.a;
		pbrGBuffers.metallic = pbr.r;
		pbrGBuffers.roughness = pbr.g;
		pbrGBuffers.realDistanceFromCamera = position_dist.w;
	}

	void WriteLitImage(vec4 color) {
		out_img_lit = color;
	}

#endif
