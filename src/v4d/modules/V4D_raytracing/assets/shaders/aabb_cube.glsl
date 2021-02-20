#define RAY_TRACING
#include "v4d/modules/V4D_raytracing/glsl_includes/set0.glsl"

#############################################################
#shader rint

void main() {
	// Ray-Box Intersection
	const vec3  aabb_min = GetProceduralVertexAABB_min(gl_PrimitiveID);
	const vec3  aabb_max = GetProceduralVertexAABB_max(gl_PrimitiveID);
	const vec3  invDir = 1.0 / gl_ObjectRayDirectionEXT;
	const vec3  tbot   = invDir * (aabb_min - gl_ObjectRayOriginEXT);
	const vec3  ttop   = invDir * (aabb_max - gl_ObjectRayOriginEXT);
	const vec3  tmin   = min(ttop, tbot);
	const vec3  tmax   = max(ttop, tbot);
	const float t1     = max(tmin.x, max(tmin.y, tmin.z));
	const float t2     = min(tmax.x, min(tmax.y, tmax.z));

	// Outside of box
	if (gl_RayTminEXT <= t1 && t1 < gl_RayTmaxEXT && t2 > t1) {
		reportIntersectionEXT(t1, 0);
	}
	// Inside of box
	else if (t1 <= gl_RayTminEXT && t2 >= gl_RayTminEXT) {
		reportIntersectionEXT(t2, 1);
	}
}


#############################################################
#shader visibility.rchit

layout(location = RAY_PAYLOAD_LOCATION_VISIBILITY) rayPayloadInEXT VisibilityPayload ray;

void main() {
	WriteRayPayload(ray);
	
	// Compute normal for a box (this method works for boxes with arbitrary width/height/depth)
	const float THRESHOLD = 0.000001 * (ray.normal.w+ray.position.w);
	const vec3 aabb_min = GetProceduralVertexAABB_min(gl_PrimitiveID);
	const vec3 aabb_max = GetProceduralVertexAABB_max(gl_PrimitiveID);
	const vec3 absMin = abs(ray.position.xyz - (gl_HitKindEXT == 0? aabb_min.xyz : aabb_max.xyz));
	const vec3 absMax = abs(ray.position.xyz - (gl_HitKindEXT == 0? aabb_max.xyz : aabb_min.xyz));
		 if (absMin.x < THRESHOLD) ray.normal.xyz = vec3(-1, 0, 0);
	else if (absMin.y < THRESHOLD) ray.normal.xyz = vec3( 0,-1, 0);
	else if (absMin.z < THRESHOLD) ray.normal.xyz = vec3( 0, 0,-1);
	else if (absMax.x < THRESHOLD) ray.normal.xyz = vec3( 1, 0, 0);
	else if (absMax.y < THRESHOLD) ray.normal.xyz = vec3( 0, 1, 0);
	else if (absMax.z < THRESHOLD) ray.normal.xyz = vec3( 0, 0, 1);
	
	// Vertex Colors
	if (HasVertexColor()) {
		ray.color = GetVertexColor(gl_PrimitiveID);
	} else {
		ray.color = vec4(1);
	}
}
