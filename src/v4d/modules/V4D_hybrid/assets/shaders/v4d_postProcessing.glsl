#include "core.glsl"
#include "Camera.glsl"

const bool humanEyeExposure = true; // otherwise, use full range

#common .*frag

layout(location = 0) in vec2 in_uv;
layout(location = 0) out vec4 out_color;
layout(set = 1, binding = 0) uniform sampler2D litImage;
layout(set = 1, binding = 1) uniform sampler2D uiImage;
layout(set = 1, input_attachment_index = 0, binding = 2) uniform highp subpassInput ppImage;
layout(set = 1, binding = 3) uniform sampler2D historyImage; // previous frame
layout(set = 1, binding = 4) uniform sampler2D depthImage;
layout(set = 1, binding = 5) uniform sampler2D rasterDepthImage;

#include "core_buffers.glsl"

##################################################################
#shader vert

layout (location = 0) out vec2 out_uv;

void main() {
	out_uv = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
	gl_Position = vec4(out_uv * 2.0f + -1.0f, 0.0f, 1.0f);
}

##################################################################

#shader txaa.frag
void main() {
	vec2 uvCurrent = in_uv - camera.txaaOffset;
	vec4 lit = texture(litImage, uvCurrent);
	out_color = lit;
	
	if (TXAA) {
		float depth;
		switch (camera.renderMode) {
			case 0: // raster
				depth = texture(rasterDepthImage, uvCurrent).r;
			break;
			case 1: // ray tracing
				depth = texture(depthImage, uvCurrent).r;
			break;
		}
		vec4 clipSpaceCoords = camera.reprojectionMatrix * vec4((in_uv * 2 - 1), depth, 1);
		vec2 uvHistory = (clipSpaceCoords.xy / 2 + 0.5) /* - camera.historyTxaaOffset*/ ;
		vec4 history = texture(historyImage, uvHistory);
		
		if (length(history.rgb) > 0) {
			vec3 nearColor0 = textureLodOffset(litImage, uvCurrent, 0.0, ivec2( 1,  0)).rgb;
			vec3 nearColor1 = textureLodOffset(litImage, uvCurrent, 0.0, ivec2( 0,  1)).rgb;
			vec3 nearColor2 = textureLodOffset(litImage, uvCurrent, 0.0, ivec2(-1,  0)).rgb;
			vec3 nearColor3 = textureLodOffset(litImage, uvCurrent, 0.0, ivec2( 0, -1)).rgb;
			// vec3 nearColor4 = textureLodOffset(litImage, uvCurrent, 0.0, ivec2( 1,  1)).rgb;
			// vec3 nearColor5 = textureLodOffset(litImage, uvCurrent, 0.0, ivec2(-1,  1)).rgb;
			// vec3 nearColor6 = textureLodOffset(litImage, uvCurrent, 0.0, ivec2( 1, -1)).rgb;
			// vec3 nearColor7 = textureLodOffset(litImage, uvCurrent, 0.0, ivec2(-1, -1)).rgb;
			
			vec3 m1 = nearColor0
					+ nearColor1
					+ nearColor2
					+ nearColor3
					// + nearColor4
					// + nearColor5
					// + nearColor6
					// + nearColor7
			;
			vec3 m2 = nearColor0*nearColor0
					+ nearColor1*nearColor1 
					+ nearColor2*nearColor2
					+ nearColor3*nearColor3
					// + nearColor4*nearColor4
					// + nearColor5*nearColor5
					// + nearColor6*nearColor6
					// + nearColor7*nearColor7
			;

			vec3 mu = m1 / 4.0;
			vec3 sigma = sqrt(m2 / 4.0 - mu * mu);

			float variance_clipping_gamma = 1.0;
			vec3 boxMin = mu - variance_clipping_gamma * sigma;
			vec3 boxMax = mu + variance_clipping_gamma * sigma;
			history.rgb = clamp(history.rgb, boxMin, boxMax);
			
			float factor = 1.0/8.0;
			// if (depth == 1.0 || depth == 0.0) factor = 1.0;
			
			out_color = vec4(mix(history.rgb, lit.rgb, factor), lit.a);
		}
	}
}

#shader history.frag
void main() {
	out_color = vec4(subpassLoad(ppImage).rgb * (TXAA?2:1), 1);
}

#shader hdr.frag
void main() {
	vec3 color = subpassLoad(ppImage).rgb;
	vec2 in_uv = gl_FragCoord.st / textureSize(litImage,0).st;
	
	// HDR ToneMapping (Reinhard)
	if (HDR) {
		float exposure;
		if (humanEyeExposure) {
			// Human Eye Exposure
			exposure = 2.0 / (camera.luminance.r + camera.luminance.g + camera.luminance.b) * camera.luminance.a;
			color = vec3(1.0) - exp(-color * clamp(exposure, 0.0001, 10));
		} else {
			// Full range Exposure
			exposure = 1.0 / (camera.luminance.r + camera.luminance.g + camera.luminance.b) * camera.luminance.a;
			color = vec3(1.0) - exp(-color * clamp(exposure, 1e-100, 1e100));
		}
	}
	
	// Contrast / Brightness
	if (camera.contrast != 1.0 || camera.brightness != 1.0) {
		color = mix(color * camera.brightness, mix(vec3(0.5), color, camera.contrast), 0.5);
	}
	
	// Gamma correction 
	if (GammaCorrection) {
		const float gamma = camera.gamma;
		color = pow(color, vec3(1.0 / gamma));
	}
	
	// Final color
	out_color = vec4(max(vec3(0),color.rgb), 1.0);
}

#shader ui.frag
void main() {
	vec4 ui = texture(uiImage, in_uv);
	if (length(ui.rgb) > 0) {
		ui.a = length(ui.rgb) + 0.2;
	}
	out_color = ui;
}

