#version 460 core

precision highp int;
precision highp float;
precision highp sampler2D;

#include "Camera.glsl"
#include "_v4d_baseDescriptorSet.glsl"

#common .*frag

layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 out_color;
layout(set = 1, binding = 0) uniform sampler2D gBuffer_albedo;
layout(set = 1, binding = 1) uniform sampler2D gBuffer_normal;
layout(set = 1, binding = 2) uniform sampler2D gBuffer_emission;
layout(set = 1, binding = 3) uniform sampler2D gBuffer_position;
layout(set = 1, binding = 4) uniform sampler2D litImage;
layout(set = 1, binding = 5) uniform sampler2D depthStencilImage;
layout(set = 1, binding = 6) uniform sampler2D historyImage; // previous frame
layout(set = 1, binding = 7) uniform sampler2D uiImage;
layout(set = 1, binding = 8) uniform sampler2D histogramImage;
layout(set = 1, input_attachment_index = 0, binding = 9) uniform highp subpassInput ppImage;

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
	vec2 uvCurrent = uv - camera.txaaOffset;
	
	vec4 lit = texture(litImage, uvCurrent);
	out_color = lit;
	
	if (camera.txaa) {
		float depth = texture(depthStencilImage, uvCurrent).r;
		vec4 clipSpaceCoords = camera.reprojectionMatrix * vec4((uv * 2 - 1), depth, 1);
		vec2 uvHistory = (clipSpaceCoords.xy / 2 + 0.5) /* - camera.historyTxaaOffset*/ ;
		vec4 history = texture(historyImage, uvHistory);
		
		if (length(history.rgb) > 0) {
			vec3 nearColor0 = textureLodOffset(litImage, uvCurrent, 0.0, ivec2(1, 0)).rgb;
			vec3 nearColor1 = textureLodOffset(litImage, uvCurrent, 0.0, ivec2(0, 1)).rgb;
			vec3 nearColor2 = textureLodOffset(litImage, uvCurrent, 0.0, ivec2(-1, 0)).rgb;
			vec3 nearColor3 = textureLodOffset(litImage, uvCurrent, 0.0, ivec2(0, -1)).rgb;
			vec3 nearColor4 = textureLodOffset(litImage, uvCurrent, 0.0, ivec2(1, 1)).rgb;
			vec3 nearColor5 = textureLodOffset(litImage, uvCurrent, 0.0, ivec2(-1, 1)).rgb;
			vec3 nearColor6 = textureLodOffset(litImage, uvCurrent, 0.0, ivec2(-1, 0)).rgb;
			vec3 nearColor7 = textureLodOffset(litImage, uvCurrent, 0.0, ivec2(0, -1)).rgb;
			
			vec3 m1 = nearColor0
					+ nearColor1
					+ nearColor2
					+ nearColor3
					+ nearColor4
					+ nearColor5
					+ nearColor6
					+ nearColor7
			;
			vec3 m2 = nearColor0*nearColor0
					+ nearColor1*nearColor1 
					+ nearColor2*nearColor2
					+ nearColor3*nearColor3
					+ nearColor4*nearColor4
					+ nearColor5*nearColor5
					+ nearColor6*nearColor6
					+ nearColor7*nearColor7
			;

			vec3 mu = m1 / 8.0;
			vec3 sigma = sqrt(m2 / 8.0 - mu * mu);

			float variance_clipping_gamma = 2.0;
			vec3 boxMin = mu - variance_clipping_gamma * sigma;
			vec3 boxMax = mu + variance_clipping_gamma * sigma;
			history.rgb = clamp(history.rgb, boxMin, boxMax);
			
			out_color = vec4(mix(history.rgb, lit.rgb, 1.0/8.0), lit.a);
		}
	}
}

#shader history.frag
void main() {
	out_color = vec4(subpassLoad(ppImage).rgb * (camera.txaa?2:1), texture(depthStencilImage, uv).r);
}

#shader hdr.frag
void main() {
	vec3 color = subpassLoad(ppImage).rgb;
	vec2 uv = gl_FragCoord.st / textureSize(litImage,0).st;
	
	// // HDR ToneMapping (Reinhard)
	// const float exposure = 1.0;
	// color = vec3(1.0) - exp(-color * exposure);
	
	// Gamma correction 
	const float gamma = 2.2;
	color = pow(color, vec3(1.0 / gamma));
	
	// Debug G-Buffers
	if (camera.debug) {
		if (uv.t > 0.75) {
			if (uv.s < 0.25) {
				color = vec3(texture(gBuffer_position, uv).a) / 100.0;
				if (color.r == 0) color = vec3(1,0,1);
			}
			if (uv.s > 0.25 && uv.s < 0.5) {
				color = texture(gBuffer_normal, uv).rgb;
			}
			if (uv.s > 0.5 && uv.s < 0.75) {
				color = vec3(texture(gBuffer_normal, uv).a);
			}
			if (uv.s > 0.75) {
				color = vec3(texture(gBuffer_emission, uv).a);
				if (color.r < 0) {
					color *= vec3(0,0,-1);
				}
			}
		}
		if (uv.s > 0.75 && uv.t < 0.25)
			color = texture(histogramImage, (uv-vec2(0.75,0)) * 4).rgb;
	}

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

