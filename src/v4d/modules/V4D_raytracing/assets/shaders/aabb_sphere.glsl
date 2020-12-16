#define RAY_TRACING
#include "v4d/modules/V4D_raytracing/glsl_includes/set0.glsl"


#############################################################
#shader rint

hitAttributeEXT vec3 sphereGeomPositionAttr;

void main() {
	vec3 aabb_min = GetProceduralVertexAABB_min(gl_PrimitiveID);
	vec3 aabb_max = GetProceduralVertexAABB_max(gl_PrimitiveID);
	vec3 spherePosition = (GetModelViewMatrix() * vec4((aabb_max + aabb_min)/2, 1)).xyz;
	float sphereRadius = aabb_max.x;
	
	const vec3 origin = gl_WorldRayOriginEXT;
	const vec3 direction = gl_WorldRayDirectionEXT;
	const float tMin = gl_RayTminEXT;
	const float tMax = gl_RayTmaxEXT;

	const vec3 oc = origin - spherePosition;
	const float a = dot(direction, direction);
	const float b = dot(oc, direction);
	const float c = dot(oc, oc) - sphereRadius*sphereRadius;
	const float discriminant = b * b - a * c;

	if (discriminant >= 0) {
		const float discriminantSqrt = sqrt(discriminant);
		const float t1 = (-b - discriminantSqrt) / a;
		const float t2 = (-b + discriminantSqrt) / a;

		if ((tMin <= t1 && t1 < tMax) || (tMin <= t2 && t2 < tMax)) {
			sphereGeomPositionAttr = spherePosition;
			reportIntersectionEXT((tMin <= t1 && t1 < tMax) ? t1 : t2, 0);
		}
	}
}


#############################################################
#shader rchit

hitAttributeEXT vec3 sphereGeomPositionAttr;

layout(location = 0) rayPayloadInEXT RayTracingPayload ray;

void main() {
	vec3 hitPoint = gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * gl_HitTEXT;
	
	WriteRayPayload(ray);
	ray.albedo = HasVertexColor()? GetVertexColor(gl_PrimitiveID).rgb : vec3(0);
	ray.normal = DoubleSidedNormals(normalize(hitPoint - sphereGeomPositionAttr));
	// ray.refractionIndex = 1.5;
	// ray.nextRayStartOffset = 1.0;
	ray.metallic = 0.8;
	ray.roughness = 0.1;
}


#############################################################
#shader light.rchit

hitAttributeEXT vec3 sphereGeomPositionAttr;

layout(location = 0) rayPayloadInEXT RayTracingPayload ray;

void main() {
	vec3 hitPoint = gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * gl_HitTEXT;
	
	WriteRayPayload(ray);
	ray.albedo = vec3(0);
	ray.normal = normalize(hitPoint - sphereGeomPositionAttr);
	ray.emission = HasVertexColor()? GetVertexColor(gl_PrimitiveID).rgb : vec3(0);
	ray.metallic = 0.0;
	ray.roughness = 0.0;
}
