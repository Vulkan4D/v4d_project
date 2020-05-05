#version 460 core
#shader comp

layout(set = 1, binding = 0, rgba32f) uniform /*readonly|writeonly*/ imageCube galaxyCubeMap;

void main() {
	// Equivalent of the "GalaxyFade" fragment shader
	vec4 color = imageLoad(galaxyCubeMap, ivec3(gl_GlobalInvocationID));
	imageStore(galaxyCubeMap, ivec3(gl_GlobalInvocationID), color - vec4(0.004,0.004,0.004, 0));
}
