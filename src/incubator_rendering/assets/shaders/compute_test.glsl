#version 460 core
#shader comp

layout(set = 1, binding = 0, rgba32f) uniform writeonly imageCube galaxyCubeMap;

void main() {
	
	// imageStore(galaxyCubeMap, ivec3(gl_GlobalInvocationID), vec4(1,0,0,1));
}
