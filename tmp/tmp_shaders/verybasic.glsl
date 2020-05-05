#version 460 core
#extension GL_ARB_separate_shader_objects : enable

#shader vert

layout(location = 0) out vec4 out_color;

const vec2 positions[4] = {
	vec2(-.5, -.5),
	vec2(-.5, 0.5),
	vec2(0.5, -.5),
	vec2(0.5, 0.5),
};

const vec4 colors[4] = {
	vec4(1.0, 0.0, 0.0, 1.0),
	vec4(0.0, 1.0, 0.0, 1.0),
	vec4(0.0, 0.0, 1.0, 1.0),
	vec4(0.5, 0.5, 0.5, 1.0),
};

void main() {
	gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);
	out_color = colors[gl_VertexIndex];
}

#shader frag

layout(location = 0) in vec4 in_color;
layout(location = 0) out vec4 out_color;

void main() {
	out_color = in_color;
}
