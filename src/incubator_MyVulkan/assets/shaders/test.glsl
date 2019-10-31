#version 460 core
#extension GL_ARB_separate_shader_objects : enable

// STAGING STRUCTURES
struct V2F {
	vec4 color;
    vec2 uv;
};

// UNIFORMS
layout(binding = 0) uniform UBO {
    mat4 model;
    mat4 view;
    mat4 proj;
} ubo;
layout(binding = 1) uniform sampler2D tex;

##################################################################

#shader vert

// INPUT
layout(location = 0) in vec3 in_position;
layout(location = 1) in vec4 in_color;
layout(location = 2) in vec2 in_uv;

// OUTPUT
layout(location = 0) out V2F v;

// ENTRY POINT
void main() {
    gl_Position = ubo.proj * ubo.view * ubo.model * vec4(in_position, 1);
    v.color = in_color;
    v.uv = in_uv;
}

##################################################################

#shader frag

// INPUT
layout(location = 0) in V2F v;

// OUTPUT
layout(location = 0) out vec4 o_color;

// ENTRY POINT
void main() {
    o_color = texture(tex, v.uv) * v.color;
}

