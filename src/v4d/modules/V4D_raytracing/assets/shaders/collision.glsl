#define RAY_TRACING
#include "v4d/modules/V4D_raytracing/glsl_includes/set0.glsl"

struct CollisionPayload {
	float hitDistance;
	int32_t ignoreInstanceIndex;
	int32_t acceptInstanceIndex;
};


#############################################################
#shader rgen

#include "v4d/modules/V4D_raytracing/glsl_includes/set1_collision.glsl"

layout(location = RAY_PAYLOAD_LOCATION_COLLISION) rayPayloadEXT CollisionPayload ray;

void main() {
	const int maxRaysToSurface = 5;
	
	Collision collision = collisions[gl_LaunchIDEXT.x];
	uint rayTraceMask = RAY_TRACED_ENTITY_DEFAULT;
	vec3 rayOrigin = collision.startPosition.xyz;
	vec3 rayDirection = collision.velocity.xyz;
	float rayMaxDistance = collision.startPosition.w + GetOptimalBounceStartDistance(length(rayOrigin));
	
	ray.ignoreInstanceIndex = -1;
	ray.acceptInstanceIndex = int(collision.objectInstanceA);
	int rayCountToSurface = 0;
	do {
		// find object surface
		traceRayEXT(topLevelAS, 0, rayTraceMask, RAY_SBT_OFFSET_COLLISION, 0, RAY_MISS_OFFSET_COLLISION, rayOrigin, 0.0, rayDirection, rayMaxDistance, RAY_PAYLOAD_LOCATION_COLLISION);
		if (ray.hitDistance == -1) break;
		rayOrigin += rayDirection * ray.hitDistance;
	} while (++rayCountToSurface < maxRaysToSurface);
	
	// find collision
	rayTraceMask = RAY_TRACED_ENTITY_DEFAULT|RAY_TRACED_ENTITY_TERRAIN;
	rayOrigin -= rayDirection * GetOptimalBounceStartDistance(length(rayOrigin));
	rayMaxDistance = collision.velocity.w + GetOptimalBounceStartDistance(length(rayOrigin))*2;
	ray.ignoreInstanceIndex = int(collision.objectInstanceA);
	ray.acceptInstanceIndex = -1;
	traceRayEXT(topLevelAS, 0, rayTraceMask, RAY_SBT_OFFSET_COLLISION, 0, RAY_MISS_OFFSET_COLLISION, rayOrigin, 0.0, rayDirection, rayMaxDistance, RAY_PAYLOAD_LOCATION_COLLISION);
	if (ray.hitDistance > -1) {
		collisions[gl_LaunchIDEXT.x].objectGeometryA = 0;
		collisions[gl_LaunchIDEXT.x].objectInstanceB = 0;
		collisions[gl_LaunchIDEXT.x].objectGeometryB = 0;
		collisions[gl_LaunchIDEXT.x].contactA = vec4(0);
		collisions[gl_LaunchIDEXT.x].contactB = vec4(0);
		collisions[gl_LaunchIDEXT.x].startPosition = vec4(0);
	} else if (length(camera.gravityVector) > 0) {
		// test if below terrain
		rayTraceMask = RAY_TRACED_ENTITY_TERRAIN;
		rayDirection = normalize(-camera.gravityVector);
		rayMaxDistance = 100000;
		ray.ignoreInstanceIndex = -1;
		ray.acceptInstanceIndex = -1;
		traceRayEXT(topLevelAS, 0, rayTraceMask, RAY_SBT_OFFSET_COLLISION, 0, RAY_MISS_OFFSET_COLLISION, rayOrigin, 0.0, rayDirection, rayMaxDistance, RAY_PAYLOAD_LOCATION_COLLISION);
		if (ray.hitDistance > -1) {
			collisions[gl_LaunchIDEXT.x].objectGeometryA = 0;
			collisions[gl_LaunchIDEXT.x].objectInstanceB = 0;
			collisions[gl_LaunchIDEXT.x].objectGeometryB = 0;
			collisions[gl_LaunchIDEXT.x].contactA = vec4(0);
			collisions[gl_LaunchIDEXT.x].contactB = vec4(0);
			collisions[gl_LaunchIDEXT.x].startPosition = vec4(rayDirection, ray.hitDistance);
		}
	}
}


#############################################################
#shader rmiss

layout(location = RAY_PAYLOAD_LOCATION_COLLISION) rayPayloadInEXT CollisionPayload ray;

void main() {
	ray.hitDistance = -1;
}


#############################################################
#shader rahit

layout(location = RAY_PAYLOAD_LOCATION_COLLISION) rayPayloadInEXT CollisionPayload ray;

void main() {
	if (ray.ignoreInstanceIndex == gl_InstanceCustomIndexEXT || (ray.acceptInstanceIndex != -1 && ray.acceptInstanceIndex != gl_InstanceCustomIndexEXT)) {
		ignoreIntersectionEXT;
	}
}


#############################################################
#shader rchit

layout(location = RAY_PAYLOAD_LOCATION_COLLISION) rayPayloadInEXT CollisionPayload ray;

void main() {
	ray.hitDistance = gl_HitTEXT;
}

