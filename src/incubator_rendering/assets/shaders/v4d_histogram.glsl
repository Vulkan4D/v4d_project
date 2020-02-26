#version 460 core
#shader comp

layout(set = 0, binding = 0, rgba32f) uniform readonly image2D thumbnail;
layout(set = 0, binding = 1, rgba32f) uniform writeonly image2D histogram;

void main() {
	vec4 color = imageLoad(thumbnail, ivec2(gl_GlobalInvocationID.xy));
	color += imageLoad(thumbnail, ivec2(gl_GlobalInvocationID.xy)+ivec2(1,1));
	color += imageLoad(thumbnail, ivec2(gl_GlobalInvocationID.xy)+ivec2(-1,-1));
	color += imageLoad(thumbnail, ivec2(gl_GlobalInvocationID.xy)+ivec2(1,-1));
	color += imageLoad(thumbnail, ivec2(gl_GlobalInvocationID.xy)+ivec2(-1,1));
	color += imageLoad(thumbnail, ivec2(gl_GlobalInvocationID.xy)+ivec2(1,0));
	color += imageLoad(thumbnail, ivec2(gl_GlobalInvocationID.xy)+ivec2(-1,0));
	color += imageLoad(thumbnail, ivec2(gl_GlobalInvocationID.xy)+ivec2(0,-1));
	color += imageLoad(thumbnail, ivec2(gl_GlobalInvocationID.xy)+ivec2(0,1));
	imageStore(histogram, ivec2(gl_GlobalInvocationID), color/9);
}
