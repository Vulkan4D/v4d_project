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
	dvec3 cameraPosition;
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
// layout(location = 1) out vec4 oitBuffer;

void main() {
	o_color = v.color;
	
	// float oit_weight = o_color.a;
	// o_color = vec4(o_color.rgb * o_color.a, o_color.a) * oit_weight;
	// oitBuffer = vec4(0,0,0, o_color.a);
}
