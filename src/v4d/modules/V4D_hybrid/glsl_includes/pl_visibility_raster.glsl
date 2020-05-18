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
	#ifdef SHADER_SUBPASS_0
		layout(location = 0) out vec4 out_img_tmpBuffer; // R=sfloat32(color.rgba8), G=sfloat32(uint(geometryId)), B=sfloat32(normal.xyz8+?), A=sfloat32(uv.st16)
	#endif
	#ifndef SHADER_SUBPASS_0 // or SHADER_SUBPASS_1
		layout(location = 0) out vec2 out_img_gBuffer_0; // R=snorm8(metallic), G=snorm8(roughness)
		layout(location = 1) out vec4 out_img_gBuffer_1; // RGB=sfloat32(normal.xyz32), A=sfloat32(packed(uv16))
		layout(location = 2) out vec4 out_img_gBuffer_2; // RGB=sfloat32(viewPosition.xyz), A=sfloat32(realDistanceFromCamera)
		layout(location = 3) out vec4 out_img_gBuffer_3; // RGB=snorm8(albedo.rgb), A=snorm8(emit?)
	#endif
#endif
