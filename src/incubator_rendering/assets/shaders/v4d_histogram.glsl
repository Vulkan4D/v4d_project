#version 460 core
#shader comp

layout(set = 0, binding = 0, rgba16f) uniform readonly image2D thumbnail;
layout(set = 0, binding = 1) buffer writeonly OutputData {
	vec4 totalLuminance;
};

void main() {
	uvec2 size = imageSize(thumbnail);
	uvec2 imageOffset = size/4;
	vec4 luminance = vec4(0);
	for (uint x = 0; x < size.s/2; ++x) {
		for (uint y = 0; y < size.t/2; ++y) {
			vec4 color = imageLoad(thumbnail, ivec2(imageOffset + uvec2(x,y)));
			luminance += vec4(max(vec3(0), color.rgb), 1);
		}
	}
	totalLuminance = vec4(mix(totalLuminance.rgb, luminance.rgb, 0.005), luminance.a);
}
