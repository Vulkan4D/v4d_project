#include "core.glsl"
#include "v4d/modules/V4D_hybrid/glsl_includes/pl_histogram.glsl"

const float minAcceptedLuminancePerPixel = 0.01;
const float maxAcceptedLuminancePerPixel = 100;

#shader comp

void main() {
	if (DebugWireframe) return;
	
	uvec2 size = imageSize(img_thumbnail);
	uvec2 imageOffset = size/4;
	vec4 luminance = vec4(0);
	for (uint x = 0; x < size.s/2; ++x) {
		for (uint y = 0; y < size.t/2; ++y) {
			vec4 color = imageLoad(img_thumbnail, ivec2(imageOffset + uvec2(x,y)));
			luminance += vec4(max(vec3(minAcceptedLuminancePerPixel), min(vec3(maxAcceptedLuminancePerPixel), color.rgb)), 1);
		}
	}
	if (HDR) {
		buffer_totalLuminance = vec4(mix(buffer_totalLuminance.rgb, luminance.rgb, 0.008), luminance.a);
	} else {
		buffer_totalLuminance = vec4(luminance.rgb, luminance.a);
	}
}
