#define RAY_TRACING
#include "v4d/modules/V4D_raytracing/glsl_includes/set0.glsl"


struct SphereAttr {
	vec3 normal; // local space
	float radius;
	float t2;
};

#############################################################
#common .*rint

hitAttributeEXT SphereAttr sphereAttr;

void main() {
	const vec3 aabb_min = GetProceduralVertexAABB_min(gl_PrimitiveID);
	const vec3 aabb_max = GetProceduralVertexAABB_max(gl_PrimitiveID);
	const vec3 spherePosition = (aabb_max + aabb_min) / 2;
	const float sphereRadius = (aabb_max.x - aabb_min.x) / 2;
	const vec3 origin = gl_ObjectRayOriginEXT;
	const vec3 direction = gl_ObjectRayDirectionEXT;
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
		vec3 hitPoint = origin + direction * t1;
		
		sphereAttr.normal = normalize(hitPoint - spherePosition);
		sphereAttr.radius = sphereRadius;
		sphereAttr.t2 = t2;
		
		// Outside of sphere
		if (tMin <= t1 && t1 < tMax) {
			reportIntersectionEXT(t1, 0);
		}
		
		// // Inside of sphere
		// if (t1 <= tMin && t2 >= tMin) {
		// 	reportIntersectionEXT(tMin, 1);
		// }
	}
}

#shader rint
#shader light.rint

#############################################################
#shader visibility.rchit

hitAttributeEXT SphereAttr sphereAttr;

layout(location = RAY_PAYLOAD_LOCATION_VISIBILITY) rayPayloadInEXT VisibilityPayload ray;

void main() {
	WriteRayPayload(ray);
	
	if (HasVertexColor()) {
		ray.color = GetVertexColor(gl_PrimitiveID);
	}
	
	ray.normal.xyz = sphereAttr.normal;
	ray.uv = vec2(sphereAttr.radius, sphereAttr.t2);

	// float metallic = 0.8;
	// float roughness = 0.1;
}


#############################################################
#shader light.visibility.rchit

hitAttributeEXT SphereAttr sphereAttr;

layout(location = RAY_PAYLOAD_LOCATION_VISIBILITY) rayPayloadInEXT VisibilityPayload ray;

void main() {
	WriteRayPayload(ray);
	
	if (HasVertexColor()) {
		ray.color = GetVertexColor(gl_PrimitiveID);
	}
	
	// ray.color = vec4(0);
	
	ray.normal.xyz = sphereAttr.normal;
	ray.uv = vec2(sphereAttr.radius, sphereAttr.t2);

}
