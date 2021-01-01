#define RAY_TRACING
#include "v4d/modules/V4D_raytracing/glsl_includes/set0.glsl"


#############################################################
#shader rint

hitAttributeEXT vec3 aabbGeomLocalPositionAttr;

float hitAabb(const vec3 minimum, const vec3 maximum) {
	vec3  invDir = 1.0 / gl_ObjectRayDirectionEXT;
	vec3  tbot   = invDir * (minimum - gl_ObjectRayOriginEXT);
	vec3  ttop   = invDir * (maximum - gl_ObjectRayOriginEXT);
	vec3  tmin   = min(ttop, tbot);
	vec3  tmax   = max(ttop, tbot);
	float t0     = max(tmin.x, max(tmin.y, tmin.z));
	float t1     = min(tmax.x, min(tmax.y, tmax.z));
	return t1 > max(t0, 0.0) ? t0 : -1.0;
}

void main() {
	vec3 aabb_min = GetProceduralVertexAABB_min(gl_PrimitiveID);
	vec3 aabb_max = GetProceduralVertexAABB_max(gl_PrimitiveID);
	aabbGeomLocalPositionAttr = (aabb_max + aabb_min)/2;
	reportIntersectionEXT(max(hitAabb(aabb_min, aabb_max), gl_RayTminEXT), 0);
}


#############################################################
#shader rendering.rchit

hitAttributeEXT vec3 aabbGeomLocalPositionAttr;

layout(location = 0) rayPayloadInEXT RayTracingPayload ray;

void main() {
	vec3 hitPointObj = gl_ObjectRayOriginEXT + gl_ObjectRayDirectionEXT * gl_HitTEXT;
	
	// Calculate normal for a cube (will most likely NOT work with any non-uniform cuboid)
	vec3 normal = normalize(hitPointObj - aabbGeomLocalPositionAttr);
	vec3 absN = abs(normal);
	float maxC = max(max(absN.x, absN.y), absN.z);
	normal = GetModelNormalViewMatrix() * (
		(maxC == absN.x) ?
			vec3(sign(normal.x), 0, 0) :
			((maxC == absN.y) ? 
				vec3(0, sign(normal.y), 0) : 
				vec3(0, 0, sign(normal.z))
			)
	);
	
	vec4 color = HasVertexColor()? GetVertexColor(gl_PrimitiveID) : vec4(0,0,0,1);
	
	WriteRayPayload(ray);
	ray.albedo = color.rgb;
	ray.opacity = color.a;
	ray.normal = normal;
	ray.metallic = 0.5;
	ray.roughness = 0.1;
}
