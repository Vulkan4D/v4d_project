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

layout(location = 0) rayPayloadInEXT RayTracingPayload ray;

void main() {
	vec3 hitPoint = gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * gl_HitTEXT;
	
	vec4 color = HasVertexColor()? GetVertexColor(gl_PrimitiveID) : vec4(0,0,0,1);
	
	// Material
	Material material = GetGeometry().material;
	
	WriteRayPayload(ray);
	ray.albedo = color.rgb;
	ray.opacity = color.a;
	ray.indexOfRefraction = float(material.indexOfRefraction) / 50.0;
	ray.normal = DoubleSidedNormals(normalize(hitPoint - sphereAttr.xyz));
	ray.metallic = 0.8;
	ray.roughness = 0.1;
}


#############################################################
#shader light.rendering.rchit

hitAttributeEXT vec4 sphereAttr;

layout(location = 0) rayPayloadInEXT RayTracingPayload ray;

void main() {
	vec3 hitPoint = gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * gl_HitTEXT;
	
	WriteRayPayload(ray);
	ray.albedo = vec3(0);
	ray.normal = normalize(hitPoint - sphereAttr.xyz);
	ray.emission = HasVertexColor()? GetVertexColor(gl_PrimitiveID).rgb : vec3(0);
	ray.metallic = 0.0;
	ray.roughness = 0.0;
}
