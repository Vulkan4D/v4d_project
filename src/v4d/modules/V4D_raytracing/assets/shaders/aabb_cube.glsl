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
#shader visibility.rchit

hitAttributeEXT vec3 aabbGeomLocalPositionAttr;

layout(location = RAY_PAYLOAD_LOCATION_VISIBILITY) rayPayloadInEXT VisibilityPayload ray;

void main() {
	WriteRayPayload(ray);
	
	// Calculate normal for a cube (will most likely NOT work with any non-uniform cuboid)
	vec3 n = normalize(ray.position.xyz - aabbGeomLocalPositionAttr);
	vec3 absN = abs(n);
	float maxC = max(max(absN.x, absN.y), absN.z);
	ray.normal.xyz = (maxC == absN.x) ?
		vec3(sign(n.x), 0, 0)
		 :
		((maxC == absN.y) ? 
			vec3(0, sign(n.y), 0) : 
			vec3(0, 0, sign(n.z))
		)
	;
	
	if (HasVertexColor()) {
		ray.color = GetVertexColor(gl_PrimitiveID);
	}
	
	
	// // ray.color = color;
	
	// // // float metallic = 0.5;
	// // // float roughness = 0.5;
	// // // if (metallic > 0) {
	// // // 	ray.color.a = 1.0 - FresnelReflectAmount(1.0, GetGeometry().material.visibility.indexOfRefraction, normal, gl_WorldRayDirectionEXT, metallic);
	// // // 	ScatterMetallic(ray, roughness, gl_WorldRayDirectionEXT, normal);
	// // // } else if (color.a < 1) {
	// // // 	ScatterDieletric(ray, GetGeometry().material.visibility.indexOfRefraction, gl_WorldRayDirectionEXT, normal);
	// // // } else {
	// // // 	ScatterLambertian(ray, roughness, normal);
	// // // }
	
	// // // DebugRay(ray, color.rgb, normal, GetGeometry().material.visibility.emission, metallic, roughness);
	
	
	
	
}
