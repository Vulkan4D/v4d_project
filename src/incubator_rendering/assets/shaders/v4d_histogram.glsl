#version 460 core
#shader comp

layout(set = 0, binding = 0, rgba32f) uniform readonly image2D thumbnail;
layout(set = 0, binding = 1, rgba32f) uniform writeonly image2D histogram;

void main() {
	vec4 color = imageLoad(thumbnail, ivec2(gl_GlobalInvocationID.xy));
	imageStore(histogram, ivec2(gl_GlobalInvocationID), color);
}
