// This shader needs 3 empty vertices

#version 460 core
#extension GL_ARB_separate_shader_objects : enable

// STAGING STRUCTURES
struct V2F {
	vec4 color;
};

// UNIFORMS
layout(binding = 0) uniform UBO {
    // mat4 model;
    mat4 view;
    mat4 proj;
} ubo;

##################################################################

#shader vert

// INPUT
layout(location = 0) in vec3 in_position;
layout(location = 1) in float in_refl;
layout(location = 2) in vec4 in_color;

// OUTPUT
layout(location = 0) out V2F v;

// vec2 positions[3] = vec2[](
//     vec2(0.0, -0.5),
//     vec2(0.5, 0.5),
//     vec2(-0.5, 0.5)
// );

// vec3 colors[3] = vec3[](
//     vec3(1.0, 0.0, 0.0),
//     vec3(0.0, 1.0, 0.0),
//     vec3(0.0, 0.0, 1.0)
// );

// ENTRY POINT
void main() {
    gl_Position = ubo.proj * ubo.view /* ubo.model*/ * vec4(in_position, 1);
    v.color = in_color;
}

##################################################################

#shader frag

layout(location = 0) in V2F v;

layout(location = 0) out vec4 o_color;

void main() {
    o_color = v.color;
}
