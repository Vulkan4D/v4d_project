#version 460
#extension GL_NV_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_buffer_reference : enable

// Settings
const bool shadowsEnabled = true;
const float radianceThreshold = 1e-10;
// const int reflection_max_recursion = 3;

// Descriptor Set 0
#include "Camera.glsl"
layout(set = 0, binding = 1) readonly buffer ActiveLights {
	uint activeLights;
	uint lights[];
};
layout(set = 0, binding = 2) readonly buffer Lights {float lightSources[];};

// Descriptor Set 1
layout(set = 1, binding = 0) uniform accelerationStructureNV topLevelAS;
layout(set = 1, binding = 1, rgba16f) uniform image2D litImage;
layout(set = 1, binding = 2) readonly buffer Geometries {uvec4 geometries[];};
layout(set = 1, binding = 3) readonly buffer Indices {uint indices[];};
layout(set = 1, binding = 4) readonly buffer VertexPositions {vec4 vertexPositions[];};
layout(set = 1, binding = 5) readonly buffer VertexMaterials {uint vertexMaterials[];};
layout(set = 1, binding = 6) readonly buffer VertexNormals {vec2 vertexNormals[];};
layout(set = 1, binding = 7) readonly buffer VertexUVs {uint vertexUVs[];};
layout(set = 1, binding = 8) readonly buffer VertexColors {uint vertexColors[];};
layout(set = 1, binding = 9, r32f) uniform image2D depthImage;

// Ray Tracing Payload
struct RayPayload {
	vec3 color;
	vec3 origin;
	vec3 direction;
	float distance;
	// float reflector;
};
 
struct Vertex {
	vec3 pos;
	float info;
	uint mat;
	vec3 normal;
	vec2 uv;
	vec4 color;
};

vec3 UnpackNormal(in vec2 norm) {
	vec2 fenc = norm * 4.0 - 2.0;
	float f = dot(fenc, fenc);
	float g = sqrt(1.0 - f / 4.0);
	return vec3(fenc * g, 1.0 - f / 2.0);
}

vec2 UnpackUV(in uint uv) {
	return vec2(
		(uv & 0xffff0000) >> 16,
		(uv & 0x0000ffff) >> 0
	) / 65535.0;
}

vec4 UnpackColor(in uint color) {
	return vec4(
		(color & 0xff000000) >> 24,
		(color & 0x00ff0000) >> 16,
		(color & 0x0000ff00) >> 8,
		(color & 0x000000ff) >> 0
	) / 255.0;
}

struct LightSource {
	vec3 position;
	float intensity;
	vec3 color;
	uint type;
	uint attributes;
	float radius;
};

LightSource GetLight (uint i) {
	LightSource light;
	light.position = vec3(lightSources[i*8 + 0], lightSources[i*8 + 1], lightSources[i*8 + 2]);
	light.intensity = lightSources[i*8 + 3];
	vec4 c = UnpackColor(floatBitsToUint(lightSources[i*8 + 4]));
	light.color = c.rgb;
	light.type = floatBitsToUint(lightSources[i*8 + 4]) & 0x000000ff;
	light.attributes = floatBitsToUint(lightSources[i*8 + 5]);
	light.radius = lightSources[i*8 + 6];
	// light._unused_ = lightSources[i*8 + 7];
	return light;
};

Vertex GetVertex(uint index) {
	Vertex v;
	v.pos = vertexPositions[index].xyz;
	v.info = vertexPositions[index].w;
	v.mat = vertexMaterials[index];
	v.normal = UnpackNormal(vertexNormals[index]);
	v.uv = UnpackUV(vertexUVs[index]);
	v.color = UnpackColor(vertexColors[index]);
	return v;
}

#####################################################
#common .*rgen

layout(location = 0) rayPayloadNV RayPayload ray;

#####################################################
#common .*(hit|rmiss)

layout(location = 0) rayPayloadInNV RayPayload ray;

#####################################################
#common .*hit

hitAttributeNV vec3 hitAttribs;

struct Fragment {
	uint indexOffset;
	uint vertexOffset;
	uint objectIndex;
	uint otherIndex;
	Vertex v0;
	Vertex v1;
	Vertex v2;
	vec3 barycentricCoords;
	vec3 hitPoint; // World space
	vec3 pos;
	float info;
	vec3 normal;
	vec2 uv;
	vec4 color;
};

Fragment GetHitFragment(bool interpolateVertexData) {
	Fragment f;
	
	f.indexOffset = geometries[gl_InstanceCustomIndexNV].x;
	f.vertexOffset = geometries[gl_InstanceCustomIndexNV].y;
	f.objectIndex = geometries[gl_InstanceCustomIndexNV].z;
	f.otherIndex = geometries[gl_InstanceCustomIndexNV].w;
	
	f.v0 = GetVertex(indices[f.indexOffset + (3 * gl_PrimitiveID) + 0] + f.vertexOffset);
	f.v1 = GetVertex(indices[f.indexOffset + (3 * gl_PrimitiveID) + 1] + f.vertexOffset);
	f.v2 = GetVertex(indices[f.indexOffset + (3 * gl_PrimitiveID) + 2] + f.vertexOffset);
	
	f.barycentricCoords = vec3(1.0f - hitAttribs.x - hitAttribs.y, hitAttribs.x, hitAttribs.y);
	f.hitPoint = gl_WorldRayOriginNV + gl_WorldRayDirectionNV * gl_HitTNV;
	
	if (interpolateVertexData) {
		f.pos = (f.v0.pos * f.barycentricCoords.x + f.v1.pos * f.barycentricCoords.y + f.v2.pos * f.barycentricCoords.z);
		f.info = (f.v0.info * f.barycentricCoords.x + f.v1.info * f.barycentricCoords.y + f.v2.info * f.barycentricCoords.z);
		f.normal = normalize(f.v0.normal * f.barycentricCoords.x + f.v1.normal * f.barycentricCoords.y + f.v2.normal * f.barycentricCoords.z);
		f.uv = (f.v0.uv * f.barycentricCoords.x + f.v1.uv * f.barycentricCoords.y + f.v2.uv * f.barycentricCoords.z);
		f.color = (f.v0.color * f.barycentricCoords.x + f.v1.color * f.barycentricCoords.y + f.v2.color * f.barycentricCoords.z);
	}
	
	return f;
}

layout(location = 2) rayPayloadNV bool shadowed;

vec3 ViewSpaceNormal(vec3 normal) {
	return normalize(transpose(inverse(mat3(camera.viewMatrix))) * normal);
}

const float PI = 3.1415926543;

// PBR formulas
float distributionGGX(float NdotH, float roughness) {
	float a = roughness*roughness;
	float a2 = a*a;
	float denom = NdotH*NdotH * (a2 - 1.0) + 1.0;
	denom = PI * denom*denom;
	return a2 / max(denom, 0.0000001);
}
float geometrySmith(float NdotV, float NdotL, float roughness) {
	float r = roughness + 1.0;
	float k = (r*r) / 8.0;
	float ggx1 = NdotV / (NdotV * (1.0 - k) + k);
	float ggx2 = NdotL / (NdotL * (1.0 - k) + k);
	return ggx1 * ggx2;
}
vec3 fresnelSchlick(float HdotV, vec3 baseReflectivity) {
	// baseReflectivity in range 0 to 1
	// returns range baseReflectivity to 1
	// increases as HdotV decreases (more reflectivity when surface viewed at larger angles)
	return baseReflectivity + (1.0 - baseReflectivity) * pow(1.0 - HdotV, 5.0);
}

void ApplyStandardShading(vec3 hitPoint, vec3 color, vec3 normal, float roughness, float metallic) {
	vec3 viewSpaceNormal = ViewSpaceNormal(normal);
	
	// PBR lighting
	vec3 N = normalize(viewSpaceNormal);
	vec3 V = normalize(-hitPoint);
	// calculate reflectance at normal incidence; if dia-electric (like plastic) use baseReflectivity of 0.4 and if it's metal, use the albedo color as baseReflectivity
	vec3 baseReflectivity = mix(vec3(0.04), color, max(0,metallic));
	
	for (int lightIndex = 0; lightIndex < activeLights; lightIndex++) {
		LightSource light = GetLight(lights[lightIndex]);
		
		// Calculate light radiance at distance
		float dist = length(light.position - hitPoint);
		vec3 radiance = light.color * light.intensity * (1.0 / (dist*dist));
		
		if (length(radiance) > radianceThreshold) {
			if (shadowsEnabled) {
				shadowed = true;
				traceNV(topLevelAS, gl_RayFlagsTerminateOnFirstHitNV | gl_RayFlagsOpaqueNV | gl_RayFlagsSkipClosestHitShaderNV, 0xFF, 0, 0, 1, hitPoint, 0.001, normalize(light.position - hitPoint), length(light.position - hitPoint), 2);
			}
			if (!shadowsEnabled || !shadowed) {
				// cook-torrance BRDF
				vec3 L = normalize(light.position - hitPoint);
				vec3 H = normalize(V + L);
				float NdotV = max(dot(N,V), 0.000001);
				float NdotL = max(dot(N,L), 0.000001);
				float HdotV = max(dot(H,V), 0.0);
				float NdotH = max(dot(N,H), 0.0);
				//
				float D = distributionGGX(NdotH, roughness); // larger the more micro-facets aligned to H (normal distribution function)
				float G = geometrySmith(NdotV, NdotL, roughness); // smaller the more micro-facets shadowed by other micro-facets
				vec3 F = fresnelSchlick(HdotV, baseReflectivity); // proportion of specular reflectance
				vec3 FV = fresnelSchlick(NdotV, baseReflectivity); // proportion of specular reflectance
				vec3 FL = fresnelSchlick(NdotL, baseReflectivity); // proportion of specular reflectance
				//
				vec3 specular = D * G * F;
				specular /= 4.0 * NdotV * NdotL;
				// for energy conservation, the diffuse and specular light can't be above 1.0 (unless the surface emits light); to preserve this relationship the diffuse component (kD) should equal to 1.0 - kS.
				// vec3 kD = vec3(1.0)-F;
				vec3 kD = (vec3(1.0)-FV) * (vec3(1.0)-FL) * 1.05;
				// multiply kD by the inverse metalness such that only non-metals have diffuse lighting, or a linear blend if partly metal (pure metals have no diffuse light).
				kD *= 1.0 - max(0, metallic);
				// Final lit color
				ray.color += (kD * color / PI + specular) * radiance * max(dot(N,L) + max(0, metallic/-5), 0);
				
				// Sub-Surface Scattering (simple rim for now)
				// if (metallic < 0) {
					float rim = pow(1.0 - NdotV, 2-metallic*2+NdotL) * NdotL;
					ray.color += -min(0,metallic) * radiance * rim;
				// }
			}
		}
	}
	
	
	//TODO opacity
	
	
	// // Reflections
	// if (reflection_max_recursion > 1) {
	// 	ray.origin = hitPoint + viewSpaceNormal * 0.001f;
	// 	ray.direction = reflect(gl_WorldRayDirectionNV, viewSpaceNormal);
	// 	// ray.reflector = clamp(metallic - roughness, 0, 1);
	// }
	
	
	ray.distance = gl_HitTNV;
}


				// #####################################################

				// #common sphere.*

				// 		// // Sphere
				// 		// struct Sphere {
				// 		// 	float emissive;
				// 		// 	float roughness;
				// 		// 	vec3 pos;
				// 		// 	float radius;
				// 		// 	vec4 color;
				// 		// 	float specular;
				// 		// 	float metallic;
				// 		// 	float refraction;
				// 		// 	float density;
				// 		// };
				// 		// uint sphereStructSize = 5;
				// 		// Sphere unpackSphere(uint index) {
				// 		// 	Sphere s;
				// 		// 	s.emissive = sphereBuffer[sphereStructSize * index + 1].z;
				// 		// 	s.roughness = sphereBuffer[sphereStructSize * index + 1].w;
				// 		// 	s.pos = sphereBuffer[sphereStructSize * index + 2].xyz;
				// 		// 	s.radius = sphereBuffer[sphereStructSize * index + 2].w;
				// 		// 	s.color = sphereBuffer[sphereStructSize * index + 3];
				// 		// 	s.specular = sphereBuffer[sphereStructSize * index + 4].x;
				// 		// 	s.metallic = sphereBuffer[sphereStructSize * index + 4].y;
				// 		// 	s.refraction = sphereBuffer[sphereStructSize * index + 4].z;
				// 		// 	s.density = sphereBuffer[sphereStructSize * index + 4].w;
				// 		// 	return s;
				// 		// }


#############################################################
#shader rgen

void main() {
	const vec2 pixelCenter = vec2(gl_LaunchIDNV.xy) + vec2(0.5);
	const vec2 inUV = pixelCenter/vec2(gl_LaunchSizeNV.xy);
	const vec2 d = inUV * 2.0 - 1.0;
	const vec3 target = vec4(inverse(camera.projectionMatrix) * dvec4(d.x, d.y, 1, 1)).xyz;
	
	vec3 origin = vec3(0); //vec4(inverse(camera.viewMatrix) * dvec4(0,0,0,1)).xyz;
	vec3 direction = normalize(target); //vec4(inverse(camera.viewMatrix) * dvec4(normalize(target), 0)).xyz;
	
	vec3 finalColor = vec3(0);
	float max_distance = float(camera.zfar);
	ray.color = vec3(0);
	
	// if (reflection_max_recursion > 1) {
	// 	float reflection = 1.0;
	// 	for (int i = 0; i < reflection_max_recursion; i++) {
	// 		ray.reflector = 0.0;
	// 		traceNV(topLevelAS, gl_RayFlagsOpaqueNV, 0xff, 0, 0, 0, origin, 0.001, direction, max_distance, 0);
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
		traceNV(topLevelAS, gl_RayFlagsOpaqueNV, 0xff, 0, 0, 0, origin, 0.001, direction, max_distance, 0);
		finalColor = ray.color;
	// }
	
	float depth = float(GetDepthBufferFromTrueDistance(ray.distance));
	
	ivec2 coords = ivec2(gl_LaunchIDNV.xy);
	imageStore(depthImage, coords, vec4(depth, 0,0,0));
	imageStore(litImage, coords, vec4(finalColor, 1.0));
}


// #############################################################
// #shader rahit

// void main() {
// 	// Fragment fragment = GetHitFragment(true);
// 	// if (fragment.color.a < 0.01) {
// 	// 	ignoreIntersectionNV();
// 	// } else {
// 	// 	ray.color = mix(ray.color, fragment.color.rgb, fragment.color.a);
// 	// }
// }


#############################################################
#shader rchit

void main() {
	Fragment fragment = GetHitFragment(true);
	
	ApplyStandardShading(fragment.hitPoint, fragment.color.rgb, fragment.normal, /*roughness*/0.5, /*metallic*/0.0);
	
	// ray.color = fragment.color.rgb;
	// ray.distance = gl_HitTNV;
}


#############################################################
#shader rmiss

void main() {
	ray.color = vec3(0);
	ray.distance = 0;
}


#############################################################

#shader shadow.rmiss

layout(location = 2) rayPayloadInNV bool shadowed;

void main() {
	shadowed = false;
}


		// #############################################################

		// #shader sphere.rint

		// // hitAttributeNV Sphere attribs;

		// void main() {
		// 	// // const Sphere sphere = unpackSphere(gl_InstanceCustomIndexNV);
		// 	// const Sphere sphere = unpackSphere(gl_PrimitiveID);
		// 	// const vec3 origin = gl_WorldRayOriginNV;
		// 	// const vec3 direction = gl_WorldRayDirectionNV;
		// 	// const float tMin = gl_RayTminNV;
		// 	// const float tMax = gl_RayTmaxNV;
		
		// 	// const vec3 oc = origin - sphere.pos;
		// 	// const float a = dot(direction, direction);
		// 	// const float b = dot(oc, direction);
		// 	// const float c = dot(oc, oc) - sphere.radius * sphere.radius;
		// 	// const float discriminant = b * b - a * c;

		// 	// if (discriminant >= 0) {
		// 	// 	const float discriminantSqrt = sqrt(discriminant);
		// 	// 	const float t1 = (-b - discriminantSqrt) / a;
		// 	// 	const float t2 = (-b + discriminantSqrt) / a;

		// 	// 	if ((tMin <= t1 && t1 < tMax) || (tMin <= t2 && t2 < tMax)) {
		// 	// 		attribs = sphere;
		// 	// 		reportIntersectionNV((tMin <= t1 && t1 < tMax) ? t1 : t2, 0);
		// 	// 	}
		// 	// }
		// 	}


		// #############################################################

		// #shader sphere.rchit

		// // hitAttributeNV Sphere sphere;

		// void main() {
		// 	// // Hit World Position
		// 	// const vec3 hitPoint = gl_WorldRayOriginNV + gl_WorldRayDirectionNV * gl_HitTNV;
		// 	// // Hit object Position
		// 	// const vec3 objPoint = hitPoint - sphere.pos;
		// 	// // Normal
		// 	// const vec3 normal = objPoint / sphere.radius;
		
		// 	// // if (sphere.metallic == 0.0) {
		// 	// // 	float r = (snoise(vec4(objPoint, ubo.time / 60.0) * 10.0, 30) / 2.0 + 0.5) * 1.5;
		// 	// // 	float g = (snoise(vec4(objPoint, ubo.time / 60.0) * 5.0, 20) / 2.0 + 0.5) * min(r, 0.9);
		// 	// // 	float b = (snoise(vec4(objPoint, ubo.time / 60.0) * 15.0, 20) / 2.0 + 0.5) * min(g, 0.7);
		// 	// // 	ray.color = vec3(max(r + g + b, 0.3), min(r - 0.2, max(g, b + 0.3)), b) * 1.8;
		// 	// // } else {
		// 	// ApplyStandardShading(hitPoint, objPoint, sphere.color, normal, sphere.emissive, sphere.roughness, sphere.specular, sphere.metallic);
		// 	// // }
		// }

