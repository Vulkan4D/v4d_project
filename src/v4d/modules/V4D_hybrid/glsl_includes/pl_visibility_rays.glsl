#include "set0_base.glsl"
#include "set1_visibility_rays.glsl"

Vertex GetVertex(uint index) {
	Vertex v;
	v.pos = vertices[index*2].xyz;
	v.color = UnpackColorFromFloat(vertices[index*2].w);
	v.normal = vertices[index*2+1].xyz;
	v.customData = floatBitsToUint(vertices[index*2+1].w);
	v.uv = UnpackUVfromFloat(vertices[index*2+1].w);
	return v;
}

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
	
	void WritePbrGBuffers() {
		const ivec2 coords = ivec2(gl_LaunchIDEXT.xy);
		float depth = clamp(GetFragDepthFromViewSpacePosition(pbrGBuffers.viewSpacePosition), 0, 1);
		imageStore(img_depth, coords, vec4(depth, 0,0,0));
		imageStore(img_gBuffer_0, coords, vec4(pbrGBuffers.metallic, pbrGBuffers.roughness, 0, 0));
		imageStore(img_gBuffer_1, coords, vec4(pbrGBuffers.viewSpaceNormal, pbrGBuffers.uv));
		imageStore(img_gBuffer_2, coords, vec4(pbrGBuffers.viewSpacePosition, pbrGBuffers.distance));
		imageStore(img_gBuffer_3, coords, vec4(pbrGBuffers.albedo, pbrGBuffers.emit));
	}
	
	void WriteEmptyPbrGBuffers() {
		const ivec2 coords = ivec2(gl_LaunchIDEXT.xy);
		imageStore(img_depth, coords, vec4(0));
		imageStore(img_gBuffer_0, coords, vec4(0));
		imageStore(img_gBuffer_1, coords, vec4(0));
		imageStore(img_gBuffer_2, coords, vec4(0));
		imageStore(img_gBuffer_3, coords, vec4(0));
	}
	
	void WriteCustomBuffer(uvec4 customData) {
		const ivec2 coords = ivec2(gl_LaunchIDEXT.xy);
		imageStore(img_gBuffer_4, coords, customData);
	}
	
	void WriteEmptyCustomBuffer() {
		const ivec2 coords = ivec2(gl_LaunchIDEXT.xy);
		imageStore(img_gBuffer_4, coords, uvec4(0));
	}
	
#endif
