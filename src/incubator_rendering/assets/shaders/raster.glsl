#version 460 core
#extension GL_ARB_separate_shader_objects : enable

struct V2F {
	vec4 color;
};

layout(set = 0, binding = 0) uniform UBO {
	dmat4 proj;
	dmat4 view;
	dmat4 model;
	dvec3 cameraPosition;
} ubo;

##################################################################

#shader vert

layout(location = 0) in double posX;
layout(location = 1) in double posY;
layout(location = 2) in double posZ;
layout(location = 3) in vec4 in_color;

layout(location = 0) out V2F v;

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
