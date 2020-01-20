#version 460 core

precision highp int;
precision highp float;
precision highp sampler2D;

#common .*frag

#include "LightSource.glsl"
#include "gBuffers_in.glsl"

layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 out_color;

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
	GBuffers gBuffers = LoadGBuffers();
	vec3 color = gBuffers.albedo.rgb;
	float alpha = gBuffers.albedo.a;
	
	out_color = vec4(color, alpha);
}

#shader transparent.frag
void main() {
	GBuffers gBuffers = LoadGBuffers();
	vec3 color = gBuffers.albedo.rgb;
	float alpha = gBuffers.albedo.a;
	
	if (alpha < 0.01) discard;
	
	out_color = vec4(color, alpha);
}
