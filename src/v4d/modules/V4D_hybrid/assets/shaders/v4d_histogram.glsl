#version 460 core
#shader comp

#include "Camera.glsl"

layout(set = 1, binding = 0, rgba16f) uniform readonly image2D thumbnail;
layout(set = 1, binding = 1) buffer writeonly OutputData {
	vec4 totalLuminance;
};

const float minAcceptedLuminancePerPixel = 0.01;
const float maxAcceptedLuminancePerPixel = 100;

void main() {
	if (camera.debug) return;
	
	uvec2 size = imageSize(thumbnail);
	uvec2 imageOffset = size/4;
	vec4 luminance = vec4(0);
	for (uint x = 0; x < size.s/2; ++x) {
		for (uint y = 0; y < size.t/2; ++y) {
			vec4 color = imageLoad(thumbnail, ivec2(imageOffset + uvec2(x,y)));
			luminance += vec4(max(vec3(minAcceptedLuminancePerPixel), min(vec3(maxAcceptedLuminancePerPixel), color.rgb)), 1);
		}
	}
	if (HDR) {
		totalLuminance = vec4(mix(totalLuminance.rgb, luminance.rgb, 0.008), luminance.a);
	} else {
		totalLuminance = vec4(luminance.rgb, luminance.a);
	}
}