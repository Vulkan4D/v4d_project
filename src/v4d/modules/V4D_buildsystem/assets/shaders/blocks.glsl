#include "core.glsl"

struct V2F {
	vec4 color;
	vec4 pos;
	vec3 normal;
};

#common .*rchit

#include "v4d/modules/V4D_hybrid/glsl_includes/rtx.glsl"
#include "v4d/modules/V4D_hybrid/glsl_includes/pl_visibility_rays.glsl"

#############################################################
#shader rchit

hitAttributeEXT vec3 hitAttribs;

layout(location = 0) rayPayloadInEXT RayPayload_visibility ray;

#include "rtx_fragment.glsl"

void main() {
	Fragment fragment = GetHitFragment(true);
	
	ray.customData = GenerateCustomData(fragment.objectIndex, /*type8*/0, /*flags32*/0, /*custom32*/0, /*custom32*/0);
	ray.viewSpacePosition = fragment.hitPoint;
	ray.viewSpaceNormal = fragment.viewSpaceNormal;
	ray.albedo = fragment.color.rgb;
	ray.emit = 0;
	ray.uv = uintBitsToFloat(fragment.customData);
	ray.metallic = 0.0;
	ray.roughness = 0.0;
	ray.distance = gl_HitTEXT;
}

###########################################
#shader vert

#include "v4d/modules/V4D_hybrid/glsl_includes/pl_visibility_raster.glsl"

layout(location = 0) out V2F v2f;
layout(location = 3) out flat uint customData;

void main() {
	GeometryInstance geometry = GetGeometryInstance(geometryIndex);
	Vertex vert = GetVertex();
	
	v2f.color = vert.color;
	v2f.pos = (geometry.modelViewTransform * vec4(vert.pos, 1));
	v2f.normal = geometry.normalViewTransform * vert.normal;
	customData = vert.customData;
	
	// Calculate actual distance from camera
	v2f.pos.w = clamp(distance(vec3(camera.worldPosition), (mat4(geometry.modelTransform) * vec4(vert.pos,1)).xyz), float(camera.znear), float(camera.zfar));
	
	gl_Position = mat4(camera.projectionMatrix) * vec4(v2f.pos.xyz, 1);
}

###########################################
#shader frag

#include "v4d/modules/V4D_hybrid/glsl_includes/pl_visibility_raster.glsl"

layout(location = 0) in V2F v2f;
layout(location = 3) in flat uint customData;

void main() {
	// GeometryInstance geometryInstance = GetGeometryInstance(geometryIndex);
	
	pbrGBuffers.viewSpacePosition = v2f.pos.xyz;
	pbrGBuffers.viewSpaceNormal = v2f.normal;
	pbrGBuffers.albedo = v2f.color.rgb;
	pbrGBuffers.uv = uintBitsToFloat(customData);
	pbrGBuffers.emit = 0;
	pbrGBuffers.metallic = 0.0;
	pbrGBuffers.roughness = 0.0;
	
	pbrGBuffers.distance = v2f.pos.w;
	WritePbrGBuffers();
	WriteCustomBuffer(objectIndex, /*type8*/0, /*flags32*/0, /*custom32*/0, /*custom32*/0);
}

