#define RAY_TRACING
#include "v4d/modules/V4D_raytracing/glsl_includes/set0.glsl"

layout(set = 1, binding = 0, rgba16f) uniform image2D img_lit;
layout(set = 1, binding = 1, r32f) uniform image2D img_depth;


// 64 bytes
struct RayTracingPayload {
	vec3 albedo;
	vec3 normal;
	vec3 emission;
	vec3 position;
	float refractionIndex;
	float metallic;
	float roughness;
	float distance;
};


#############################################################
#shader rgen

layout(location = 0) rayPayloadEXT RayTracingPayload ray;
layout(location = 1) rayPayloadEXT bool shadowed;

const int MAX_BOUNCES = 10; // 0 is infinite

#include "v4d/modules/V4D_raytracing/glsl_includes/core_pbr.glsl"

void main() {
	const ivec2 imgCoords = ivec2(gl_LaunchIDEXT.xy);
	const vec2 pixelCenter = vec2(gl_LaunchIDEXT.xy) + vec2(0.5);
	const vec2 d = pixelCenter/vec2(gl_LaunchSizeEXT.xy) * 2.0 - 1.0;
	const vec3 origin = vec3(0);
	const vec3 direction = normalize(vec4(inverse(camera.projectionMatrix) * dvec4(d.x, d.y, 1, 1)).xyz);
	vec3 litColor = vec3(0);
	int bounces = 0;

	// Trace Primary Rays
	traceRayEXT(topLevelAS, gl_RayFlagsOpaqueEXT, GEOMETRY_ATTR_PRIMARY_VISIBLE, 0, 0, 0, origin, float(camera.znear), direction, float(camera.zfar), 0);
	
	if (ray.distance > 0) {
		litColor = ApplyPBRShading(ray.position, ray.albedo, ray.normal, /*bump*/vec3(0), ray.roughness, ray.metallic) + ray.emission;
		
		// // Refraction
		// if (ray.refractionIndex >= 1.0) {
		// 	vec3 glassColor = ray.albedo;
		// 	float glassRoughness = ray.roughness;
		// 	//TODO
		// }
		
		float reflectivity = min(0.9, ray.metallic);
		vec3 reflectionOrigin = ray.position + ray.normal * 0.01;
		vec3 viewDirection = normalize(ray.position);
		vec3 surfaceNormal = normalize(ray.normal);
		while (Reflections && reflectivity > 0.01) {
			vec3 reflectDirection = reflect(viewDirection, surfaceNormal);
			traceRayEXT(topLevelAS, gl_RayFlagsOpaqueEXT, GEOMETRY_ATTR_REFLECTION_VISIBLE, 0, 0, 0, reflectionOrigin, float(camera.znear), reflectDirection, float(camera.zfar), 0);
			if (ray.distance > 0) {
				vec3 reflectColor = ApplyPBRShading(ray.position, ray.albedo, ray.normal, /*bump*/vec3(0), ray.roughness, ray.metallic) + ray.emission;
				litColor = mix(litColor, reflectColor*litColor, reflectivity);
				reflectivity *= min(0.9, ray.metallic);
				if (reflectivity > 0) {
					reflectionOrigin = ray.position + ray.normal * 0.01;
					viewDirection = normalize(ray.position);
					surfaceNormal = normalize(ray.normal);
				}
			} else {
				litColor = mix(litColor, litColor*vec3(0.5)/* fake atmosphere color (temporary) */, reflectivity);
				reflectivity = 0;
			}
			if (MAX_BOUNCES > 0 && ++bounces > MAX_BOUNCES) break;
		}
		
	} else {
		vec3(0); // background
	}
	
	float depth = clamp(GetFragDepthFromViewSpacePosition(ray.position), 0, 1);
	imageStore(img_depth, imgCoords, vec4(depth, 0,0,0));
	imageStore(img_lit, imgCoords, vec4(litColor,1));
}


#############################################################
#shader rchit

hitAttributeEXT vec3 hitAttribs;

layout(location = 0) rayPayloadInEXT RayTracingPayload ray;

void main() {
	vec3 hitPoint = gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * gl_HitTEXT;
	
	uint i0 = GetIndex(0);
	uint i1 = GetIndex(1);
	uint i2 = GetIndex(2);
	
	// Interpolate fragment
	vec3 barycentricCoords = vec3(1.0f - hitAttribs.x - hitAttribs.y, hitAttribs.x, hitAttribs.y);
	// vec3 pos = (
	// 	+ GetVertexPosition(i0) * barycentricCoords.x
	// 	+ GetVertexPosition(i1) * barycentricCoords.y
	// 	+ GetVertexPosition(i2) * barycentricCoords.z
	// );
	vec3 normal = normalize(
		+ GetVertexNormal(i0) * barycentricCoords.x
		+ GetVertexNormal(i1) * barycentricCoords.y
		+ GetVertexNormal(i2) * barycentricCoords.z
	);
	vec4 color = HasVertexColor()? (
		+ GetVertexColor(i0) * barycentricCoords.x
		+ GetVertexColor(i1) * barycentricCoords.y
		+ GetVertexColor(i2) * barycentricCoords.z
	) : vec4(0);
	// vec2 uv = HasVertexUV()? (
	// 	+ GetVertexUV(i0) * barycentricCoords.x
	// 	+ GetVertexUV(i1) * barycentricCoords.y
	// 	+ GetVertexUV(i2) * barycentricCoords.z
	// ) : vec2(0);
	
	ray.albedo = color.rgb;
	ray.normal = normalize(GetModelNormalViewMatrix() * normal);
	ray.emission = vec3(0);
	ray.position = hitPoint;
	ray.refractionIndex = 0.0;
	ray.metallic = 0.5;
	ray.roughness = 0.1;
	ray.distance = gl_HitTEXT;
}


#############################################################
#shader rmiss

layout(location = 0) rayPayloadInEXT RayTracingPayload ray;

void main() {
	ray.distance = 0;
}


#############################################################
#shader shadow.rmiss

layout(location = 1) rayPayloadInEXT bool shadowed;

void main() {
	shadowed = false;
}


#############################################################
#shader aabb.cube.rint

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
	float tHit = hitAabb(
		aabb_min, 
		aabb_max
	);
	aabbGeomLocalPositionAttr = (aabb_max + aabb_min)/2;
	reportIntersectionEXT(max(tHit,gl_RayTminEXT), 0);
}


#############################################################
#shader aabb.cube.rchit

hitAttributeEXT vec3 aabbGeomLocalPositionAttr;

layout(location = 0) rayPayloadInEXT RayTracingPayload ray;

void main() {
	vec3 hitPoint = gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * gl_HitTEXT;
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
	
	ray.albedo = HasVertexColor()? GetVertexColor(gl_PrimitiveID).rgb : vec3(0);
	ray.normal = normal;
	ray.emission = vec3(0);
	ray.position = hitPoint;
	ray.refractionIndex = 0.0;
	ray.metallic = 0.5;
	ray.roughness = 0.1;
	ray.distance = gl_HitTEXT;
}


#############################################################
#shader aabb.sphere.rint

hitAttributeEXT vec3 sphereGeomPositionAttr;

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

		if ((tMin <= t1 && t1 < tMax) || (tMin <= t2 && t2 < tMax)) {
			sphereGeomPositionAttr = spherePosition;
			reportIntersectionEXT((tMin <= t1 && t1 < tMax) ? t1 : t2, 0);
		}
	}
}


#############################################################
#shader aabb.sphere.rchit

hitAttributeEXT vec3 sphereGeomPositionAttr;

layout(location = 0) rayPayloadInEXT RayTracingPayload ray;

void main() {
	vec3 hitPoint = gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * gl_HitTEXT;
	vec3 normal = normalize(hitPoint - sphereGeomPositionAttr);
	
	ray.albedo = HasVertexColor()? GetVertexColor(gl_PrimitiveID).rgb : vec3(0);
	ray.normal = normalize(hitPoint - sphereGeomPositionAttr);
	ray.emission = vec3(0);
	ray.position = hitPoint;
	ray.refractionIndex = 0.0;
	ray.metallic = 0.8;
	ray.roughness = 0.1;
	ray.distance = gl_HitTEXT;
}


#############################################################
#shader aabb.sphere.light.rchit

hitAttributeEXT vec3 sphereGeomPositionAttr;

layout(location = 0) rayPayloadInEXT RayTracingPayload ray;

void main() {
	vec3 hitPoint = gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * gl_HitTEXT;
	vec3 normal = normalize(hitPoint - sphereGeomPositionAttr);
	vec4 color = HasVertexColor()? GetVertexColor(gl_PrimitiveID) : vec4(0);
	
	ray.albedo = vec3(0);
	ray.normal = normalize(hitPoint - sphereGeomPositionAttr);
	ray.emission = color.rgb;
	ray.position = hitPoint;
	ray.refractionIndex = 0.0;
	ray.metallic = 0.0;
	ray.roughness = 0.0;
	ray.distance = gl_HitTEXT;
}
