// This shader needs 6 empty vertices

#version 460 core
#extension GL_ARB_separate_shader_objects : enable

struct V2F {
	vec3 color;
};

##################################################################

#shader vert

layout(location = 0) out V2F v;

vec2 positions[6] = vec2[](
    vec2(-1, 1),
    vec2(-1,-1),
    vec2( 1,-1),
    vec2(-1, 1),
    vec2( 1,-1),
    vec2( 1, 1)
);

vec3 colors[6] = vec3[](
    vec3(1.0, 0.0, 0.0),
    vec3(0.0, 1.0, 0.0),
    vec3(0.0, 0.0, 1.0),
    vec3(1.0, 0.0, 0.0),
    vec3(0.0, 0.0, 1.0),
    vec3(0.0, 1.0, 1.0)
);

void main() {
    gl_Position = vec4(positions[gl_VertexIndex], 0, 1);
    v.color = colors[gl_VertexIndex];
}

##################################################################

#shader frag

layout(location = 0) in V2F v;

layout(location = 0) out vec4 o_color;

void main() {
    o_color = vec4(v.color, 1.0);
}
