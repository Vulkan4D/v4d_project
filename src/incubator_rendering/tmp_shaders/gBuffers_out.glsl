#include "GBuffers.glsl"

layout(location = 0) out highp vec4 gBuffer_albedo;
layout(location = 1) out highp vec4 gBuffer_normal;
layout(location = 2) out highp vec4 gBuffer_emission;
layout(location = 3) out highp vec4 gBuffer_position;

void WriteGBuffers(GBuffers gBuffers) {
	gBuffer_albedo = vec4(gBuffers.albedo, gBuffers.alpha);
	gBuffer_normal = vec4(gBuffers.normal, gBuffers.roughness);
	gBuffer_emission = vec4(gBuffers.emission, gBuffers.metallic);
	gBuffer_position = vec4(gBuffers.position, gBuffers.dist);
}
