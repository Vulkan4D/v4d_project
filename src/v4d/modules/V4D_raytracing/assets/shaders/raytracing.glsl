#define RAY_TRACING
#include "v4d/modules/V4D_raytracing/glsl_includes/set0.glsl"

layout(set = 1, binding = 0, rgba16f) uniform image2D img_lit;
layout(set = 1, binding = 1, r32f) uniform image2D img_depth;


// 64 bytes
struct RayTracingPayload {
	vec3 albedo;
	vec3 normal;
	vec3 emission;
	vec3 position;
	float refractionIndex;
	float metallic;
	float roughness;
	float distance;
};


#############################################################
#shader rgen

layout(location = 0) rayPayloadEXT RayTracingPayload ray;

void main() {
	const ivec2 imgCoords = ivec2(gl_LaunchIDEXT.xy);
	const vec2 pixelCenter = vec2(gl_LaunchIDEXT.xy) + vec2(0.5);
	const vec2 d = pixelCenter/vec2(gl_LaunchSizeEXT.xy) * 2.0 - 1.0;
	const vec3 origin = vec3(0);
	const vec3 direction = normalize(vec4(inverse(camera.projectionMatrix) * dvec4(d.x, d.y, 1, 1)).xyz);
	vec3 color = vec3(0);


	// Trace Visibility Rays for Opaque geometry
	traceRayEXT(topLevelAS, gl_RayFlagsOpaqueEXT, 0xff, 0, 0, 0, origin, float(camera.znear), direction, float(camera.zfar), 0);
	
	color = ray.albedo;
	
	
	float depth = clamp(GetFragDepthFromViewSpacePosition(ray.position), 0, 1);
	imageStore(img_depth, imgCoords, vec4(depth, 0,0,0));
	imageStore(img_lit, imgCoords, vec4(color,1));
}


#############################################################
#shader rmiss

layout(location = 0) rayPayloadInEXT RayTracingPayload ray;

void main() {
	ray.albedo = vec3(0);
	ray.distance = 0;
}


#############################################################
#shader shadow.rmiss

layout(location = 1) rayPayloadInEXT bool shadowed;

void main() {
	shadowed = false;
}


#############################################################
#shader rchit

hitAttributeEXT vec3 hitAttribs;

layout(location = 0) rayPayloadInEXT RayTracingPayload ray;

void main() {
	// RenderableEntityInstance instance = renderableEntityInstances[gl_InstanceCustomIndexEXT];
	
	uint i0 = GetIndex(0);
	uint i1 = GetIndex(1);
	uint i2 = GetIndex(2);
	
	// vec3 v0_pos = GetVertexPosition(i0);
	// vec3 v1_pos = GetVertexPosition(i1);
	// vec3 v2_pos = GetVertexPosition(i2);
	
	vec3 v0_normal = GetVertexNormal(i0);
	vec3 v1_normal = GetVertexNormal(i1);
	vec3 v2_normal = GetVertexNormal(i2);
	
	vec4 v0_color = GetVertexColor(i0);
	vec4 v1_color = GetVertexColor(i1);
	vec4 v2_color = GetVertexColor(i2);
	
	// vec2 v0_uv = GetVertexUV(i0);
	// vec2 v1_uv = GetVertexUV(i1);
	// vec2 v2_uv = GetVertexUV(i2);
	
	// Interpolate
	vec3 barycentricCoords = vec3(1.0f - hitAttribs.x - hitAttribs.y, hitAttribs.x, hitAttribs.y);
	// vec3 pos = (v0_pos * barycentricCoords.x + v1_pos * barycentricCoords.y + v2_pos * barycentricCoords.z);
	vec3 normal = normalize(v0_normal * barycentricCoords.x + v1_normal * barycentricCoords.y + v2_normal * barycentricCoords.z);
	vec4 color = (v0_color * barycentricCoords.x + v1_color * barycentricCoords.y + v2_color * barycentricCoords.z);
	// vec2 uv = (v0_uv * barycentricCoords.x + v1_uv * barycentricCoords.y + v2_uv * barycentricCoords.z);
	
	ray.albedo = color.rgb;
	ray.normal = normalize(GetModelTransform().normalView * normal);
	ray.emission = vec3(0);
	ray.position = gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * gl_HitTEXT;
	ray.refractionIndex = 0.0;
	ray.metallic = 0.0;
	ray.roughness = 0.0;
	ray.distance = gl_HitTEXT;
}

