#define GEOMETRY_BUFFERS_ACCESS readonly
#include "set0_base.glsl"
#include "set1_visibility_raster.glsl"
#include "GeometryPushConstant.glsl"

#ifdef SHADER_VERT
	#ifdef PROCEDURAL_GEOMETRY
		layout(location = 0) in vec3 aabbMin;
		layout(location = 1) in vec3 aabbMax;
		layout(location = 2) in uint _in_color;
		layout(location = 3) in float custom1;
		
		vec4 GetColor() {
			return UnpackColorFromUint(_in_color);
		}
	#else
		layout(location = 0) in vec3 in_pos;
		layout(location = 1) in uint _in_color;
		layout(location = 2) in vec3 in_normal;
		layout(location = 3) in uint _in_uv;
			
		Vertex GetVertex() {
			Vertex v;
			v.pos = in_pos.xyz;
			v.color = UnpackColorFromUint(_in_color);
			v.normal = in_normal.xyz;
			v.customData = _in_uv;
			v.uv = UnpackUVfromUint(_in_uv);
			return v;
		}
	#endif

	
#endif

#ifdef SHADER_FRAG
	layout(location = 0) out vec2 out_img_gBuffer_0; // R=snorm8(metallic), G=snorm8(roughness)
	layout(location = 1) out vec4 out_img_gBuffer_1; // RGB=sfloat32(normal.xyz32), A=sfloat32(packed(uv16))
	layout(location = 2) out vec4 out_img_gBuffer_2; // RGB=sfloat32(viewPosition.xyz), A=sfloat32(distance)
	layout(location = 3) out vec4 out_img_gBuffer_3; // RGB=snorm8(albedo.rgb), A=snorm8(emit?)
	layout(location = 4) out uvec4 out_img_gBuffer_4; // R=objectIndex24+customType8, G=flags32, B=customData32, A=customData32
	
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
		out_img_gBuffer_0 = vec2(pbrGBuffers.metallic, pbrGBuffers.roughness);
		out_img_gBuffer_1 = vec4(pbrGBuffers.viewSpaceNormal, pbrGBuffers.uv);
		out_img_gBuffer_2 = vec4(pbrGBuffers.viewSpacePosition, pbrGBuffers.distance);
		out_img_gBuffer_3 = vec4(pbrGBuffers.albedo, pbrGBuffers.emit);
	}
	
	void WriteCustomBuffer(uint objectIndex, uint customType8, uint flags32, uint customData32_1, uint customData32_2) {
		out_img_gBuffer_4 = GenerateCustomData(objectIndex, customType8, flags32, customData32_1, customData32_2);
	}
	
#endif
