#include "set0_base.glsl"
#include "set1_lighting_rays.glsl"

#ifdef SHADER_RGEN

	struct GBuffersPbr {
		vec3 viewSpacePosition;
		vec3 viewSpaceNormal;
		vec3 albedo;
		float uv;
		float emit;
		float metallic;
		float roughness;
		float distance;
	} pbrGBuffers;

	void ReadPbrGBuffers() {
		const ivec2 coords = ivec2(gl_LaunchIDEXT.xy);
		
		vec2 pbr = imageLoad(img_gBuffer_0, coords).rg;
		vec4 normal_uv = imageLoad(img_gBuffer_1, coords);
		vec4 position_dist = imageLoad(img_gBuffer_2, coords);
		vec4 albedo_emit = imageLoad(img_gBuffer_3, coords);
		
		pbrGBuffers.viewSpacePosition = position_dist.xyz;
		pbrGBuffers.viewSpaceNormal = normal_uv.xyz;
		pbrGBuffers.uv = normal_uv.w;
		pbrGBuffers.albedo = albedo_emit.rgb;
		pbrGBuffers.emit = albedo_emit.a;
		pbrGBuffers.metallic = pbr.r;
		pbrGBuffers.roughness = pbr.g;
		pbrGBuffers.distance = position_dist.w;
	}

	float GetDepth() {
		float depth;
		if (RayTracedVisibility) {
			depth = imageLoad(img_depth, ivec2(gl_LaunchIDEXT.xy)).r;
		} else {
			depth = texture(tex_img_rasterDepth, (vec2(gl_LaunchIDEXT.xy)+vec2(0.5))/vec2(gl_LaunchSizeEXT.xy)).r;
		}
		return depth;
	}

	void WriteLitImage(vec4 color) {
		imageStore(img_lit, ivec2(gl_LaunchIDEXT.xy), color);
	}

#endif
