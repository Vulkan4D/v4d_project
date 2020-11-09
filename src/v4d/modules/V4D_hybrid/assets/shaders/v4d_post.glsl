#include "core.glsl"
#include "v4d/modules/V4D_hybrid/glsl_includes/pl_post.glsl"

const bool humanEyeExposure = true; // otherwise, use full range

struct V2F {
	vec2 uv;
};

#common .*frag

layout(location = 0) in V2F v2f;

##################################################################
#shader vert

layout (location = 0) out V2F v2f;

void main() {
	v2f.uv = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
	gl_Position = vec4(v2f.uv * 2.0f + -1.0f, 0.0f, 1.0f);
}

##################################################################

#shader txaa.frag.0
void main() {
	vec2 uvCurrent = v2f.uv - camera.txaaOffset;
	vec4 lit = texture(tex_img_lit, uvCurrent);
	out_img_pp = lit;
	
	if (TXAA) {
		float depth;
		if (RayTracedVisibility) {
			depth = texture(tex_img_depth, uvCurrent).r;
		} else {
			depth = texture(tex_img_rasterDepth, uvCurrent).r;
		}
		vec4 clipSpaceCoords = camera.reprojectionMatrix * vec4((v2f.uv * 2 - 1), depth, 1);
		vec2 uvHistory = (clipSpaceCoords.xy / 2 + 0.5) /* - camera.historyTxaaOffset*/ ;
		vec4 history = texture(tex_img_history, uvHistory);
		
		if (length(history.rgb) > 0) {
			vec3 nearColor0 = textureLodOffset(tex_img_lit, uvCurrent, 0.0, ivec2( 1,  0)).rgb;
			vec3 nearColor1 = textureLodOffset(tex_img_lit, uvCurrent, 0.0, ivec2( 0,  1)).rgb;
			vec3 nearColor2 = textureLodOffset(tex_img_lit, uvCurrent, 0.0, ivec2(-1,  0)).rgb;
			vec3 nearColor3 = textureLodOffset(tex_img_lit, uvCurrent, 0.0, ivec2( 0, -1)).rgb;
			// vec3 nearColor4 = textureLodOffset(tex_img_lit, uvCurrent, 0.0, ivec2( 1,  1)).rgb;
			// vec3 nearColor5 = textureLodOffset(tex_img_lit, uvCurrent, 0.0, ivec2(-1,  1)).rgb;
			// vec3 nearColor6 = textureLodOffset(tex_img_lit, uvCurrent, 0.0, ivec2( 1, -1)).rgb;
			// vec3 nearColor7 = textureLodOffset(tex_img_lit, uvCurrent, 0.0, ivec2(-1, -1)).rgb;
			
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
			
			out_img_pp = vec4(mix(history.rgb, lit.rgb, factor), lit.a);
		}
	}
}

#shader history_write.frag.1
void main() {
	out_img_history = vec4(subpassLoad(in_img_pp).rgb * (TXAA?2:1), 1);
}

#shader hdr.frag.2
void main() {
	vec3 color = subpassLoad(in_img_pp).rgb;
	vec2 uv = gl_FragCoord.st / textureSize(tex_img_lit,0).st;
	
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
	out_swapchain = vec4(max(vec3(0),color.rgb), 1.0);
	
	
	// // Debug G-Buffers or Depth
	// float depth;
	// if (RayTracedVisibility) {
	// 	depth = texture(tex_img_depth, uv).r;
	// } else {
	// 	depth = texture(tex_img_rasterDepth, uv).r;
	// }
	// out_swapchain = vec4(vec3(depth*1e3),1);
	
	// out_swapchain = vec4(texture(img_gBuffer_2, uv).w)/12.0;
}

#shader overlay_apply.frag.2
void main() {
	out_swapchain = texture(tex_img_overlay, v2f.uv);
}

