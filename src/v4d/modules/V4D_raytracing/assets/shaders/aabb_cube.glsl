#define RAY_TRACING
#include "v4d/modules/V4D_raytracing/glsl_includes/set0.glsl"

#############################################################
#shader rint

// hitAttributeEXT vec3 aabbGeomLocalPositionAttr;

void main() {
	vec3 aabb_min = GetProceduralVertexAABB_min(gl_PrimitiveID);
	vec3 aabb_max = GetProceduralVertexAABB_max(gl_PrimitiveID);
	
	vec3  invDir = 1.0 / gl_ObjectRayDirectionEXT;
	vec3  tbot   = invDir * (aabb_min - gl_ObjectRayOriginEXT);
	vec3  ttop   = invDir * (aabb_max - gl_ObjectRayOriginEXT);
	vec3  tmin   = min(ttop, tbot);
	vec3  tmax   = max(ttop, tbot);
	float t1     = max(tmin.x, max(tmin.y, tmin.z));
	float t2     = min(tmax.x, min(tmax.y, tmax.z));
	
	// aabbGeomLocalPositionAttr = (aabb_max + aabb_min)/2;

	// Outside of box
	if (gl_RayTminEXT <= t1 && t1 < gl_RayTmaxEXT && t2 > t1) {
		reportIntersectionEXT(t1, 0);
	}
	
	// Inside of box
	else if (t1 <= gl_RayTminEXT && t2 >= gl_RayTminEXT) {
		reportIntersectionEXT(gl_RayTminEXT, 1);
	}
}


#############################################################
#shader visibility.rchit

// hitAttributeEXT vec3 aabbGeomLocalPositionAttr;

layout(location = RAY_PAYLOAD_LOCATION_VISIBILITY) rayPayloadInEXT VisibilityPayload ray;

void main() {
	WriteRayPayload(ray);
	
	// // Calculate normal for a cube (only works with perfect uniform cubes)
	// vec3 n = normalize(ray.position.xyz - aabbGeomLocalPositionAttr);
	// vec3 absN = abs(n);
	// float maxC = max(max(absN.x, absN.y), absN.z);
	// ray.normal.xyz = (maxC == absN.x) ?
	// 	vec3(sign(n.x), 0, 0)
	// 	 :
	// 	((maxC == absN.y) ? 
	// 		vec3(0, sign(n.y), 0) : 
	// 		vec3(0, 0, sign(n.z))
	// 	)
	// ;
	
	// Calculate normal for a box (this method works for boxes with arbitrary width/height/depth)
	const float THRESHOLD = 0.000001 * (ray.normal.w+ray.position.w);
	vec3 aabb_min = GetProceduralVertexAABB_min(gl_PrimitiveID);
	vec3 aabb_max = GetProceduralVertexAABB_max(gl_PrimitiveID);
	vec3 absMin = abs(ray.position.xyz - aabb_min.xyz);
	vec3 absMax = abs(ray.position.xyz - aabb_max.xyz);
		 if (absMin.x < THRESHOLD) ray.normal.xyz = vec3(-1, 0, 0);
	else if (absMin.y < THRESHOLD) ray.normal.xyz = vec3( 0,-1, 0);
	else if (absMin.z < THRESHOLD) ray.normal.xyz = vec3( 0, 0,-1);
	else if (absMax.x < THRESHOLD) ray.normal.xyz = vec3( 1, 0, 0);
	else if (absMax.y < THRESHOLD) ray.normal.xyz = vec3( 0, 1, 0);
	else if (absMax.z < THRESHOLD) ray.normal.xyz = vec3( 0, 0, 1);
	else {
		// We're inside the box
	}
	
	if (HasVertexColor()) {
		ray.color = GetVertexColor(gl_PrimitiveID);
	} else {
		ray.color = vec4(1);
	}
	
}
