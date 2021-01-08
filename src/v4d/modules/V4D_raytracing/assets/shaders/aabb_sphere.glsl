#define RAY_TRACING
#include "v4d/modules/V4D_raytracing/glsl_includes/set0.glsl"


#############################################################
#common .*rint

hitAttributeEXT vec4 sphereAttr; // center position and thickness

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
			sphereAttr = vec4(spherePosition, abs(t2 - t1));
			reportIntersectionEXT((tMin <= t1 && t1 < tMax) ? t1 : t2, 0);
		}
	}
}

#shader rint
#shader light.rint

#############################################################
#shader rendering.rchit

hitAttributeEXT vec4 sphereAttr;

layout(location = 0) rayPayloadInEXT RenderingPayload ray;

void main() {
	vec3 hitPoint = gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * gl_HitTEXT;
	
	vec4 color = HasVertexColor()? GetVertexColor(gl_PrimitiveID) : vec4(0,0,0,1);
	
	WriteRayPayload(ray);
	ray.color = color;
	float metallic = 0.8;
	float roughness = 0.1;
	vec3 normal = DoubleSidedNormals(normalize(hitPoint - sphereAttr.xyz));
	if (metallic > 0) {
		ray.color.a = 1.0 - FresnelReflectAmount(1.0, GetGeometry().material.indexOfRefraction, normal, gl_WorldRayDirectionEXT, metallic);
		ScatterMetallic(ray, roughness, gl_WorldRayDirectionEXT, normal);
	} else if (color.a < 1) {
		ScatterDieletric(ray, GetGeometry().material.indexOfRefraction, gl_WorldRayDirectionEXT, normal);
	} else {
		ScatterLambertian(ray, roughness, normal);
	}
	
	DebugRay(ray, color.rgb, normal, GetGeometry().material.emission, metallic, roughness);
}


#############################################################
#shader light.rendering.rchit

hitAttributeEXT vec4 sphereAttr;

layout(location = 0) rayPayloadInEXT RenderingPayload ray;

void main() {
	WriteRayPayload(ray);
	ray.color = HasVertexColor()? GetVertexColor(gl_PrimitiveID) /* GetGeometry().material.emission*/ : vec4(0);
	
	DebugRay(ray, ray.color.rgb, vec3(0), GetGeometry().material.emission, 0, 0);
}
