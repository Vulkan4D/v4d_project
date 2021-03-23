#define RAY_TRACING
#include "v4d/modules/V4D_raytracing/glsl_includes/set0.glsl"

struct RayPayloadTerrainNegateSphereExtra {
	vec3 normal; // local space
	float t1;
	float t2;
	float radius;
	uint entityInstanceIndex;
	uint geometryIndex;
	bool hit;
};

struct SphereAttr {
	vec3 insideFaceNormal; // local space
	float radius;
	float t2;
};

#############################################################
#shader rint

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
		vec3 endPoint = origin + direction * t2;
		
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
#shader visibility.rchit

#define DISABLE_SHADOWS
#include "v4d/modules/V4D_raytracing/glsl_includes/core_pbr.glsl"

hitAttributeEXT SphereAttr sphereAttr;

layout(location = RAY_PAYLOAD_LOCATION_VISIBILITY) rayPayloadInEXT VisibilityPayload ray;
layout(location = RAY_PAYLOAD_LOCATION_EXTRA) rayPayloadEXT RayPayloadTerrainNegateSphereExtra rayExtra;

void main() {
	vec3 hitPointViewSpace = gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * gl_HitTEXT;
	vec3 endPointViewSpace = gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * sphereAttr.t2;
	
	float totalRayDistance = ray.normal.w;
	
	// Find all touching negate spheres along that ray and get the maximum distance up to the last sphere's furthest point
	rayExtra.normal = sphereAttr.insideFaceNormal;
	rayExtra.t1 = gl_HitTEXT;
	rayExtra.t2 = sphereAttr.t2;
	rayExtra.radius = sphereAttr.radius;
	rayExtra.entityInstanceIndex = gl_InstanceCustomIndexEXT;
	rayExtra.geometryIndex = gl_GeometryIndexEXT;
	int loop = 0;
	for (;;) {
		rayExtra.hit = false;
		traceRayEXT(topLevelAS, 0, RAY_TRACED_ENTITY_TERRAIN_NEGATE, RAY_SBT_OFFSET_EXTRA, 0, RAY_MISS_OFFSET_VOID, gl_WorldRayOriginEXT, rayExtra.t1 + GetOptimalBounceStartDistance(totalRayDistance + rayExtra.t1), gl_WorldRayDirectionEXT, rayExtra.t2, RAY_PAYLOAD_LOCATION_EXTRA);
		if (!rayExtra.hit) break;
		if (loop++>100) {
			break;
		}
	}
	vec3 normal = rayExtra.normal;
	float hitDepthTotal = rayExtra.t2;
	float sphereRadius = rayExtra.radius;
	vec3 endPointLocalSpace = gl_ObjectRayOriginEXT + gl_ObjectRayDirectionEXT * hitDepthTotal;
	endPointViewSpace = gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * hitDepthTotal;
	totalRayDistance += hitDepthTotal;
	// we now have the total distance to the last sphere's furthest point stored in hitDepthTotal and the last hit sphere's normal at that position
	
	// Find other geometries within that distance
	uint traceMask = RAY_TRACED_ENTITY_DEFAULT|RAY_TRACED_ENTITY_LIGHT;
	if (ray.bounceMask > 0) {
		traceMask &= ray.bounceMask;
	}
	traceRayEXT(topLevelAS, 0, traceMask, RAY_SBT_OFFSET_VISIBILITY, 0, RAY_MISS_OFFSET_STANDARD, gl_WorldRayOriginEXT, gl_HitTEXT + GetOptimalBounceStartDistance(totalRayDistance), gl_WorldRayDirectionEXT, hitDepthTotal, RAY_PAYLOAD_LOCATION_VISIBILITY);
	if (ray.position.w != 0) return; // we found a geometry, just exit here to draw it on screen.

	// Find out if we are above or below ground level by tracing a terrain-seeking ray upwards
	traceRayEXT(topLevelAS, gl_RayFlagsTerminateOnFirstHitEXT, RAY_TRACED_ENTITY_TERRAIN, RAY_SBT_OFFSET_VISIBILITY, 0, RAY_MISS_OFFSET_STANDARD, endPointViewSpace, GetOptimalBounceStartDistance(totalRayDistance), normalize(-camera.gravityVector), 20000000, RAY_PAYLOAD_LOCATION_VISIBILITY);
	if (ray.position.w == 0) {
		// we are above ground, continue with a new ray starting at the total depth of the spheres
		traceMask = RAY_TRACE_MASK_VISIBLE;
		if (ray.bounceMask > 0) {
			traceMask &= ray.bounceMask;
		}
		traceRayEXT(topLevelAS, 0, traceMask, RAY_SBT_OFFSET_VISIBILITY, 0, RAY_MISS_OFFSET_STANDARD, gl_WorldRayOriginEXT, hitDepthTotal + GetOptimalBounceStartDistance(totalRayDistance), gl_WorldRayDirectionEXT, float(camera.zfar), RAY_PAYLOAD_LOCATION_VISIBILITY);
		return;
	}
	
	// we are below ground level, draw the undergrounds
	
	float undergroundDepth = ray.position.w;
	uint terrainChunkId = ray.entityInstanceIndex;
	vec4 terrainChunkColor = ray.color;
	WriteRayPayload(ray);
	ray.entityInstanceIndex = int(rayExtra.entityInstanceIndex);
	ray.geometryIndex = int(rayExtra.geometryIndex);
	ray.position.xyz = endPointLocalSpace;
	ray.position.w = hitDepthTotal;
	ray.color = terrainChunkColor;
	ray.rayDirection.xyz = inverse(GetModelNormalViewMatrix(ray.entityInstanceIndex, ray.geometryIndex)) * gl_WorldRayDirectionEXT;
	ray.uv = vec2(undergroundDepth, sphereRadius);
	ray.normal.xyz = normal;
	ray.extra = terrainChunkId;
}


#############################################################
#shader visibility.rahit

// hitAttributeEXT SphereAttr sphereAttr;

layout(location = RAY_PAYLOAD_LOCATION_VISIBILITY) rayPayloadInEXT VisibilityPayload ray;

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
	rayExtra.geometryIndex = gl_GeometryIndexEXT;
	rayExtra.hit = true;
}


#############################################################
#shader extra.rahit

hitAttributeEXT SphereAttr sphereAttr;

layout(location = RAY_PAYLOAD_LOCATION_EXTRA) rayPayloadInEXT RayPayloadTerrainNegateSphereExtra rayExtra;

void main() {
	if (rayExtra.entityInstanceIndex == gl_InstanceCustomIndexEXT && rayExtra.geometryIndex == gl_GeometryIndexEXT) {
		ignoreIntersectionEXT;
	}
	if (gl_HitKindEXT == 1 && sphereAttr.t2 < rayExtra.t2) {
		ignoreIntersectionEXT;
	}
}
