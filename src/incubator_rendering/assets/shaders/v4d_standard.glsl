#version 460 core

precision highp int;
precision highp float;
precision highp sampler2D;

#include "_v4d_baseDescriptorSet.glsl"

#common .*surface\.frag

// G-Buffers
layout(location = 0) out highp vec3 gBuffer_albedo;
layout(location = 1) out lowp  vec3 gBuffer_normal;
layout(location = 2) out lowp float gBuffer_roughness;
layout(location = 3) out lowp float gBuffer_metallic;
layout(location = 4) out lowp float gBuffer_scatter;
layout(location = 5) out lowp float gBuffer_occlusion;
layout(location = 6) out highp vec3 gBuffer_emission;
layout(location = 7) out highp vec3 gBuffer_position;


##################################################################
#shader vert

void main() 
{
	// gl_Position = 
}

##################################################################

#shader opaque.surface.frag
void main() {
	gBuffer_albedo = vec3(0,1,0);
}

#shader transparent.surface.frag
void main() {
	gBuffer_albedo = vec3(0,1,0);
}
