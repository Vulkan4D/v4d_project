#version 460 core

precision highp int;
precision highp float;
precision highp sampler2D;

#include "_v4d_baseDescriptorSet.glsl"

#common .*frag

layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 out_color;

// G-Buffers
layout(set = 1, input_attachment_index = 0, binding = 0) uniform highp subpassInput gBuffer_albedo;
layout(set = 1, input_attachment_index = 1, binding = 1) uniform lowp  subpassInput gBuffer_normal;
layout(set = 1, input_attachment_index = 2, binding = 2) uniform lowp  subpassInput gBuffer_roughness;
layout(set = 1, input_attachment_index = 3, binding = 3) uniform lowp  subpassInput gBuffer_metallic;
layout(set = 1, input_attachment_index = 4, binding = 4) uniform lowp  subpassInput gBuffer_scatter;
layout(set = 1, input_attachment_index = 5, binding = 5) uniform lowp  subpassInput gBuffer_occlusion;
layout(set = 1, input_attachment_index = 6, binding = 6) uniform highp subpassInput gBuffer_emission;
layout(set = 1, input_attachment_index = 7, binding = 7) uniform highp subpassInput gBuffer_position;

struct GBuffer {
	highp vec3 albedo;
	lowp  vec3 normal;
	lowp float roughness;
	lowp float metallic;
	lowp float scatter;
	lowp float occlusion;
	highp vec3 emission;
	highp vec3 position;
};

GBuffer LoadGBuffer() {
	return GBuffer(
		subpassLoad(gBuffer_albedo).rgb,
		subpassLoad(gBuffer_normal).xyz,
		subpassLoad(gBuffer_roughness).s,
		subpassLoad(gBuffer_metallic).s,
		subpassLoad(gBuffer_scatter).s,
		subpassLoad(gBuffer_occlusion).s,
		subpassLoad(gBuffer_emission).rgb,
		subpassLoad(gBuffer_position).xyz
	);
}

##################################################################
#shader vert

layout (location = 0) out vec2 outUV;

void main() 
{
	outUV = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
	gl_Position = vec4(outUV * 2.0f + -1.0f, 0.0f, 1.0f);
}

##################################################################

#shader opaque.frag
void main() {
	GBuffer gBuffer = LoadGBuffer();
	vec3 color = gBuffer.albedo;
	out_color = vec4(color, 1.0);
}

#shader transparent.frag
void main() {
	GBuffer gBuffer = LoadGBuffer();
	vec3 color = gBuffer.albedo;
	out_color = vec4(color, 1.0);
}
