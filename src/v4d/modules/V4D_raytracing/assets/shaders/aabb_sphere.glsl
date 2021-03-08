#define RAY_TRACING
#include "v4d/modules/V4D_raytracing/glsl_includes/set0.glsl"


struct SphereAttr {
	float t1;
	float t2;
	float radius;
};

#############################################################
#common .*rint

hitAttributeEXT SphereAttr sphereAttr;

void main() {
	// Ray-Sphere Intersection
	const vec3 aabb_min = GetProceduralVertexAABB_min(gl_PrimitiveID);
	const vec3 aabb_max = GetProceduralVertexAABB_max(gl_PrimitiveID);
	const vec3 spherePosition = (aabb_max + aabb_min) / 2;
	const float sphereRadius = (aabb_max.x - aabb_min.x) / 2;
	const vec3 oc = gl_ObjectRayOriginEXT - spherePosition;
	const float a = dot(gl_ObjectRayDirectionEXT, gl_ObjectRayDirectionEXT);
	const float b = dot(oc, gl_ObjectRayDirectionEXT);
	const float c = dot(oc, oc) - sphereRadius*sphereRadius;
	const float discriminant = b * b - a * c;
	
	// If we hit the sphere
	if (discriminant >= 0) {
		const float discriminantSqrt = sqrt(discriminant);
		
		sphereAttr.t1 = (-b - discriminantSqrt) / a;
		sphereAttr.t2 = (-b + discriminantSqrt) / a;
		sphereAttr.radius = sphereRadius;
		
		// Outside of sphere
		if (gl_RayTminEXT <= sphereAttr.t1 && sphereAttr.t1 < gl_RayTmaxEXT) {
			reportIntersectionEXT(sphereAttr.t1, 0);
		}
		// Inside of sphere
		else if (sphereAttr.t1 <= gl_RayTminEXT && sphereAttr.t2 >= gl_RayTminEXT) {
			reportIntersectionEXT(sphereAttr.t2, 1);
		}
	}
}

#############################################################
#common .*collision.rchit

#include "v4d/modules/V4D_raytracing/glsl_includes/set1_collision.glsl"
layout(location = RAY_PAYLOAD_LOCATION_COLLISION) rayPayloadInEXT CollisionPayload ray;

hitAttributeEXT SphereAttr sphereAttr;

void main() {
	ray.hit = vec4(gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * gl_HitTEXT, gl_HitTEXT);
	ray.entityInstanceIndex = gl_InstanceCustomIndexEXT;
	ray.geometryIndex = gl_GeometryIndexEXT;
	
	// Compute normal
	const vec3 aabb_min = GetProceduralVertexAABB_min(gl_PrimitiveID);
	const vec3 aabb_max = GetProceduralVertexAABB_max(gl_PrimitiveID);
	const vec3 spherePosition = (aabb_max + aabb_min) / 2;
	const vec3 hitPoint1 = gl_ObjectRayOriginEXT + gl_ObjectRayDirectionEXT * sphereAttr.t1;
	const vec3 hitPoint2 = gl_ObjectRayOriginEXT + gl_ObjectRayDirectionEXT * sphereAttr.t2;
	ray.normal.xyz = GetModelNormalViewMatrix() * normalize(hitPoint1 - spherePosition); // always use outside normal
}

#############################################################
#shader rint
#shader light.rint

#############################################################
#shader visibility.rchit

hitAttributeEXT SphereAttr sphereAttr;

layout(location = RAY_PAYLOAD_LOCATION_VISIBILITY) rayPayloadInEXT VisibilityPayload ray;

void main() {
	WriteRayPayload(ray);
	
	// Vertex Colors
	if (HasVertexColor()) {
		ray.color = GetVertexColor(gl_PrimitiveID);
	} else {
		ray.color = vec4(1);
	}
	
	// Compute normal
	const vec3 aabb_min = GetProceduralVertexAABB_min(gl_PrimitiveID);
	const vec3 aabb_max = GetProceduralVertexAABB_max(gl_PrimitiveID);
	const vec3 spherePosition = (aabb_max + aabb_min) / 2;
	const vec3 hitPoint1 = gl_ObjectRayOriginEXT + gl_ObjectRayDirectionEXT * sphereAttr.t1;
	const vec3 hitPoint2 = gl_ObjectRayOriginEXT + gl_ObjectRayDirectionEXT * sphereAttr.t2;
	if (gl_HitKindEXT == 0) {
		// Outside of sphere
		ray.normal.xyz = normalize(hitPoint1 - spherePosition);
	}
	else if (gl_HitKindEXT == 1) {
		// Inside of sphere
		ray.normal.xyz = normalize(spherePosition - hitPoint2);
	}
	
	// Store useful information in the UV payload member
	ray.uv = vec2(sphereAttr.radius, (sphereAttr.t2 - sphereAttr.t1) * sign(sphereAttr.t1 - gl_RayTminEXT));
}


#############################################################
#shader light.visibility.rchit

hitAttributeEXT SphereAttr sphereAttr;

layout(location = RAY_PAYLOAD_LOCATION_VISIBILITY) rayPayloadInEXT VisibilityPayload ray;

void main() {
	WriteRayPayload(ray);
	
	// Vertex Colors
	if (HasVertexColor()) {
		ray.color = GetVertexColor(gl_PrimitiveID);
	} else {
		ray.color = vec4(1);
	}
	
	// Compute normal
	const vec3 aabb_min = GetProceduralVertexAABB_min(gl_PrimitiveID);
	const vec3 aabb_max = GetProceduralVertexAABB_max(gl_PrimitiveID);
	const vec3 spherePosition = (aabb_max + aabb_min) / 2;
	const vec3 hitPoint1 = gl_ObjectRayOriginEXT + gl_ObjectRayDirectionEXT * sphereAttr.t1;
	const vec3 hitPoint2 = gl_ObjectRayOriginEXT + gl_ObjectRayDirectionEXT * sphereAttr.t2;
	ray.normal.xyz = normalize(hitPoint1 - spherePosition); // always use outside normal
	
	// Store useful information in the UV payload member
	ray.uv = vec2(sphereAttr.radius, (sphereAttr.t2 - sphereAttr.t1) * sign(sphereAttr.t1 - gl_RayTminEXT));
}


#shader collision.rchit
#shader light.collision.rchit
