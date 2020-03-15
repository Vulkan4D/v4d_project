#version 460
#extension GL_NV_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_buffer_reference : enable

// Settings
const bool shadowsEnabled = true;
const float radianceThreshold = 1e-10;
// const int reflection_max_recursion = 3;

#define NormalBuffer_T vec4 // or pack into vec2 ???

// Descriptor Set 0
#include "Camera.glsl"
layout(set = 0, binding = 1) readonly buffer ObjectBuffer {mat4 objectInstances[];};
layout(set = 0, binding = 2) readonly buffer LightBuffer {float lightSources[];};
layout(set = 0, binding = 3) readonly buffer ActiveLights {
	uint activeLights;
	uint lightIndices[];
};

// Descriptor Set 1
layout(set = 1, binding = 0) uniform accelerationStructureNV topLevelAS;
layout(set = 1, binding = 1, rgba16f) uniform image2D litImage;
layout(set = 1, binding = 2, r32f) uniform image2D depthImage;
layout(set = 1, binding = 3) readonly buffer GeometryBuffer {uvec4 geometries[];};
layout(set = 1, binding = 4) readonly buffer IndexBuffer {uint indices[];};
layout(set = 1, binding = 5) readonly buffer VertexBuffer {vec4 vertices[];};

// Ray Tracing Payload
struct RayPayload {
	vec3 color;
	vec3 origin;
	vec3 direction;
	float distance;
	float opacity;
	// float reflector;
};

// vec3 UnpackNormal(in vec2 norm) {
// 	vec2 fenc = norm * 4.0 - 2.0;
// 	float f = dot(fenc, fenc);
// 	float g = sqrt(1.0 - f / 4.0);
// 	return vec3(fenc * g, 1.0 - f / 2.0);
// }

vec2 UnpackUVfromFloat(in float uv) {
	uint packed = floatBitsToUint(uv);
	return vec2(
		(packed & 0xffff0000) >> 16,
		(packed & 0x0000ffff) >> 0
	) / 65535.0;
}

vec2 UnpackUVfromUint(in uint uv) {
	return vec2(
		(uv & 0xffff0000) >> 16,
		(uv & 0x0000ffff) >> 0
	) / 65535.0;
}

vec4 UnpackColorFromFloat(in float color) {
	uint packed = floatBitsToUint(color);
	return vec4(
		(packed & 0xff000000) >> 24,
		(packed & 0x00ff0000) >> 16,
		(packed & 0x0000ff00) >> 8,
		(packed & 0x000000ff) >> 0
	) / 255.0;
}

vec4 UnpackColorFromUint(in uint color) {
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
	float custom1;
};

LightSource GetLight (uint i) {
	LightSource light;
	light.position = vec3(lightSources[i*8 + 0], lightSources[i*8 + 1], lightSources[i*8 + 2]);
	light.intensity = lightSources[i*8 + 3];
	vec4 c = UnpackColorFromFloat(lightSources[i*8 + 4]);
	light.color = c.rgb;
	light.type = floatBitsToUint(lightSources[i*8 + 4]) & 0x000000ff;
	light.attributes = floatBitsToUint(lightSources[i*8 + 5]);
	light.radius = lightSources[i*8 + 6];
	light.custom1 = lightSources[i*8 + 7];
	return light;
};

struct ObjectInstance {
	mat4 modelViewMatrix;
	mat3 normalMatrix;
	vec3 position;
	vec3 custom3;
	vec4 custom4;
};

ObjectInstance GetObjectInstance(uint index) {
	ObjectInstance obj;
	obj.modelViewMatrix = objectInstances[index*2];
	mat4 secondMatrix = objectInstances[index*2+1];
	obj.normalMatrix = mat3(
		secondMatrix[0].xyz,
		vec3(secondMatrix[0].w, secondMatrix[1].x, secondMatrix[1].y),
		vec3(secondMatrix[1].z, secondMatrix[1].w, secondMatrix[2].x)
	);
	obj.position = obj.modelViewMatrix[3].xyz;
	obj.custom3 = secondMatrix[2].yzw;
	obj.custom4 = objectInstances[index*2+1][3];
	return obj;
}

struct Vertex {
	vec3 pos;
	vec4 color;
	vec3 normal;
	vec2 uv;
};

Vertex GetVertex(uint index) {
	Vertex v;
	v.pos = vertices[index*2].xyz;
	v.color = UnpackColorFromFloat(vertices[index*2].w);
	v.normal = vertices[index*2+1].xyz;
	v.uv = UnpackUVfromFloat(vertices[index*2+1].w);
	return v;
}

struct ProceduralGeometry {
	uint vertexOffset;
	uint objectIndex;
	uint material;
	
	vec3 aabbMin;
	vec3 aabbMax;
	vec4 color;
	float custom1;
	
	ObjectInstance objectInstance;
};

ProceduralGeometry GetProceduralGeometry(uint geometryIndex) {
	ProceduralGeometry geom;
	
	geom.vertexOffset = geometries[geometryIndex].y;
	geom.objectIndex = geometries[geometryIndex].z;
	geom.material = geometries[geometryIndex].w;
	
	geom.aabbMin = vertices[geom.vertexOffset*2].xyz;
	geom.aabbMax = vec3(vertices[geom.vertexOffset*2].w, vertices[geom.vertexOffset*2+1].xy);
	geom.color = UnpackColorFromFloat(vertices[geom.vertexOffset*2+1].z);
	geom.custom1 = vertices[geom.vertexOffset*2+1].w;
	
	geom.objectInstance = GetObjectInstance(geom.objectIndex);
	
	return geom;
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
	uint material;
	ObjectInstance objectInstance;
	Vertex v0;
	Vertex v1;
	Vertex v2;
	vec3 barycentricCoords;
	vec3 hitPoint;
	// Interpolated values
	vec3 pos;
	vec3 normal;
	vec2 uv;
	vec4 color;
};

Fragment GetHitFragment(bool interpolateVertexData) {
	Fragment f;
	
	f.indexOffset = geometries[gl_InstanceCustomIndexNV].x;
	f.vertexOffset = geometries[gl_InstanceCustomIndexNV].y;
	f.objectIndex = geometries[gl_InstanceCustomIndexNV].z;
	f.material = geometries[gl_InstanceCustomIndexNV].w;
	
	f.objectInstance = GetObjectInstance(f.objectIndex);
	
	f.v0 = GetVertex(indices[f.indexOffset + (3 * gl_PrimitiveID) + 0] + f.vertexOffset);
	f.v1 = GetVertex(indices[f.indexOffset + (3 * gl_PrimitiveID) + 1] + f.vertexOffset);
	f.v2 = GetVertex(indices[f.indexOffset + (3 * gl_PrimitiveID) + 2] + f.vertexOffset);
	
	f.barycentricCoords = vec3(1.0f - hitAttribs.x - hitAttribs.y, hitAttribs.x, hitAttribs.y);
	f.hitPoint = gl_WorldRayOriginNV + gl_WorldRayDirectionNV * gl_HitTNV;
	
	if (interpolateVertexData) {
		f.pos = (f.v0.pos * f.barycentricCoords.x + f.v1.pos * f.barycentricCoords.y + f.v2.pos * f.barycentricCoords.z);
		f.normal = normalize(f.v0.normal * f.barycentricCoords.x + f.v1.normal * f.barycentricCoords.y + f.v2.normal * f.barycentricCoords.z);
		// f.normal = normalize(transpose(inverse(mat3(f.objectInstance.modelViewMatrix))) * f.normal);
		f.normal = normalize(f.objectInstance.normalMatrix * f.normal);
		f.uv = (f.v0.uv * f.barycentricCoords.x + f.v1.uv * f.barycentricCoords.y + f.v2.uv * f.barycentricCoords.z);
		f.color = (f.v0.color * f.barycentricCoords.x + f.v1.color * f.barycentricCoords.y + f.v2.color * f.barycentricCoords.z);
	}
	
	return f;
}

#####################################################
#common .*rchit

layout(location = 2) rayPayloadNV bool shadowed;

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

vec3 ViewSpaceNormal(vec3 normal) {
	return normalize(transpose(inverse(mat3(camera.viewMatrix))) * normal);
}

vec3 ApplyStandardShading(vec3 hitPoint, vec3 albedo, vec3 normal, float roughness, float metallic) {
	vec3 color = vec3(0);
	
	// Black backfaces
	if (dot(normal, gl_WorldRayDirectionNV) > 0) {
		return color;
	}
	
	// PBR lighting
	vec3 N = normal;
	vec3 V = normalize(-hitPoint);
	// calculate reflectance at normal incidence; if dia-electric (like plastic) use baseReflectivity of 0.4 and if it's metal, use the albedo color as baseReflectivity
	vec3 baseReflectivity = mix(vec3(0.04), albedo, max(0,metallic));
	
	for (int lightIndex = 0; lightIndex < activeLights; lightIndex++) {
		LightSource light = GetLight(lightIndices[lightIndex]);
		
		// Calculate light radiance at distance
		float dist = length(light.position - hitPoint);
		vec3 radiance = light.color * light.intensity * (1.0 / (dist*dist));
		vec3 L = normalize(light.position - hitPoint);
		
		if (length(radiance) > radianceThreshold) {
			if (shadowsEnabled) {
				shadowed = true;
				if (dot(L, normal) > 0) {
					traceNV(topLevelAS, gl_RayFlagsTerminateOnFirstHitNV | gl_RayFlagsOpaqueNV | gl_RayFlagsSkipClosestHitShaderNV, 0xFF, 0, 0, 1, hitPoint, float(camera.znear), L, length(light.position - hitPoint) - light.radius, 2);
				}
			}
			if (!shadowsEnabled || !shadowed) {
				// cook-torrance BRDF
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
				color += (kD * albedo / PI + specular) * radiance * max(dot(N,L) + max(0, metallic/-5), 0);
				
				// Sub-Surface Scattering (simple rim for now)
				// if (metallic < 0) {
					float rim = pow(1.0 - NdotV, 2-metallic*2+NdotL) * NdotL;
					color += -min(0,metallic) * radiance * rim;
				// }
			}
		}
	}
	
	// // Reflections
	// if (reflection_max_recursion > 1) {
	// 	ray.origin = hitPoint + normal * 0.001f;
	// 	ray.direction = reflect(gl_WorldRayDirectionNV, normal);
	// 	// ray.reflector = clamp(metallic - roughness, 0, 1);
	// }
	
	return color;
}


#############################################################
#shader rgen

void main() {
	const vec2 pixelCenter = vec2(gl_LaunchIDNV.xy) + vec2(0.5);
	const vec2 inUV = pixelCenter/vec2(gl_LaunchSizeNV.xy);
	const vec2 d = inUV * 2.0 - 1.0;
	const vec3 target = vec4(inverse(camera.projectionMatrix) * dvec4(d.x, d.y, 1, 1)).xyz;
	
	vec3 origin = vec3(0); //vec4(inverse(camera.viewMatrix) * dvec4(0,0,0,1)).xyz;
	vec3 direction = normalize(target); //vec4(inverse(camera.viewMatrix) * dvec4(normalize(target), 0)).xyz;
	
	ray.color = vec3(0);
	ray.opacity = 0;
	ray.distance = 0;
	
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
		traceNV(topLevelAS, gl_RayFlagsOpaqueNV, 0xff, 0, 0, 0, origin, float(camera.znear), direction, float(camera.zfar), 0);
	// }
	
	// Manual ray-intersection for light spheres (possibly faster than using traceNV if the number of sphere lights is very low)
	for (int lightIndex = 0; lightIndex < activeLights; lightIndex++) {
		LightSource light = GetLight(lightIndices[lightIndex]);
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
	
	float depth = float(GetDepthBufferFromTrueDistance(ray.distance));
	
	ivec2 coords = ivec2(gl_LaunchIDNV.xy);
	imageStore(depthImage, coords, vec4(depth, 0,0,0));
	imageStore(litImage, coords, vec4(ray.color, 1.0));
}


#############################################################
#shader rahit

void main() {
	// Fragment fragment = GetHitFragment(true);
	// if (fragment.color.a < 0.99) {
	// 	ray.color += fragment.color.rgb * fragment.color.a;
	// }
	
	// ignoreIntersectionNV();
	
	// ray.color += fragment.color.rgb * fragment.color.a;
}


#############################################################
#shader rchit

void main() {
	Fragment fragment = GetHitFragment(true);
	
	vec3 color = ApplyStandardShading(fragment.hitPoint, fragment.color.rgb, fragment.normal, /*roughness*/0.5, /*metallic*/0.0);
	
	// Transparency ?
		// if (ray.opacity < 0.9 && fragment.color.a < 0.99) {
		// 	traceNV(topLevelAS, gl_RayFlagsOpaqueNV, 0xff, 0, 0, 0, fragment.hitPoint, float(camera.znear), ray.direction, float(camera.zfar), 0);
		// }
		// ray.color = mix(ray.color, color, fragment.color.a);
		// ray.opacity += fragment.color.a;
	// or
		ray.color = color;
	
	ray.distance = gl_HitTNV;
}


#############################################################
#shader rmiss

void main() {
	//... may return a background color if other than black
}


#############################################################

#shader shadow.rmiss

layout(location = 2) rayPayloadInNV bool shadowed;

void main() {
	shadowed = false;
}


#############################################################
#shader sphere.rint

hitAttributeNV ProceduralGeometry sphereGeomAttr;

void main() {
	ProceduralGeometry geom = GetProceduralGeometry(gl_InstanceCustomIndexNV);
	vec3 spherePosition = geom.objectInstance.position;
	float sphereRadius = geom.aabbMax.x;
	
	const vec3 origin = gl_WorldRayOriginNV;
	const vec3 direction = gl_WorldRayDirectionNV;
	const float tMin = gl_RayTminNV;
	const float tMax = gl_RayTmaxNV;

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
			reportIntersectionNV((tMin <= t1 && t1 < tMax) ? t1 : t2, 0);
		}
	}
}


#############################################################
#shader sphere.rchit

hitAttributeNV ProceduralGeometry sphereGeomAttr;

void main() {
	vec3 spherePosition = sphereGeomAttr.objectInstance.position;
	float sphereRadius = sphereGeomAttr.aabbMax.x;
	
	// Hit World Position
	const vec3 hitPoint = gl_WorldRayOriginNV + gl_WorldRayDirectionNV * gl_HitTNV;
	const vec3 normal = normalize(hitPoint - spherePosition);
	
	vec3 color = ApplyStandardShading(hitPoint, sphereGeomAttr.color.rgb, normal, /*roughness*/0.5, /*metallic*/0.0);
	
	ray.color = color;
	ray.distance = gl_HitTNV;
}

