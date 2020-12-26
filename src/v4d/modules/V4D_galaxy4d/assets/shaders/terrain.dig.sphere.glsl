#define RAY_TRACING
#include "v4d/modules/V4D_raytracing/glsl_includes/set0.glsl"

struct RayPayloadTerrainNegateSphere1 {
	vec3 normal;
	float t1;
	float t2;
	bool hit;
};

#############################################################
#shader rint

hitAttributeEXT vec4 sphereAttr; // inside normal and t2

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
		vec3 endPoint = gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * t2;
		sphereAttr = vec4(normalize(spherePosition - endPoint), t2);
		
		// Outside of sphere
		if (tMin <= t1 && t1 < tMax) {
			reportIntersectionEXT(t1, 0);
		}
		
		// Inside of sphere
		if (t1 <= tMin && t2 >= tMin) {
			reportIntersectionEXT(tMin, 1);
		}
	}
}


#############################################################
#shader rchit

hitAttributeEXT vec4 sphereAttr;

layout(location = 0) rayPayloadInEXT RayTracingPayload ray;
layout(location = 1) rayPayloadEXT RayPayloadTerrainNegateSphere1 ray1;

void main() {
	vec3 hitPoint = gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * gl_HitTEXT;
	vec3 endPoint = gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * sphereAttr.w;
	
	// Find all touching negate spheres along that ray and get the maximum distance up to the last sphere's furthest point
	ray1.normal = sphereAttr.xyz;
	ray1.t1 = 0;
	ray1.t2 = sphereAttr.w - gl_HitTEXT;
	int loop = 0;
	for (;;) {
		ray1.hit = false;
		traceRayEXT(topLevelAS, 0, RAY_TRACED_ENTITY_TERRAIN_NEGATE, 1, 0, 2, hitPoint, ray1.t1 + float(camera.znear), gl_WorldRayDirectionEXT, ray1.t2, 1);
		if (!ray1.hit) break;
		if (loop++>20) {
			WriteRayPayload(ray);
			ray.albedo += vec3(1,0,0);
			ray.normal = vec3(0);
			ray.metallic = 0;
			ray.roughness = 0;
			return;
		}
	}
	vec3 normal = ray1.normal;
	float hitDepthTotal = ray1.t2;
	endPoint = gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * (gl_HitTEXT + hitDepthTotal);
	// we now have the total distance to the last sphere's furthest point stored in hitDepthTotal and the last hit sphere's normal at that position
	
	// Find other geometries within that distance
	int traceMask = RAY_TRACED_ENTITY_DEFAULT|RAY_TRACED_ENTITY_LIGHT;
	traceRayEXT(topLevelAS, 0, traceMask, 0, 0, 0, hitPoint, float(camera.znear), gl_WorldRayDirectionEXT, hitDepthTotal, 0);
	if (ray.distance != 0) return; // we found a geometry, just exit here to draw it on screen.

	// Find out if we are above or below ground level by tracing a terrain-seeking ray upwards
	traceRayEXT(topLevelAS, gl_RayFlagsTerminateOnFirstHitEXT, RAY_TRACED_ENTITY_TERRAIN, 0, 0, 0, endPoint, float(camera.znear), normalize(-camera.gravityVector), 10000, 0);
	if (ray.distance == 0) {
		// we are above ground, trace a standard ray and exit here
		if (ray.recursions++ > 30) {
			WriteRayPayload(ray);
			ray.albedo += vec3(0,0,1);
			ray.normal = vec3(0);
			ray.metallic = 0;
			ray.roughness = 0;
			return;
		}
		ray.entityInstanceIndex = gl_InstanceCustomIndexEXT;
		// traceRayEXT(topLevelAS, 0, RAY_TRACE_MASK_VISIBLE, 0, 0, 0, endPoint, float(camera.znear), gl_WorldRayDirectionEXT, float(camera.zfar), 0);
		ray.passthrough = true;
		ray.distance = gl_HitTEXT + hitDepthTotal;
		return;
	}
	
	// we are below ground level, draw the undergrounds
	
	float undergroundDepth = ray.distance;
	
	ray.albedo *= vec3(0.1/undergroundDepth);
	// ray.metallic = 0;
	// ray.roughness = 0;
	
	ray.normal = normalize(mix(ray.normal, normal, 0.5));
	ray.distance = gl_HitTEXT + hitDepthTotal;
}


#############################################################
#shader 1.rchit

hitAttributeEXT vec4 sphereAttr;

layout(location = 1) rayPayloadInEXT RayPayloadTerrainNegateSphere1 ray1;

void main() {
	ray1.normal = sphereAttr.xyz;
	ray1.t1 = gl_HitTEXT;
	ray1.t2 = sphereAttr.w;
	ray1.hit = true;
}


#############################################################
#shader rahit

// hitAttributeEXT vec4 sphereAttr;

layout(location = 0) rayPayloadInEXT RayTracingPayload ray;

void main() {
	if (ray.entityInstanceIndex == gl_InstanceCustomIndexEXT) {
		ignoreIntersectionEXT;
	}
}


#############################################################
#shader 1.rahit

// hitAttributeEXT vec4 sphereAttr;

void main() {
	if (gl_HitKindEXT == 1) {
		ignoreIntersectionEXT;
	}
}
