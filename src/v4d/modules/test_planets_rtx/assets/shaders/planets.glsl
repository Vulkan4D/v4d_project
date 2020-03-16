#include "rtx_base.glsl"

#############################################################
#shader rint

hitAttributeNV ProceduralGeometry sphereGeomAttr;

void main() {
	ProceduralGeometry geom = GetProceduralGeometry(gl_InstanceCustomIndexNV);
	vec3 spherePosition = geom.objectInstance.position;
	float sphereRadius = geom.aabbMax.x;
	
	const vec3 origin = gl_WorldRayOriginNV;
	const vec3 direction = gl_WorldRayDirectionNV;
	const float tMin = gl_RayTminNV;
	const float tMax = gl_RayTmaxNV;

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
			sphereGeomAttr = geom;
			reportIntersectionNV((tMin <= t1 && t1 < tMax) ? t1 : t2, 0);
		}
	}
}

#############################################################
#shader rchit

hitAttributeNV ProceduralGeometry sphereGeomAttr;

layout(location = 0) rayPayloadInNV RayPayload ray;
layout(location = 2) rayPayloadNV bool shadowed;

#include "rtx_pbr.glsl"

void main() {
	vec3 spherePosition = sphereGeomAttr.objectInstance.position;
	float sphereRadius = sphereGeomAttr.aabbMax.x;
	
	// Hit World Position
	const vec3 hitPoint = gl_WorldRayOriginNV + gl_WorldRayDirectionNV * gl_HitTNV;
	const vec3 normal = normalize(hitPoint - spherePosition);
	
	vec3 color = ApplyPBRShading(hitPoint, sphereGeomAttr.color.rgb, normal, /*roughness*/0.5, /*metallic*/0.0);
	
	ray.color = color;
	ray.distance = gl_HitTNV;
}

