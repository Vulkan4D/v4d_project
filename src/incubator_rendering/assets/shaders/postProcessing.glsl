#version 460 core
#extension GL_ARB_separate_shader_objects : enable

#shader vert

layout (location = 0) out vec2 outUV;

void main() 
{
    outUV = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    gl_Position = vec4(outUV * 2.0f + -1.0f, 0.0f, 1.0f);
}

##################################################################

#shader frag

layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 color;
layout(set = 0, binding = 0) uniform sampler2D image;

void main() {
	color = texture(image, uv);
	// Post processing here
	//...
}

