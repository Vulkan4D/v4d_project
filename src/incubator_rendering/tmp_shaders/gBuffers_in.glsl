#include "GBuffers.glsl"

layout(set = 1, input_attachment_index = 1, binding = 0) uniform highp subpassInput gBuffer_albedo;
layout(set = 1, input_attachment_index = 2, binding = 1) uniform highp subpassInput gBuffer_normal;
layout(set = 1, input_attachment_index = 3, binding = 2) uniform highp subpassInput gBuffer_emission;
layout(set = 1, input_attachment_index = 4, binding = 3) uniform highp subpassInput gBuffer_position;

// #define kPI 3.14159265358979323846 
// vec3 decodeNormal(in vec2 enc) {
// 	float ang = enc.x*kPI;
// 	return vec3(vec2(cos(ang),sin(ang))*sqrt(1.0-enc.y*enc.y), enc.y);
// }

GBuffers LoadGBuffers() {
	vec4 albedo = subpassLoad(gBuffer_albedo);
	vec4 norm = subpassLoad(gBuffer_normal);
	vec4 emission = subpassLoad(gBuffer_emission);
	vec4 pos = subpassLoad(gBuffer_position);
	return GBuffers(
		albedo.rgb,
		norm.xyz,
		emission.rgb,
		pos.xyz,
		pos.w,
		albedo.a,
		norm.w,
		emission.w
	);
}
