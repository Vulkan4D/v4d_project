#version 460 core

precision highp int;
precision highp float;
precision highp sampler2D;

#include "Camera.glsl"
#include "_v4d_baseDescriptorSet.glsl"

#common .*frag

layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 out_color;
layout(set = 1, binding = 0) uniform sampler2D litImage;
layout(set = 1, binding = 1) uniform sampler2D depthStencilImage;
layout(set = 1, binding = 2) uniform sampler2D historyImage; // previous frame
layout(set = 1, binding = 3) uniform sampler2D uiImage;
layout(set = 1, input_attachment_index = 0, binding = 4) uniform highp subpassInput ppImage;

##################################################################
#shader vert

layout (location = 0) out vec2 outUV;

void main() {
	outUV = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
	gl_Position = vec4(outUV * 2.0f + -1.0f, 0.0f, 1.0f);
}

##################################################################

#shader txaa.frag
void main() {
	vec4 lit = texture(litImage, uv);
	if (camera.txaa == 0) {
		out_color = vec4(lit.rgb, lit.a);
	} else {
		vec4 history = texture(historyImage, uv);
		
		const float VARIANCE_CLIPPING_GAMMA = 1.0;

		vec3 nearColor0 = textureLodOffset(litImage, uv, 0.0, ivec2(1, 0)).rgb;
		vec3 nearColor1 = textureLodOffset(litImage, uv, 0.0, ivec2(0, 1)).rgb;
		vec3 nearColor2 = textureLodOffset(litImage, uv, 0.0, ivec2(-1, 0)).rgb;
		vec3 nearColor3 = textureLodOffset(litImage, uv, 0.0, ivec2(0, -1)).rgb;

		vec3 m1 = lit.rgb + nearColor0 + nearColor1 + nearColor2 + nearColor3;
		vec3 m2 = lit.rgb * lit.rgb + nearColor0 * nearColor0 + nearColor1 * nearColor1 
			+ nearColor2 * nearColor2 + nearColor3 * nearColor3;

		vec3 mu = m1 / 5.0;
		vec3 sigma = sqrt(m2 / 5.0 - mu * mu);

		vec3 boxMin = mu - VARIANCE_CLIPPING_GAMMA * sigma;
		vec3 boxMax = mu + VARIANCE_CLIPPING_GAMMA * sigma;

		history.rgb = clamp(history.rgb, boxMin, boxMax);
		
		if (length(history.rgb) > 0) {
			out_color = vec4(mix(history.rgb, lit.rgb, 1.0/float(camera.txaa*2)), lit.a);
		} else {
			out_color = vec4(lit.rgb, lit.a);
		}
	}
}

#shader history.frag
void main() {
	out_color = vec4(subpassLoad(ppImage).rgb * (camera.txaa>0?2.0:0.0), texture(depthStencilImage, uv).r);
}

#shader hdr.frag
void main() {
	vec3 color = subpassLoad(ppImage).rgb;

	// HDR ToneMapping (Reinhard)
	const float exposure = 1.0;
	color = vec3(1.0) - exp(-color * exposure);
	
	// Gamma correction 
	const float gamma = 2.2;
	color = pow(color, vec3(1.0 / gamma));
	
	// Final color
	out_color = vec4(max(vec3(0),color.rgb), 1.0);
}

#shader ui.frag
void main() {
	vec4 ui = texture(uiImage, uv);
	if (length(ui.rgb) > 0) {
		ui.a = length(ui.rgb) + 0.2;
	}
	out_color = ui;
}

