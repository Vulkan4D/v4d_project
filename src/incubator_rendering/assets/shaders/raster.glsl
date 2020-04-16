#include "core.glsl"
#include "Camera.glsl"

layout(std430, push_constant) uniform GeometryPushConstant{
	uint objectIndex;
	uint geometryIndex;
};

struct V2F {
	vec4 pos;
	vec4 color;
	vec4 normal_dist;
	vec2 uv;
};

#shader visibility.vert

#define VISIBILITY_VERTEX_SHADER
#include "core_buffers.glsl"

layout(location = 0) out V2F v2f;

void main() {
	GeometryInstance geometry = GetGeometryInstance(geometryIndex);
	Vertex vert = GetVertex();
	vec4 pos = vec4(vert.pos, 1);
	
	v2f.pos = geometry.modelViewTransform * pos;
	v2f.color = vert.color;
	v2f.normal_dist = vec4(
		geometry.normalViewTransform * vert.normal,
		// True Distance From Camera
		clamp(distance(vec3(camera.worldPosition), (mat4(geometry.modelTransform) * pos).xyz), float(camera.znear), float(camera.zfar))
		// length(geometry.position)
	);
	v2f.uv = vert.uv;
	
	gl_Position = mat4(camera.projectionMatrix) * v2f.pos;
}

#shader visibility.frag

layout(location = 0) in V2F v2f;

layout(location = 0) out vec2 gBuffer_albedo_geometryIndex; // r = albedo, g = geometry index
layout(location = 1) out vec4 gBuffer_normal_uv; // rgb = normal xyz,  a = uv
layout(location = 2) out vec4 gBuffer_position_dist; // rgb = position xyz,  a = trueDistanceFromCamera

void main() {
	gBuffer_albedo_geometryIndex = vec2(PackColorAsFloat(v2f.color), uintBitsToFloat(geometryIndex));
	gBuffer_normal_uv = vec4(v2f.normal_dist.xyz, PackUVasFloat(v2f.uv));
	gBuffer_position_dist = vec4(v2f.pos.xyz, v2f.normal_dist.w);
}
