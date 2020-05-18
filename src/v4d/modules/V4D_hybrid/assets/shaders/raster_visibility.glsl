#include "core.glsl"
#define GEOMETRY_BUFFERS_ACCESS readonly
#include "v4d/modules/V4D_hybrid/glsl_includes/pl_visibility_raster.glsl"

struct V2F {
	vec4 pos;
	vec4 color;
	vec4 normal_dist;
	vec2 uv;
};

###########################################
#shader vert

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
		// length(geometry.viewPosition)
	);
	v2f.uv = vert.uv;
	
	gl_Position = mat4(camera.projectionMatrix) * v2f.pos;
}

###########################################
#shader frag

layout(location = 0) in V2F v2f;

void main() {
	out_img_gBuffer_0 = vec2(PackColorAsFloat(v2f.color), uintBitsToFloat(geometryIndex));
	out_img_gBuffer_1 = vec4(v2f.normal_dist.xyz, PackUVasFloat(v2f.uv));
	out_img_gBuffer_2 = vec4(v2f.pos.xyz, v2f.normal_dist.w);
}

