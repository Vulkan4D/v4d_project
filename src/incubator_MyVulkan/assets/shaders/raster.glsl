// This shader needs 3 empty vertices

#version 460 core
#extension GL_ARB_separate_shader_objects : enable

// STAGING STRUCTURES
struct V2F {
	vec4 color;
};

// UNIFORMS
layout(binding = 0) uniform UBO {
	mat4 model;
	mat4 view;
	mat4 proj;
} ubo;

##################################################################

#shader vert

// INPUT
layout(location = 0) in vec3 in_position;
layout(location = 1) in vec4 in_color;

// OUTPUT
layout(location = 0) out V2F v;

// ENTRY POINT
void main() {
	gl_Position = ubo.proj * ubo.view * ubo.model * vec4(in_position, 1);
	v.color = in_color;
}

##################################################################

#shader frag

layout(location = 0) in V2F v;

layout(location = 0) out vec4 o_color;

void main() {
	o_color = v.color;
}
