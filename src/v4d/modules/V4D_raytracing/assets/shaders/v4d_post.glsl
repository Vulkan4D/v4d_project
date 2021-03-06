#include "v4d/modules/V4D_raytracing/glsl_includes/pl_post.glsl"

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

// #shader txaa.frag
// void main() {
// 	vec2 uvCurrent = v2f.uv - camera.txaaOffset;
// 	vec4 lit = texture(tex_img_lit, uvCurrent);
// 	out_img_pp = lit;
	
// 	if (TXAA) {
// 		float depth = texture(tex_img_depth, uvCurrent).r;
		
		
// 		vec3 posViewSpace = vec3((v2f.uv * 2 - 1), depth);
// 		vec4 posClipSpace = camera.reprojectionMatrix * vec4(posViewSpace, 1);
// 		vec3 posNDC = posClipSpace.xyz/posClipSpace.w;
// 		vec3 posScreenSpace = posNDC * vec3(0.5, 0.5, 0) + vec3(0.5, 0.5, -posViewSpace.z);
// 		vec2 uvHistory = posScreenSpace.xy;
		
// 		vec4 history = texture(tex_img_history, uvHistory);
		
// 		if (uvHistory.x > 0 && uvHistory.x < 1 && uvHistory.y > 0 && uvHistory.y < 1) {
// 			if (false) {
// 				vec3 nearColor0 = textureLodOffset(tex_img_lit, uvCurrent, 0.0, ivec2( 1,  0)).rgb;
// 				vec3 nearColor1 = textureLodOffset(tex_img_lit, uvCurrent, 0.0, ivec2( 0,  1)).rgb;
// 				vec3 nearColor2 = textureLodOffset(tex_img_lit, uvCurrent, 0.0, ivec2(-1,  0)).rgb;
// 				vec3 nearColor3 = textureLodOffset(tex_img_lit, uvCurrent, 0.0, ivec2( 0, -1)).rgb;
// 				// vec3 nearColor4 = textureLodOffset(tex_img_lit, uvCurrent, 0.0, ivec2( 1,  1)).rgb;
// 				// vec3 nearColor5 = textureLodOffset(tex_img_lit, uvCurrent, 0.0, ivec2(-1,  1)).rgb;
// 				// vec3 nearColor6 = textureLodOffset(tex_img_lit, uvCurrent, 0.0, ivec2( 1, -1)).rgb;
// 				// vec3 nearColor7 = textureLodOffset(tex_img_lit, uvCurrent, 0.0, ivec2(-1, -1)).rgb;
				
// 				vec3 m1 = nearColor0
// 						+ nearColor1
// 						+ nearColor2
// 						+ nearColor3
// 						// + nearColor4
// 						// + nearColor5
// 						// + nearColor6
// 						// + nearColor7
// 				;
// 				vec3 m2 = nearColor0*nearColor0
// 						+ nearColor1*nearColor1 
// 						+ nearColor2*nearColor2
// 						+ nearColor3*nearColor3
// 						// + nearColor4*nearColor4
// 						// + nearColor5*nearColor5
// 						// + nearColor6*nearColor6
// 						// + nearColor7*nearColor7
// 				;

// 				vec3 mu = m1 / 4.0;
// 				vec3 sigma = sqrt(m2 / 4.0 - mu * mu);

// 				float variance_clipping_gamma = 2.0;
// 				vec3 boxMin = mu - variance_clipping_gamma * sigma;
// 				vec3 boxMax = mu + variance_clipping_gamma * sigma;
// 				history.rgb = clamp(history.rgb, boxMin, boxMax);
// 			}
			
// 			float factor = 1.0/8;
// 			// if (depth == 1.0 || depth == 0.0) factor = 1.0;
			
// 			out_img_pp = vec4(mix(history.rgb, lit.rgb, factor), 1-factor);
// 		}
// 	}
// }

#shader hdr.frag
void main() {
	vec2 uv = gl_FragCoord.st / vec2(camera.width, camera.height);
	vec3 color = texture(tex_img_lit, uv).rgb;
	
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
	
	// float depth;
	// 	depth = texture(tex_img_depth, uv).r;
	// out_swapchain = vec4(vec3(depth*1e3),1);
}

#shader overlay_apply.frag
void main() {
	out_swapchain = texture(tex_img_overlay, v2f.uv);
}

