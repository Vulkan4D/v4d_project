#define RAY_TRACING
#include "v4d/modules/V4D_raytracing/glsl_includes/set0.glsl"


#############################################################
#shader rchit

hitAttributeEXT vec3 hitAttribs;

layout(location = 0) rayPayloadInEXT RayTracingPayload ray;

void main() {
	vec3 hitPoint = gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * gl_HitTEXT;
	
	uint i0 = GetIndex(0);
	uint i1 = GetIndex(1);
	uint i2 = GetIndex(2);
	
	// Interpolate fragment
	vec3 barycentricCoords = vec3(1.0f - hitAttribs.x - hitAttribs.y, hitAttribs.x, hitAttribs.y);
	// vec3 pos = (
	// 	+ GetVertexPosition(i0) * barycentricCoords.x
	// 	+ GetVertexPosition(i1) * barycentricCoords.y
	// 	+ GetVertexPosition(i2) * barycentricCoords.z
	// );
	vec3 normal = normalize(
		+ GetVertexNormal(i0) * barycentricCoords.x
		+ GetVertexNormal(i1) * barycentricCoords.y
		+ GetVertexNormal(i2) * barycentricCoords.z
	);
	vec4 color = HasVertexColor()? (
		+ GetVertexColor(i0) * barycentricCoords.x
		+ GetVertexColor(i1) * barycentricCoords.y
		+ GetVertexColor(i2) * barycentricCoords.z
	) : vec4(0);
	// vec2 uv = HasVertexUV()? (
	// 	+ GetVertexUV(i0) * barycentricCoords.x
	// 	+ GetVertexUV(i1) * barycentricCoords.y
	// 	+ GetVertexUV(i2) * barycentricCoords.z
	// ) : vec2(0);
	
	ray.albedo = color.rgb;
	ray.normal = DoubleSidedNormals(normalize(GetModelNormalViewMatrix() * normal));
	ray.emission = vec3(0);
	ray.position = hitPoint;
	ray.refractionIndex = 0.0;
	ray.metallic = 0.5;
	ray.roughness = 0.1;
	ray.distance = gl_HitTEXT;
	ray.instanceCustomIndex = gl_InstanceCustomIndexEXT;
	ray.primitiveID = gl_PrimitiveID;
	ray.raycastCustomData = GetCustomData();
}
