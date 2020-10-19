#include "core.glsl"
#include "v4d/modules/V4D_hybrid/glsl_includes/pl_transparent.glsl"
#include "v4d/modules/V4D_hybrid/glsl_includes/V2F.glsl"

##################################################################
#shader frag

layout(location = 0) in V2F v2f;

void main() {
	// float depthBuffer;
	// if (RayTracedVisibility) {
	// 	depthBuffer = texture(tex_img_depth, v2f.uv).r;
	// } else {
	// 	depthBuffer = texture(tex_img_rasterDepth, v2f.uv).r;
	// }
	
	vec4 color = subpassLoad(in_img_gBuffer_3);
	
	out_img_lit = color;
}
