#version 460 core

precision highp int;
precision highp float;
precision highp sampler2D;

#include "_v4d_baseDescriptorSet.glsl"

##################################################################
#shader vert

layout (location = 0) out vec2 outUV;

void main() 
{
    outUV = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    gl_Position = vec4(outUV * 2.0f + -1.0f, 0.0f, 1.0f);
}

##################################################################
#shader frag

layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 out_color;
layout(set = 1, binding = 0) uniform sampler2D tmpImage;

void main() {
	out_color = vec4(texture(tmpImage, uv).rgb, 1.0);
}

