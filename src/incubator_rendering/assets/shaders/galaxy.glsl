#version 460 core
#extension GL_ARB_separate_shader_objects : enable

layout(set = 0, binding = 0) uniform UBO {
	dmat4 proj;
	dmat4 view;
	dmat4 model;
	// dvec3 cameraPosition;
} ubo;

##################################################################

#shader vert

layout(location = 0) in vec4 posr;

void main() {
	gl_Position = posr; // passthrough
}

##################################################################

#shader geom

#include "_noise.glsl"

layout(points) in;
layout(points, max_vertices = 128) out;
layout(location = 0) out vec4 out_color;

void main(void) {
	vec3 wpos = gl_in[0].gl_Position.xyz;
	float radius = gl_in[0].gl_Position.w;
	
	uint seed = 100;
	
	for (int i = 0; i < 128; i++) {
		gl_PointSize = radius;
		gl_Position = mat4(ubo.proj) * mat4(ubo.view) * vec4(wpos + RandomInUnitSphere(seed) * 10, 1);
		out_color = vec4(1,1,1, 1);
		EmitVertex();
	}
}

##################################################################

#shader frag

layout(location = 0) in vec4 in_color;
layout(location = 0) out vec4 out_color;

void main() {
	float center = 1 - pow(length(gl_PointCoord.st * 2 - 1), 1.0 / max(0.7, in_color.a));
	out_color = vec4(in_color.rgb * in_color.a, 1) * center;
}
