#version 460 core
#extension GL_ARB_enhanced_layouts : enable

precision highp int;
precision highp float;
precision highp sampler2D;

layout(std430, push_constant) uniform PlanetChunk {
	mat4 mvp;
	vec4 testColor;
} planetChunk;

##################################################################
#shader vert

layout(location = 0) in vec4 pos;
// layout(location = 1) in vec4 normal;

void main() {
	//TODO take planet rotation into consideration
	gl_Position = planetChunk.mvp * vec4(pos.xyz, 1);
}

##################################################################

#shader geom

layout(triangles) in;
layout(line_strip, max_vertices = 6) out;

void main() {
	gl_Position = gl_in[0].gl_Position;
	EmitVertex();
	gl_Position = gl_in[1].gl_Position;
	EmitVertex();
	EndPrimitive();
	
	gl_Position = gl_in[1].gl_Position;
	EmitVertex();
	gl_Position = gl_in[2].gl_Position;
	EmitVertex();
	EndPrimitive();
	
	gl_Position = gl_in[2].gl_Position;
	EmitVertex();
	gl_Position = gl_in[0].gl_Position;
	EmitVertex();
	EndPrimitive();
}

##################################################################

#shader frag

layout(location = 0) out vec4 out_color;

void main() {
	out_color = planetChunk.testColor;
}

