#include "core.glsl"
#include "rtx_base.glsl"

#############################################################
#shader rgen

layout(location = 0) rayPayloadEXT RayPayload ray;

void main() {
	const vec2 pixelCenter = vec2(gl_LaunchIDEXT.xy) + vec2(0.5);
	const vec2 inUV = pixelCenter/vec2(gl_LaunchSizeEXT.xy);
	const vec2 d = inUV * 2.0 - 1.0;
	const vec3 target = vec4(inverse(camera.projectionMatrix) * dvec4(d.x, d.y, 1, 1)).xyz;
	
	vec3 origin = vec3(0); //vec4(inverse(camera.viewMatrix) * dvec4(0,0,0,1)).xyz;
	vec3 direction = normalize(target); //vec4(inverse(camera.viewMatrix) * dvec4(normalize(target), 0)).xyz;

	int traceMask = SOLID|LIQUID|CLOUD|PARTICLE|TRANSPARENT|CUTOUT|CELESTIAL;
	#ifndef RENDER_LIGHT_SPHERES_MANUALLY_IN_RGEN
		traceMask |= EMITTER;
	#endif
	
	ray.color = vec3(0);
	ray.opacity = 0;
	ray.distance = 0;
	
	// if (reflection_max_recursion > 1) {
	// 	float reflection = 1.0;
	// 	for (int i = 0; i < reflection_max_recursion; i++) {
	// 		ray.reflector = 0.0;
	// 		traceEXT(topLevelAS, gl_RayFlagsOpaqueEXT, traceMask, 0, 0, 0, origin, 0.001, direction, max_distance, 0);
	// 		finalColor = mix(finalColor, ray.color, reflection);
	// 		if (ray.reflector <= 0.0) break;
	// 		reflection *= ray.reflector;
	// 		if (reflection < 0.01) break;
	// 		max_distance -= ray.distance;
	// 		if (max_distance <= 0) break;
	// 		origin = ray.origin;
	// 		direction = ray.direction;
	// 	}
	// } else {
		traceRayEXT(topLevelAS, gl_RayFlagsOpaqueEXT, traceMask, 0, 0, 0, origin, float(camera.znear), direction, float(camera.zfar), 0);
	// }
	
	#ifdef RENDER_LIGHT_SPHERES_MANUALLY_IN_RGEN
		// Manual ray-intersection for light spheres
		for (int activeLightIndex = 0; activeLightIndex < activeLights; activeLightIndex++) {
			LightSource light = GetLight(lightIndices[activeLightIndex]);
			float dist = length(light.position - origin) - light.radius;
			if (dist <= 0) {
				ray.color += light.color*light.intensity;
				ray.distance = 0;
			} else if ((dist < ray.distance || ray.distance == 0) && light.radius > 0.0) {
				vec3 oc = origin - light.position;
				if (dot(normalize(oc), direction) < 0) {
					float a = dot(direction, direction);
					float b = 2.0 * dot(oc, direction);
					float c = dot(oc,oc) - light.radius*light.radius;
					float discriminent = b*b - 4*a*c;
					dist = (-b - sqrt(discriminent)) / (2.0*a);
					if ((dist < ray.distance || ray.distance == 0) && discriminent >= 0) {
						float d = discriminent / light.radius*light.radius*4.0;
						ray.color += mix(vec3(0), light.color*pow(light.intensity, 0.5/light.radius), d);
						ray.distance = dist;
					}
				}
			}
		}
	#endif
	
	float depth = float(GetDepthBufferFromTrueDistance(ray.distance));
	
	ivec2 coords = ivec2(gl_LaunchIDEXT.xy);
	imageStore(depthImage, coords, vec4(ray.distance>0? depth:0, 0,0,0));
	imageStore(litImage, coords, vec4(ray.color, 1.0));
	
	//TODO also write to : 
	// gBuffer_albedo_geometryIndex
	// gBuffer_normal_uv
	imageStore(gBuffer_position_dist, coords, vec4(ray.origin*ray.direction*ray.distance, ray.distance));
}


// #############################################################
// #shader rahit

// layout(location = 0) rayPayloadInEXT RayPayload ray;
// hitAttributeEXT vec3 hitAttribs;

// void main() {
// 	// Fragment fragment = GetHitFragment(true);
// 	// if (fragment.color.a < 0.99) {
// 	// 	ray.color += fragment.color.rgb * fragment.color.a;
// 	// }
	
// 	// ignoreIntersectionEXT();
	
// 	// ray.color += fragment.color.rgb * fragment.color.a;
// }


#############################################################
#shader rchit

hitAttributeEXT vec3 hitAttribs;

layout(location = 0) rayPayloadInEXT RayPayload ray;
layout(location = 2) rayPayloadEXT bool shadowed;

#include "rtx_fragment.glsl"
#include "rtx_pbr.glsl"

void main() {
	Fragment fragment = GetHitFragment(true);
	
	vec3 color = ApplyPBRShading(fragment.hitPoint, fragment.color.rgb, fragment.viewSpaceNormal, /*bump*/vec3(0), /*roughness*/0.5, /*metallic*/0.0);
	
	// Transparency ?
		// if (ray.opacity < 0.9 && fragment.color.a < 0.99) {
		// 	traceEXT(topLevelAS, gl_RayFlagsOpaqueEXT, 0xff, 0, 0, 0, fragment.hitPoint, float(camera.znear), ray.direction, float(camera.zfar), 0);
		// }
		// ray.color = mix(ray.color, color, fragment.color.a);
		// ray.opacity += fragment.color.a;
	// or
		ray.color = color;
	
	ray.distance = gl_HitTEXT;
}


#############################################################
#shader rmiss

layout(location = 0) rayPayloadInEXT RayPayload ray;

void main() {
	//... may return a background color if other than black
	ray.distance = 0;
}


#############################################################

#shader shadow.rmiss

layout(location = 2) rayPayloadInEXT bool shadowed;

void main() {
	shadowed = false;
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

layout(location = 0) rayPayloadInEXT RayPayload ray;
layout(location = 2) rayPayloadEXT bool shadowed;

#include "rtx_pbr.glsl"

void main() {
	vec3 spherePosition = sphereGeomAttr.geometryInstance.viewPosition;
	float sphereRadius = sphereGeomAttr.aabbMax.x;
	
	// Hit World Position
	const vec3 hitPoint = gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * gl_HitTEXT;
	const vec3 normal = normalize(hitPoint - spherePosition);
	
	vec3 color = ApplyPBRShading(hitPoint, sphereGeomAttr.color.rgb, normal, /*bump*/vec3(0), /*roughness*/0.5, /*metallic*/0.0);
	
	ray.color = color;
	ray.distance = gl_HitTEXT;
}


#############################################################
#shader light.rchit

hitAttributeEXT ProceduralGeometry sphereGeomAttr;

layout(location = 0) rayPayloadInEXT RayPayload ray;
layout(location = 2) rayPayloadEXT bool shadowed;

void main() {
	LightSource light = GetLight(sphereGeomAttr.material);
	if (sphereGeomAttr.color.a > 0) {
		ray.color = sphereGeomAttr.color.rgb * sphereGeomAttr.color.a * light.intensity/gl_HitTEXT/gl_HitTEXT;
	} else {
		ray.color = light.color * light.intensity/gl_HitTEXT;
	}
	ray.distance = 0;
}

