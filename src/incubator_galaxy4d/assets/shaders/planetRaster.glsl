#version 460 core
#extension GL_ARB_enhanced_layouts : enable

precision highp int;
precision highp float;
precision highp sampler2D;

// layout(set = 1, binding = 0) uniform Planets {
// 	PlanetInfo planets[255];
// };

layout(std430, push_constant) uniform PlanetChunk {
	dvec3 absolutePosition;
	double radius;
} planetChunk;

#include "incubator_rendering/assets/shaders/_v4d_baseDescriptorSet.glsl"

##################################################################
#shader vert

layout(location = 0) in vec4 pos;
layout(location = 1) in vec4 normal;

void main() {
	gl_Position = cameraUBO.projection * vec4(cameraUBO.origin * dmat4(cameraUBO.relativeView) * (dvec4(planetChunk.absolutePosition, 1) + dvec4(pos.xyz, 0)));
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
	out_color = vec4(1);
}

