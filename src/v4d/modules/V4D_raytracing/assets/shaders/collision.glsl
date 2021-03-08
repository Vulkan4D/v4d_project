#define RAY_TRACING
#include "v4d/modules/V4D_raytracing/glsl_includes/set0.glsl"
#include "v4d/modules/V4D_raytracing/glsl_includes/set1_collision.glsl"

#############################################################
#shader rgen

layout(location = RAY_PAYLOAD_LOCATION_COLLISION) rayPayloadEXT CollisionPayload ray;

void main() {
	Collision collision = collisions[gl_LaunchIDEXT.x];
	// uint collisionFlags = collision.collisionInstance & 0xff;
	uint rayTraceMask = collision.objectInstanceB & 0xff;
	vec3 rayOrigin = collision.position.xyz;
	vec3 rayDirection = collision.direction.xyz;
	float rayMaxDistance = collision.position.w + collision.direction.w;
	uint rayFlags = gl_RayFlagsTerminateOnFirstHitEXT;
	ray.entityInstanceIndex = int(collision.collisionInstance >> 8);
	traceRayEXT(topLevelAS, rayFlags, rayTraceMask, RAY_SBT_OFFSET_COLLISION, 0, RAY_MISS_OFFSET_COLLISION, rayOrigin, 0.0, rayDirection, rayMaxDistance, RAY_PAYLOAD_LOCATION_COLLISION);
	
	if (ray.hit.w > -1) {
		collisions[gl_LaunchIDEXT.x].objectInstanceB |= (ray.entityInstanceIndex << 8);
		collisions[gl_LaunchIDEXT.x].objectGeometryB = ray.geometryIndex;
		collisions[gl_LaunchIDEXT.x].contactB = ray.hit;
		collisions[gl_LaunchIDEXT.x].normalB = vec4(ray.normal, ray.hit.w - collision.position.w);
		//TODO find out objectGeometryA and contactA
		collisions[gl_LaunchIDEXT.x].objectGeometryA = 0;
		collisions[gl_LaunchIDEXT.x].contactA = vec4(rayOrigin + rayDirection * collision.position.w, 0);
	}
}


#############################################################
#shader rmiss

layout(location = RAY_PAYLOAD_LOCATION_COLLISION) rayPayloadInEXT CollisionPayload ray;

void main() {
	ray.hit.w = -1;
}


#############################################################
#shader rahit

layout(location = RAY_PAYLOAD_LOCATION_COLLISION) rayPayloadInEXT CollisionPayload ray;

void main() {
	if (ray.entityInstanceIndex == gl_InstanceCustomIndexEXT) {
		ignoreIntersectionEXT;
	}
}


#############################################################
#shader rchit

layout(location = RAY_PAYLOAD_LOCATION_COLLISION) rayPayloadInEXT CollisionPayload ray;

hitAttributeEXT vec3 hitAttribs;

void main() {
	ray.hit = vec4(gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * gl_HitTEXT, gl_HitTEXT);
	ray.entityInstanceIndex = gl_InstanceCustomIndexEXT;
	ray.geometryIndex = gl_GeometryIndexEXT;
	
	uint i0 = GetIndex(0);
	uint i1 = GetIndex(1);
	uint i2 = GetIndex(2);
	
	vec3 barycentricCoords = vec3(1.0f - hitAttribs.x - hitAttribs.y, hitAttribs.x, hitAttribs.y);
	
	ray.normal.xyz = GetModelNormalViewMatrix() * normalize(
		+ GetVertexNormal(i0) * barycentricCoords.x
		+ GetVertexNormal(i1) * barycentricCoords.y
		+ GetVertexNormal(i2) * barycentricCoords.z
	);
	
}

