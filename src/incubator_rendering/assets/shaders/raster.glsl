// This shader needs 3 empty vertices

#version 460 core
#extension GL_ARB_separate_shader_objects : enable

// STAGING STRUCTURES
struct V2F {
	vec4 color;
};

// UNIFORMS
layout(set = 0, binding = 0) uniform UBO {
	dmat4 proj;
	dmat4 view;
	dmat4 model;
} ubo;

##################################################################

#shader vert

// INPUT
layout(location = 0) in double posX;
layout(location = 1) in double posY;
layout(location = 2) in double posZ;
layout(location = 3) in vec4 in_color;

// OUTPUT
layout(location = 0) out V2F v;

// ENTRY POINT
void main() {
	gl_Position = vec4(ubo.proj * ubo.view * ubo.model * dvec4(posX, posY, posZ, 1));
	v.color = in_color;
}

##################################################################

#shader frag

layout(location = 0) in V2F v;

layout(location = 0) out vec4 o_color;

void main() {
	o_color = v.color;
}

##################################################################

#shader pp.vert

layout (location = 0) out vec2 outUV;

void main() 
{
    outUV = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    gl_Position = vec4(outUV * 2.0f + -1.0f, 0.0f, 1.0f);
}

##################################################################

#shader pp.frag

layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 color;
layout(set = 1, binding = 0) uniform sampler2D image;

void main() {
	color = texture(image, uv);
	// Post processing here
	
}

