#include "core.glsl"
#include "v4d/modules/V4D_hybrid/glsl_includes/pl_transparent.glsl"
#include "v4d/modules/V4D_hybrid/glsl_includes/V2F.glsl"

##################################################################
#shader frag

layout(location = 0) in V2F v2f;

void main() {
	float distance = subpassLoad(in_img_gBuffer_2).w;
	if (distance != 0 && v2f.pos.w > distance) discard;
	out_img_lit = v2f.color;
}

#shader wireframe.frag

layout(location = 0) in V2F v2f;

void main() {
	float distance = subpassLoad(in_img_gBuffer_2).w;
	if (distance != 0 && v2f.pos.w > distance) discard;
	out_img_lit = wireframeColor * max(0.2, dot(vec3(0,0,1), v2f.normal) / 2.0 + 0.5);
}
