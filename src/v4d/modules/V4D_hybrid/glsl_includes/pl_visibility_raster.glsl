#define GEOMETRY_BUFFERS_ACCESS readonly
#include "set0_base.glsl"
#include "set1_visibility_raster.glsl"

layout(std430, push_constant) uniform GeometryPushConstant{
	uint objectIndex;
	uint geometryIndex;
};

#ifdef SHADER_VERT
	layout(location = 0) in vec3 in_pos;
	layout(location = 1) in uint _in_color;
	layout(location = 2) in vec3 in_normal;
	layout(location = 3) in uint _in_uv;

	Vertex GetVertex() {
		Vertex v;
		v.pos = in_pos.xyz;
		v.color = UnpackColorFromUint(_in_color);
		v.normal = in_normal.xyz;
		v.uv = UnpackUVfromUint(_in_uv);
		return v;
	}
	
#endif

#ifdef SHADER_FRAG
	layout(location = 0) out vec2 out_img_gBuffer_0; // R=snorm8(metallic), G=snorm8(roughness)
	layout(location = 1) out vec4 out_img_gBuffer_1; // RGB=sfloat32(normal.xyz32), A=sfloat32(packed(uv16))
	layout(location = 2) out vec4 out_img_gBuffer_2; // RGB=sfloat32(viewPosition.xyz), A=sfloat32(realDistanceFromCamera)
	layout(location = 3) out vec4 out_img_gBuffer_3; // RGB=snorm8(albedo.rgb), A=snorm8(emit?)
	
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
	
	void WritePbrGBuffers() {
		out_img_gBuffer_0 = vec2(pbrGBuffers.metallic, pbrGBuffers.roughness);
		out_img_gBuffer_1 = vec4(pbrGBuffers.viewSpaceNormal, PackUVasFloat(pbrGBuffers.uv));
		out_img_gBuffer_2 = vec4(pbrGBuffers.viewSpacePosition, pbrGBuffers.realDistanceFromCamera);
		out_img_gBuffer_3 = vec4(pbrGBuffers.albedo, pbrGBuffers.emit);
	}
	
#endif
