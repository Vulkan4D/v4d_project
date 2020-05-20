#include "core.glsl"
#include "rtx_base.glsl"
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
	
	WriteDepthFromHitDistance(ray.distance);
	
	// If we had a Hit
	if (ray.distance > 0) {
		
		pbrGBuffers.viewSpacePosition = ray.viewSpacePosition;
		pbrGBuffers.viewSpaceNormal = ray.viewSpaceNormal;
		pbrGBuffers.uv = ray.uv;
		pbrGBuffers.albedo = ray.albedo;
		pbrGBuffers.emit = ray.emit;
		pbrGBuffers.metallic = ray.metallic;
		pbrGBuffers.roughness = ray.roughness;
		pbrGBuffers.realDistanceFromCamera = ray.distance;
		
		WritePbrGBuffers();
	}
}


#############################################################
#shader rmiss

layout(location = 0) rayPayloadInEXT RayPayload_visibility ray;

void main() {
	ray.distance = 0;
}


#############################################################
#shader rchit

hitAttributeEXT vec3 hitAttribs;

layout(location = 0) rayPayloadInEXT RayPayload_visibility ray;

#include "rtx_fragment.glsl"

void main() {
	Fragment fragment = GetHitFragment(true);
	
	ray.viewSpacePosition = fragment.hitPoint;
	ray.viewSpaceNormal = fragment.viewSpaceNormal;
	ray.albedo = fragment.color.rgb;
	ray.emit = 0;
	ray.uv = fragment.uv;
	ray.metallic = 0.1;
	ray.roughness = 0.6;
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
	vec3 spherePosition = sphereGeomAttr.geometryInstance.viewPosition;
	float sphereRadius = sphereGeomAttr.aabbMax.x;
	
	// Hit World Position
	const vec3 hitPoint = gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * gl_HitTEXT;
	const vec3 normal = normalize(hitPoint - spherePosition);
	
	ray.viewSpacePosition = hitPoint;
	ray.viewSpaceNormal = normal;
	ray.albedo = sphereGeomAttr.color.rgb;
	ray.emit = 0;
	ray.uv = vec2(0);
	ray.metallic = 0.1;
	ray.roughness = 0.6;
	ray.distance = gl_HitTEXT;
}


#############################################################
#shader light.rchit

hitAttributeEXT ProceduralGeometry sphereGeomAttr;

layout(location = 0) rayPayloadInEXT RayPayload_visibility ray;

void main() {
	const vec3 hitPoint = gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * gl_HitTEXT;
	
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
	
	ray.viewSpacePosition = hitPoint;
	ray.viewSpaceNormal = vec3(0);
	ray.albedo = lightColor;
	ray.emit = emission;
	ray.uv = vec2(0);
	ray.metallic = 0;
	ray.roughness = 0;
	ray.distance = gl_HitTEXT;
}

