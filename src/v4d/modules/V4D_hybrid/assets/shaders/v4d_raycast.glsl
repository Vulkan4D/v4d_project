#include "core.glsl"
#include "v4d/modules/V4D_hybrid/glsl_includes/pl_raycast.glsl"

#shader comp

void main() {
	ivec2 imageOffset = ivec2(imageSize(img_gBuffer_1)) / 2;
	
	buffer_normal_uv = vec4(imageLoad(img_gBuffer_1, imageOffset));
	buffer_position_distance = vec4(imageLoad(img_gBuffer_2, imageOffset));
	buffer_custom = uvec4(imageLoad(img_gBuffer_4, imageOffset));
}
