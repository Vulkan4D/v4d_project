#version 460 core

precision highp int;
precision highp float;
precision highp sampler2D;

#common .*frag

layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 out_color;

// G-Buffers
layout(set = 0, binding = 0) uniform highp sampler2D surface_albedo;
layout(set = 0, binding = 1) uniform lowp  sampler2D surface_normal;
layout(set = 0, binding = 2) uniform lowp  sampler2D surface_roughness;
layout(set = 0, binding = 3) uniform lowp  sampler2D surface_metallic;
layout(set = 0, binding = 4) uniform lowp  sampler2D surface_scatter;
layout(set = 0, binding = 5) uniform lowp  sampler2D surface_occlusion;
layout(set = 0, binding = 6) uniform highp sampler2D surface_emission;
layout(set = 0, binding = 7) uniform highp sampler2D surface_position;


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
	vec3 color = texture(surface_albedo, uv).xyz;
	out_color = vec4(color, 1.0);
}

#shader transparent.frag
void main() {
	vec3 color = texture(surface_albedo, uv).xyz;
	out_color = vec4(color, 1.0);
}
