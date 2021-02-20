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
	Collision collision = collisions[gl_LaunchIDEXT.x];
	uint rayTraceMask = RAY_TRACED_ENTITY_DEFAULT;
	vec3 rayOrigin = collision.startPosition.xyz;
	vec3 rayDirection = collision.velocity.xyz;
	float rayMaxDistance = collision.startPosition.w;
	
	ray.ignoreInstanceIndex = -1;
	ray.acceptInstanceIndex = int(collision.objectInstanceA);
	do {
		traceRayEXT(topLevelAS, 0, rayTraceMask, RAY_SBT_OFFSET_COLLISION, 0, RAY_MISS_OFFSET_COLLISION, rayOrigin, GetOptimalBounceStartDistance(length(rayOrigin)), rayDirection, rayMaxDistance, RAY_PAYLOAD_LOCATION_COLLISION);
		rayOrigin += rayDirection * ray.hitDistance;
	} while (ray.hitDistance != 0);
	
	rayTraceMask = RAY_TRACED_ENTITY_DEFAULT|RAY_TRACED_ENTITY_TERRAIN;
	rayOrigin -= rayDirection * (collision.velocity.w + GetOptimalBounceStartDistance(length(rayOrigin)));
	rayMaxDistance = (collision.velocity.w + GetOptimalBounceStartDistance(length(rayOrigin)))*2.0;
	ray.ignoreInstanceIndex = int(collision.objectInstanceA);
	ray.acceptInstanceIndex = -1;
	traceRayEXT(topLevelAS, 0, rayTraceMask, RAY_SBT_OFFSET_COLLISION, 0, RAY_MISS_OFFSET_COLLISION, rayOrigin, 0.0, rayDirection, rayMaxDistance, RAY_PAYLOAD_LOCATION_COLLISION);
	if (ray.hitDistance != 0) {
		collisions[gl_LaunchIDEXT.x].objectGeometryA = 1;
		collisions[gl_LaunchIDEXT.x].objectInstanceB = 1;
		collisions[gl_LaunchIDEXT.x].objectGeometryB = 1;
		collisions[gl_LaunchIDEXT.x].contactA = vec4(0);
		collisions[gl_LaunchIDEXT.x].contactB = vec4(0);
	}
}


#############################################################
#shader rmiss

layout(location = RAY_PAYLOAD_LOCATION_COLLISION) rayPayloadInEXT CollisionPayload ray;

void main() {
	ray.hitDistance = 0;
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

