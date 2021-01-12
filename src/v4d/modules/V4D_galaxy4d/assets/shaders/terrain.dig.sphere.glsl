#define RAY_TRACING
#include "v4d/modules/V4D_raytracing/glsl_includes/set0.glsl"

struct RayPayloadTerrainNegateSphereExtra {
	vec3 normal;
	float t1;
	float t2;
	float radius;
	uint64_t entityInstanceIndex;
	bool hit;
};

struct SphereAttr {
	vec3 insideFaceNormal; // already in view-space
	float radius;
	float t2;
};

#############################################################
#shader rint

hitAttributeEXT SphereAttr sphereAttr;

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
		sphereAttr.insideFaceNormal = normalize(spherePosition - endPoint);
		sphereAttr.radius = sphereRadius;
		sphereAttr.t2 = t2;
		
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
#shader rendering.rchit

#define DISABLE_SHADOWS
#include "v4d/modules/V4D_raytracing/glsl_includes/core_pbr.glsl"

hitAttributeEXT SphereAttr sphereAttr;

layout(location = RAY_PAYLOAD_LOCATION_RENDERING) rayPayloadInEXT RenderingPayload ray;
layout(location = RAY_PAYLOAD_LOCATION_EXTRA) rayPayloadEXT RayPayloadTerrainNegateSphereExtra rayExtra;

void main() {
	vec3 hitPoint = gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * gl_HitTEXT;
	vec3 endPoint = gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * sphereAttr.t2;
	
	// Find all touching negate spheres along that ray and get the maximum distance up to the last sphere's furthest point
	rayExtra.normal = sphereAttr.insideFaceNormal;
	rayExtra.t1 = 0;
	rayExtra.t2 = sphereAttr.t2 - gl_HitTEXT;
	rayExtra.radius = sphereAttr.radius;
	rayExtra.entityInstanceIndex = gl_InstanceCustomIndexEXT;
	int loop = 0;
	for (;;) {
		rayExtra.hit = false;
		traceRayEXT(topLevelAS, 0, RAY_TRACED_ENTITY_TERRAIN_NEGATE, RAY_SBT_OFFSET_EXTRA, 0, 2, hitPoint, rayExtra.t1 + 0.01, gl_WorldRayDirectionEXT, rayExtra.t2, RAY_PAYLOAD_LOCATION_EXTRA);
		if (!rayExtra.hit) break;
		if (loop++>100) {
			// WriteRayPayload(ray);
			break;
		}
	}
	vec3 normal = rayExtra.normal;
	float hitDepthTotal = rayExtra.t2;
	float sphereRadius = rayExtra.radius;
	endPoint = gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * (gl_HitTEXT + hitDepthTotal);
	// we now have the total distance to the last sphere's furthest point stored in hitDepthTotal and the last hit sphere's normal at that position
	
	// Find other geometries within that distance
	int traceMask = RAY_TRACED_ENTITY_DEFAULT|RAY_TRACED_ENTITY_LIGHT;
	traceRayEXT(topLevelAS, 0, traceMask, RAY_SBT_OFFSET_RENDERING, 0, 0, hitPoint, float(camera.znear), gl_WorldRayDirectionEXT, hitDepthTotal, RAY_PAYLOAD_LOCATION_RENDERING);
	if (ray.position.w != 0) return; // we found a geometry, just exit here to draw it on screen.

	// Find out if we are above or below ground level by tracing a terrain-seeking ray upwards
	traceRayEXT(topLevelAS, gl_RayFlagsTerminateOnFirstHitEXT, RAY_TRACED_ENTITY_TERRAIN, RAY_SBT_OFFSET_RENDERING, 0, 0, endPoint, float(camera.znear), normalize(-camera.gravityVector), 20000000, RAY_PAYLOAD_LOCATION_RENDERING);
	if (ray.position.w == 0) {
		// we are above ground, trace a standard ray and exit here
		// if (ray.recursions++ > 200) {
		// 	// WriteRayPayload(ray);
		// 	return;
		// }
		ray.entityInstanceIndex = gl_InstanceCustomIndexEXT;
		ray.position.w = gl_HitTEXT + hitDepthTotal;
		ray.bounceDirection = vec4(gl_WorldRayDirectionEXT, float(camera.zfar));
		ray.color.rgb = vec3(1);
		ray.color.a = 0;
		return;
	}
	
	// we are below ground level, draw the undergrounds
	
	float undergroundDepth = ray.position.w;
	
	ray.position = vec4(endPoint, gl_HitTEXT + hitDepthTotal);
	ScatterLambertian(ray, 0.7, mix(ray.normal, normal, 0.5));
	ray.specular = 0;
	
	// // Override shading
	// ray.albedo = ApplyPBRShading(gl_WorldRayOriginEXT, ray.position, ray.albedo, ray.normal, /*bump*/vec3(0), ray.roughness, ray.metallic, vec4(0));
	// ray.albedo *= pow(1-clamp(linearStep(0, sphereRadius*3, undergroundDepth), 0, 1), 10);
	// ray.roughness = 0;
	
	
	DebugRay(ray, ray.color.rgb, normal, 0, 0, 0);
}


#############################################################
#shader rendering.rahit

// hitAttributeEXT SphereAttr sphereAttr;

layout(location = RAY_PAYLOAD_LOCATION_RENDERING) rayPayloadInEXT RenderingPayload ray;

void main() {
	if (ray.entityInstanceIndex == gl_InstanceCustomIndexEXT) {
		ignoreIntersectionEXT;
	}
}


#############################################################
#shader extra.rchit

hitAttributeEXT SphereAttr sphereAttr;

layout(location = RAY_PAYLOAD_LOCATION_EXTRA) rayPayloadInEXT RayPayloadTerrainNegateSphereExtra rayExtra;

void main() {
	rayExtra.normal = sphereAttr.insideFaceNormal;
	rayExtra.t1 = gl_HitTEXT;
	rayExtra.t2 = sphereAttr.t2;
	rayExtra.radius = sphereAttr.radius;
	rayExtra.entityInstanceIndex = gl_InstanceCustomIndexEXT;
	rayExtra.hit = true;
}


#############################################################
#shader extra.rahit

hitAttributeEXT SphereAttr sphereAttr;

layout(location = RAY_PAYLOAD_LOCATION_EXTRA) rayPayloadInEXT RayPayloadTerrainNegateSphereExtra rayExtra;

void main() {
	if (rayExtra.entityInstanceIndex == gl_InstanceCustomIndexEXT) {
		ignoreIntersectionEXT;
	}
	if (gl_HitKindEXT == 1 && sphereAttr.t2 < rayExtra.t2) {
		ignoreIntersectionEXT;
	}
}
