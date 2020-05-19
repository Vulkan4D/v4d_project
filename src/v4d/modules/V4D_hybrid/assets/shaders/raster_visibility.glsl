#include "core.glsl"
#include "v4d/modules/V4D_hybrid/glsl_includes/pl_visibility_raster.glsl"

struct V2F {
	vec4 color;
	vec4 pos;
	vec3 normal;
	vec2 uv;
};

###########################################
#shader vert

layout(location = 0) out V2F v2f;

void main() {
	GeometryInstance geometry = GetGeometryInstance(geometryIndex);
	Vertex vert = GetVertex();
	
	v2f.color = vert.color;
	v2f.pos = (geometry.modelViewTransform * vec4(vert.pos, 1));
	v2f.normal = geometry.normalViewTransform * vert.normal;
	v2f.uv = vert.uv;
	
	// Calculate actual distance from camera
	v2f.pos.w = clamp(distance(vec3(camera.worldPosition), (mat4(geometry.modelTransform) * vec4(vert.pos,1)).xyz), float(camera.znear), float(camera.zfar));
	
	gl_Position = mat4(camera.projectionMatrix) * vec4(v2f.pos.xyz, 1);
}

###########################################
#shader frag

layout(location = 0) in V2F v2f;

void main() {
	// Passthrough Material
	pbrGBuffers.viewSpacePosition = v2f.pos.xyz;
	pbrGBuffers.viewSpaceNormal = v2f.normal;
	pbrGBuffers.uv = v2f.uv;
	pbrGBuffers.albedo = v2f.color.rgb;
	pbrGBuffers.emit = 0;
	pbrGBuffers.metallic = 0.1;
	pbrGBuffers.roughness = 0.6;
	
	pbrGBuffers.realDistanceFromCamera = v2f.pos.w;
	WritePbrGBuffers();
}

