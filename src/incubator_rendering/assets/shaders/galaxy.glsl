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
layout(location = 1) in uint seed;

layout(location = 0) out uint out_seed;

void main() {
	gl_Position = posr; // passthrough
	out_seed = seed;
}

##################################################################

#shader geom

#include "_noise.glsl"

layout(points) in;
layout(points, max_vertices = 100) out;
layout(location = 0) out vec4 out_color;

layout(location = 0) in uint in_seed[];

void main(void) {
	uint seed = in_seed[0];
	uint fseed = seed + 15;
	
	vec3 wpos = gl_in[0].gl_Position.xyz;
	float radius = gl_in[0].gl_Position.w + RandomFloat(fseed) * 4;
	
	for (int i = 0; i < 100; i++) {
		gl_PointSize = radius;
		gl_Position = mat4(ubo.proj) * mat4(ubo.view) * vec4(wpos + RandomInUnitSphere(seed), 1);
		out_color = vec4(RandomFloat(fseed),RandomFloat(fseed),RandomFloat(fseed), RandomFloat(fseed)*3);
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
