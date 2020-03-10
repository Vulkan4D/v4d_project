#version 460
#extension GL_NV_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : enable

#extension GL_EXT_buffer_reference : enable

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

layout(set = 1, binding = 0, rgba16f) uniform image2D litImage;
layout(set = 1, binding = 1) uniform accelerationStructureNV topLevelAS;
layout(set = 1, binding = 2) readonly buffer Geometries {uvec4 geometries[];};
layout(set = 1, binding = 3) readonly buffer Indices {uint indices[];};
layout(set = 1, binding = 4) readonly buffer VertexPositions {vec4 vertexPositions[];};
layout(set = 1, binding = 5) readonly buffer VertexMaterials {uint vertexMaterials[];};
layout(set = 1, binding = 6) readonly buffer VertexNormals {vec2 vertexNormals[];};
layout(set = 1, binding = 7) readonly buffer VertexUVs {uint vertexUVs[];};
layout(set = 1, binding = 8) readonly buffer VertexColors {uint vertexColors[];};

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

#common .*rchit|.*rahit

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

void main() {
	Fragment fragment = GetHitFragment(true);
	
	// ApplyStandardShading(hitPoint, objPoint, color, normal, emissive, roughness, specular, metallic);
	
	ray.color = fragment.color.rgb;
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
