#version 460 core

precision highp int;
precision highp float;
precision highp sampler2D;

#include "_v4d_baseDescriptorSet.glsl"

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
layout(set = 1, binding = 0) uniform sampler2D tmpImage;
layout(set = 1, binding = 1) uniform sampler2D uiImage;
layout(set = 1, binding = 2) uniform sampler2D depthStencilImage;

const bool opaqueUI = false;

void main() {
	vec3 color = texture(tmpImage, uv).rgb;
	vec4 ui = texture(uiImage, uv);
	vec2 depthStencil = texture(depthStencilImage, gl_FragCoord.st / vec2(textureSize(depthStencilImage, 0))).rg;
	
	// Post processing here
	//...
	
	// HDR ToneMapping (Reinhard)
	const float exposure = 1.0;
	color = vec3(1.0) - exp(-color * exposure);
	
	// Gamma correction 
	const float gamma = 2.2;
	color = pow(color, vec3(1.0 / gamma));
	
	
	// Final color
	out_color = vec4(color.rgb, 1.0);
	
	// Add UI Overlay
	if (length(ui.rgb) > 0) {
		if (opaqueUI) {
			out_color.rgb = ui.rgb;
		} else {
			out_color.rgb += ui.rgb;
		}
	}
	
	// out_color = vec4(depthStencil.r*10000000);
}

