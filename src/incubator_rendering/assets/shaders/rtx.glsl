#version 460
#extension GL_NV_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : enable

#include "Camera.glsl"

// Ray Tracing Payload
struct RayPayload {
	vec3 color;
	vec3 origin;
	vec3 direction;
	float reflector;
	float distance;
};

// layout(set = 0, binding = 2) uniform UBO {
// 	dmat4 view;
// 	dmat4 proj;
//     dvec4 light;
// 	vec3 ambient;
// 	double time;
// 	int samplesPerPixel;
// 	int rtx_reflection_max_recursion;
// 	bool rtx_shadows;
// } ubo;

layout(set = 1, binding = 0, rgba8) uniform image2D litImage;
layout(set = 1, binding = 1) uniform accelerationStructureNV topLevelAS;

// layout(binding = 3, set = 0) buffer Vertices { vec4 vertexBuffer[]; };
// layout(binding = 4, set = 0) buffer Indices { uint indexBuffer[]; };
// layout(binding = 5, set = 0) readonly buffer Spheres { vec4 sphereBuffer[]; };

#####################################################

#common .*rchit

layout(location = 0) rayPayloadInNV RayPayload ray;
// layout(location = 2) rayPayloadNV bool shadowed;

// void ApplyStandardShading(vec3 hitPoint, vec3 objPoint, vec4 color, vec3 normal, float emissive, float roughness, float specular, float metallic) {
	
// 	// Roughness
// 	if (roughness > 0.0) {
// 		float n1 = 1.0 + noise(objPoint.xy * 1000.0 * min(4.0, roughness) + 134.455, 3) * roughness / 2.0;
// 		float n2 = 1.0 + noise(objPoint.yz * 1000.0 * min(4.0, roughness) + 2.5478787, 3) * roughness / 2.0;
// 		float n3 = 1.0 + noise(objPoint.xz * 1000.0 * min(4.0, roughness) + -124.785, 3) * roughness / 2.0;
// 		normal = normalize(normal * vec3(n1, n2, n3)/2.0);
// 	}
	
// 	// Basic shading from light angle
// 	vec3 lightVector = normalize(vec3(ubo.light.xyz) - hitPoint);
// 	const float dot_product = max(dot(lightVector, normal), 0.0);
// 	const float shade = pow(dot_product, specular);
// 	ray.color = mix(ubo.ambient, max(ubo.ambient, color.rgb * float(ubo.light.w)), shade);
	
// 	// Receive Shadows
// 	if (shade > 0.0 && ubo.rtx_shadows) {
// 		shadowed = true;
// 		traceNV(topLevelAS, gl_RayFlagsTerminateOnFirstHitNV | gl_RayFlagsOpaqueNV | gl_RayFlagsSkipClosestHitShaderNV, 0xFF, 0, 0, 1, hitPoint, 0.001, lightVector, length(vec3(ubo.light.xyz) - hitPoint), 2);
// 		if (shadowed) {
// 			ray.color = ubo.ambient;
// 		}
// 	}
	
// 	//TODO opacity
	
// 	// Reflections
// 	if (ubo.rtx_reflection_max_recursion > 1) {
// 		ray.distance = gl_RayTmaxNV;
// 		ray.origin = hitPoint + normal * 0.001f;
// 		ray.direction = reflect(gl_WorldRayDirectionNV, normal);
// 		ray.reflector = metallic;
// 	}
// }


#####################################################

#common sphere.*

// // Sphere
// struct Sphere {
// 	float emissive;
// 	float roughness;
// 	vec3 pos;
// 	float radius;
// 	vec4 color;
// 	float specular;
// 	float metallic;
// 	float refraction;
// 	float density;
// };
// uint sphereStructSize = 5;
// Sphere unpackSphere(uint index) {
// 	Sphere s;
// 	s.emissive = sphereBuffer[sphereStructSize * index + 1].z;
// 	s.roughness = sphereBuffer[sphereStructSize * index + 1].w;
// 	s.pos = sphereBuffer[sphereStructSize * index + 2].xyz;
// 	s.radius = sphereBuffer[sphereStructSize * index + 2].w;
// 	s.color = sphereBuffer[sphereStructSize * index + 3];
// 	s.specular = sphereBuffer[sphereStructSize * index + 4].x;
// 	s.metallic = sphereBuffer[sphereStructSize * index + 4].y;
// 	s.refraction = sphereBuffer[sphereStructSize * index + 4].z;
// 	s.density = sphereBuffer[sphereStructSize * index + 4].w;
// 	return s;
// }


#############################################################

#shader rgen

layout(location = 0) rayPayloadNV RayPayload ray;

void main() {
	const vec2 pixelCenter = vec2(gl_LaunchIDNV.xy) + vec2(0.5);
	const vec2 inUV = pixelCenter/vec2(gl_LaunchSizeNV.xy);
	const vec2 d = inUV * 2.0 - 1.0;

	const vec3 target = vec4(inverse(camera.projectionMatrix) * dvec4(d.x, d.y, 1, 1)).xyz;
	vec3 origin = vec4(inverse(camera.viewMatrix) * dvec4(0,0,0,1)).xyz;
	vec3 direction = vec4(inverse(camera.viewMatrix) * dvec4(normalize(target), 0)).xyz;
	
	vec3 finalColor = vec3(0.0);
	float max_distance = float(camera.zfar);
	
	// if (ubo.rtx_reflection_max_recursion > 1) {
	// 	float reflection = 1.0;
	// 	for (int i = 0; i < ubo.rtx_reflection_max_recursion; i++) {
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
	
	imageStore(litImage, ivec2(gl_LaunchIDNV.xy), vec4(finalColor, 1.0));
}


#############################################################

#shader rchit

hitAttributeNV vec3 attribs;

// // Vertex
// struct Vertex {
// 	vec3 pos;
// 	float roughness;
// 	vec3 normal;
// 	float emissive;
// 	vec4 color;
// 	vec2 uv;
// 	float specular;
// 	float metallic;
// };
// uint vertexStructSize = 4;
// Vertex unpackVertex(uint index) {
// 	Vertex v;
// 	v.pos = vertexBuffer[vertexStructSize * index + 0].xyz;
// 	v.roughness = vertexBuffer[vertexStructSize * index + 0].w;
// 	v.normal = vertexBuffer[vertexStructSize * index + 1].xyz;
// 	v.emissive = vertexBuffer[vertexStructSize * index + 1].w;
// 	v.color = vertexBuffer[vertexStructSize * index + 2];
// 	v.uv = vertexBuffer[vertexStructSize * index + 3].xy;
// 	v.specular = vertexBuffer[vertexStructSize * index + 3].z;
// 	v.metallic = vertexBuffer[vertexStructSize * index + 3].w;
// 	return v;
// }

void main() {
	// // Hit Triangle vertices
	// const vec3 barycentricCoords = vec3(1.0f - attribs.x - attribs.y, attribs.x, attribs.y);
	// const ivec3 index = ivec3(indexBuffer[3 * gl_PrimitiveID], indexBuffer[3 * gl_PrimitiveID + 1], indexBuffer[3 * gl_PrimitiveID + 2]);
	// const Vertex v0 = unpackVertex(index.x);
	// const Vertex v1 = unpackVertex(index.y);
	// const Vertex v2 = unpackVertex(index.z);
	
	// // Hit World Position
	// const vec3 hitPoint = gl_WorldRayOriginNV + gl_WorldRayDirectionNV * gl_HitTNV;
	// // Hit object Position
	// const vec3 objPoint = hitPoint - normalize(v0.pos * barycentricCoords.x + v1.pos * barycentricCoords.y + v2.pos * barycentricCoords.z);
	// // Interpolate Vertex data
	// const vec4 color = normalize(v0.color * barycentricCoords.x + v1.color * barycentricCoords.y + v2.color * barycentricCoords.z);
	// const vec3 normal = normalize(v0.normal * barycentricCoords.x + v1.normal * barycentricCoords.y + v2.normal * barycentricCoords.z);
	// const float emissive = v0.emissive * barycentricCoords.x + v1.emissive * barycentricCoords.y + v2.emissive * barycentricCoords.z;
	// const float roughness = v0.roughness * barycentricCoords.x + v1.roughness * barycentricCoords.y + v2.roughness * barycentricCoords.z;
	// const float specular = v0.specular * barycentricCoords.x + v1.specular * barycentricCoords.y + v2.specular * barycentricCoords.z;
	// const float metallic = v0.metallic * barycentricCoords.x + v1.metallic * barycentricCoords.y + v2.metallic * barycentricCoords.z;

	// ApplyStandardShading(hitPoint, objPoint, color, normal, emissive, roughness, specular, metallic);
	
	ray.color = vec3(1,0,0);
}


#############################################################

#shader rmiss

layout(location = 0) rayPayloadInNV RayPayload ray;

void main() {
	ray.color = vec3(0.0, 0.0, 0.0);
}


#############################################################

#shader shadow.rmiss

// layout(location = 2) rayPayloadInNV bool shadowed;

void main() {
	// shadowed = false;
}


#############################################################

#shader sphere.rint

// hitAttributeNV Sphere attribs;

void main() {
	// // const Sphere sphere = unpackSphere(gl_InstanceCustomIndexNV);
	// const Sphere sphere = unpackSphere(gl_PrimitiveID);
	// const vec3 origin = gl_WorldRayOriginNV;
	// const vec3 direction = gl_WorldRayDirectionNV;
	// const float tMin = gl_RayTminNV;
	// const float tMax = gl_RayTmaxNV;
	
	// const vec3 oc = origin - sphere.pos;
	// const float a = dot(direction, direction);
	// const float b = dot(oc, direction);
	// const float c = dot(oc, oc) - sphere.radius * sphere.radius;
	// const float discriminant = b * b - a * c;

	// if (discriminant >= 0) {
	// 	const float discriminantSqrt = sqrt(discriminant);
	// 	const float t1 = (-b - discriminantSqrt) / a;
	// 	const float t2 = (-b + discriminantSqrt) / a;

	// 	if ((tMin <= t1 && t1 < tMax) || (tMin <= t2 && t2 < tMax)) {
	// 		attribs = sphere;
	// 		reportIntersectionNV((tMin <= t1 && t1 < tMax) ? t1 : t2, 0);
	// 	}
	// }
}


#############################################################

#shader sphere.rchit

// hitAttributeNV Sphere sphere;

void main() {
	// // Hit World Position
	// const vec3 hitPoint = gl_WorldRayOriginNV + gl_WorldRayDirectionNV * gl_HitTNV;
	// // Hit object Position
	// const vec3 objPoint = hitPoint - sphere.pos;
	// // Normal
	// const vec3 normal = objPoint / sphere.radius;
	
	// // if (sphere.metallic == 0.0) {
	// // 	float r = (snoise(vec4(objPoint, ubo.time / 60.0) * 10.0, 30) / 2.0 + 0.5) * 1.5;
	// // 	float g = (snoise(vec4(objPoint, ubo.time / 60.0) * 5.0, 20) / 2.0 + 0.5) * min(r, 0.9);
	// // 	float b = (snoise(vec4(objPoint, ubo.time / 60.0) * 15.0, 20) / 2.0 + 0.5) * min(g, 0.7);
	// // 	ray.color = vec3(max(r + g + b, 0.3), min(r - 0.2, max(g, b + 0.3)), b) * 1.8;
	// // } else {
	// ApplyStandardShading(hitPoint, objPoint, sphere.color, normal, sphere.emissive, sphere.roughness, sphere.specular, sphere.metallic);
	// // }
}
