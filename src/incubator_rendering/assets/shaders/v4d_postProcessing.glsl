#version 460 core

precision highp int;
precision highp float;
precision highp sampler2D;

##################################################################
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
layout(location = 0) out vec4 out_color;
layout(set = 0, binding = 0) uniform sampler2D image;

void main() {
	vec3 color = texture(image, uv).xyz;
	
	// Post processing here
	//...
	
	// // HDR ToneMapping (Reinhard)
	// const float exposure = 1.0;
	// color = vec3(1.0) - exp(-color * exposure);
	
	// // Gamma correction 
	// const float gamma = 2.2;
	// color = pow(color, vec3(1.0 / gamma));
	
	
	out_color = vec4(color, 1.0);
}

