#include "core.glsl"
#include "v4d/modules/V4D_hybrid/glsl_includes/rtx.glsl"
#include "v4d/modules/V4D_hybrid/glsl_includes/pl_visibility_rays.glsl"

#############################################################
#shader rgen

layout(location = 0) rayPayloadEXT RayPayload_visibility ray;

void main() {
	const vec2 pixelCenter = vec2(gl_LaunchIDEXT.xy) + vec2(0.5);
	const vec2 d = pixelCenter/vec2(gl_LaunchSizeEXT.xy) * 2.0 - 1.0;
	const vec3 origin = vec3(0);
	const vec3 direction = normalize(vec4(inverse(camera.projectionMatrix) * dvec4(d.x, d.y, 1, 1)).xyz);

	// Trace Visibility Rays for Opaque geometry
	traceRayEXT(topLevelAS, gl_RayFlagsOpaqueEXT, GEOMETRY_ATTR_PRIMARY_VISIBLE, 0, 0, 0, origin, float(camera.znear), direction, float(camera.zfar), 0);
	
	if (ray.distance > 0) {
		// we have a Hit
		pbrGBuffers.viewSpacePosition = ray.viewSpacePosition;
		pbrGBuffers.viewSpaceNormal = ray.viewSpaceNormal;
		pbrGBuffers.uv = ray.uv;
		pbrGBuffers.albedo = ray.albedo;
		pbrGBuffers.emit = ray.emit;
		pbrGBuffers.metallic = ray.metallic;
		pbrGBuffers.roughness = ray.roughness;
		pbrGBuffers.distance = ray.distance;
		WritePbrGBuffers();
		WriteCustomBuffer(ray.customData);
	} else {
		// miss
		WriteEmptyPbrGBuffers();
		WriteEmptyCustomBuffer();
	}
}


#############################################################
#shader rmiss

layout(location = 0) rayPayloadInEXT RayPayload_visibility ray;

void main() {
	ray.distance = 0;
}


#############################################################
#shader basic.rchit

hitAttributeEXT vec3 hitAttribs;

layout(location = 0) rayPayloadInEXT RayPayload_visibility ray;

#include "rtx_fragment.glsl"

void main() {
	Fragment fragment = GetHitFragment(true);
	
	ray.customData = GenerateCustomData(fragment.objectIndex, /*type8*/0, /*flags32*/0, /*custom32*/0, /*custom32*/0);
	ray.viewSpacePosition = fragment.hitPoint;
	ray.viewSpaceNormal = fragment.viewSpaceNormal;
	ray.albedo = fragment.color.rgb;
	ray.emit = 0;
	ray.uv = PackUVasFloat(fragment.uv);
	ray.metallic = 0.0;
	ray.roughness = 0.0;
	ray.distance = gl_HitTEXT;
}


#############################################################
#shader standard.rchit

hitAttributeEXT vec3 hitAttribs;

layout(location = 0) rayPayloadInEXT RayPayload_visibility ray;

#include "rtx_fragment.glsl"

void main() {
	Fragment fragment = GetHitFragment(true);
	
	ray.customData = GenerateCustomData(fragment.objectIndex, /*type8*/0, /*flags32*/0, /*custom32*/0, /*custom32*/0);
	ray.viewSpacePosition = fragment.hitPoint;
	ray.viewSpaceNormal = fragment.viewSpaceNormal;
	ray.albedo = fragment.color.rgb;
	ray.emit = 0;
	ray.uv = PackUVasFloat(fragment.uv);
	ray.metallic = 0.0;
	ray.roughness = 0.0;
	ray.distance = gl_HitTEXT;
}


#############################################################
#shader terrain.rchit

hitAttributeEXT vec3 hitAttribs;

layout(location = 0) rayPayloadInEXT RayPayload_visibility ray;

#include "rtx_fragment.glsl"

void main() {
	Fragment fragment = GetHitFragment(true);
	
	ray.customData = GenerateCustomData(fragment.objectIndex, /*type8*/0, /*flags32*/0, /*custom32*/0, /*custom32*/0);
	ray.viewSpacePosition = fragment.hitPoint;
	ray.viewSpaceNormal = fragment.viewSpaceNormal;
	ray.albedo = fragment.color.rgb;
	ray.emit = 0;
	ray.uv = PackUVasFloat(fragment.uv);
	ray.metallic = 0.0;
	ray.roughness = 0.0;
	ray.distance = gl_HitTEXT;
}


#############################################################
#shader aabb.rint

hitAttributeEXT ProceduralGeometry aabbGeomAttr;

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
	ProceduralGeometry geom = GetProceduralGeometry(gl_InstanceCustomIndexEXT);
	float tHit = hitAabb(
		geom.aabbMin, 
		geom.aabbMax
	);
	aabbGeomAttr = geom;
	reportIntersectionEXT(max(tHit,gl_RayTminEXT), 0);
}


#############################################################
#shader aabb.rchit

hitAttributeEXT ProceduralGeometry aabbGeomAttr;

layout(location = 0) rayPayloadInEXT RayPayload_visibility ray;

void main() {
	const vec3 hitPoint = gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * gl_HitTEXT;
	vec4 color = aabbGeomAttr.color;
	
	// Calculate normal for a cube (will most likely NOT work with any non-uniform cuboid)
	const vec3 hitPointObj = gl_ObjectRayOriginEXT + gl_ObjectRayDirectionEXT * gl_HitTEXT;
	vec3 normal = normalize(hitPointObj - (aabbGeomAttr.aabbMin + aabbGeomAttr.aabbMax)/2.0);
	vec3 absN = abs(normal);
	float maxC = max(max(absN.x, absN.y), absN.z);
	normal = aabbGeomAttr.geometryInstance.normalViewTransform * (
		(maxC == absN.x) ?
			vec3(sign(normal.x), 0, 0) :
			((maxC == absN.y) ? 
				vec3(0, sign(normal.y), 0) : 
				vec3(0, 0, sign(normal.z))
			)
	);
	
	ray.customData = GenerateCustomData(aabbGeomAttr.objectIndex, /*type8*/0, /*flags32*/0, /*custom32*/0, /*custom32*/0);
	ray.viewSpacePosition = hitPoint;
	ray.viewSpaceNormal = normal;
	ray.albedo = color.rgb;
	ray.emit = aabbGeomAttr.custom1;
	ray.uv = 0;
	ray.metallic = 0.0;
	ray.roughness = 0.0;
	ray.distance = gl_HitTEXT;
}


#############################################################
#shader sphere.rint

hitAttributeEXT ProceduralGeometry sphereGeomAttr;

void main() {
	ProceduralGeometry geom = GetProceduralGeometry(gl_InstanceCustomIndexEXT);
	vec3 spherePosition = geom.geometryInstance.viewPosition;
	float sphereRadius = geom.aabbMax.x;
	
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
			sphereGeomAttr = geom;
			reportIntersectionEXT((tMin <= t1 && t1 < tMax) ? t1 : t2, 0);
		}
	}
}


#############################################################
#shader sphere.rchit

hitAttributeEXT ProceduralGeometry sphereGeomAttr;

layout(location = 0) rayPayloadInEXT RayPayload_visibility ray;

void main() {
	const vec3 hitPoint = gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * gl_HitTEXT;
	vec4 color = sphereGeomAttr.color;
	
	// Calculate normal for a sphere
	const vec3 normal = normalize(hitPoint - sphereGeomAttr.geometryInstance.viewPosition);
	
	ray.customData = GenerateCustomData(sphereGeomAttr.objectIndex, /*type8*/0, /*flags32*/0, /*custom32*/0, /*custom32*/0);
	ray.viewSpacePosition = hitPoint;
	ray.viewSpaceNormal = normal;
	ray.albedo = color.rgb;
	ray.emit = 0;
	ray.uv = 0;
	ray.metallic = 0.0;
	ray.roughness = 0.0;
	ray.distance = gl_HitTEXT;
}


#############################################################
#shader light.rchit

hitAttributeEXT ProceduralGeometry sphereGeomAttr;

layout(location = 0) rayPayloadInEXT RayPayload_visibility ray;

void main() {
	const vec3 hitPoint = gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * gl_HitTEXT;
	
	// Get color and emission from Light Source
	LightSource light = GetLight(sphereGeomAttr.material);
	vec3 lightColor;
	float emission;
	if (sphereGeomAttr.color.a > 0) {
		lightColor = sphereGeomAttr.color.rgb * sphereGeomAttr.color.a;
		emission = light.intensity/gl_HitTEXT/gl_HitTEXT;
	} else {
		lightColor = light.color;
		emission = light.intensity/gl_HitTEXT;
	}
	
	ray.customData = GenerateCustomData(sphereGeomAttr.objectIndex, /*type8*/0, /*flags32*/0, /*custom32*/0, /*custom32*/0);
	ray.viewSpacePosition = hitPoint;
	ray.viewSpaceNormal = vec3(0);
	ray.albedo = lightColor;
	ray.emit = emission;
	ray.uv = 0;
	ray.metallic = 0;
	ray.roughness = 0;
	ray.distance = gl_HitTEXT;
}


#############################################################
#shader sun.rchit

hitAttributeEXT ProceduralGeometry sphereGeomAttr;

layout(location = 0) rayPayloadInEXT RayPayload_visibility ray;

void main() {
	const vec3 hitPoint = gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * gl_HitTEXT;
	
	// Get color and emission from Light Source
	LightSource light = GetLight(sphereGeomAttr.material);
	vec3 lightColor;
	float emission;
	if (sphereGeomAttr.color.a > 0) {
		lightColor = sphereGeomAttr.color.rgb * sphereGeomAttr.color.a;
		emission = light.intensity/gl_HitTEXT/gl_HitTEXT;
	} else {
		lightColor = light.color;
		emission = light.intensity/gl_HitTEXT;
	}
	
	ray.customData = GenerateCustomData(sphereGeomAttr.objectIndex, /*type8*/0, /*flags32*/0, /*custom32*/0, /*custom32*/0);
	ray.viewSpacePosition = hitPoint;
	ray.viewSpaceNormal = vec3(0);
	ray.albedo = lightColor;
	ray.emit = emission;
	ray.uv = 0;
	ray.metallic = 0;
	ray.roughness = 0;
	ray.distance = gl_HitTEXT;
}


