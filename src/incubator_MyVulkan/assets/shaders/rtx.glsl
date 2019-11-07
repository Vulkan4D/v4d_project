#version 460
#extension GL_NV_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : enable

// Ray Tracing Payload
struct RayPayload {
	vec3 color;
	vec3 origin;
	vec3 direction;
	float reflector;
	float distance;
};

// Layout Bindings
layout(binding = 0, set = 0) uniform accelerationStructureNV topLevelAS;
layout(binding = 2, set = 0) uniform UBO {
	mat4 view;
	mat4 proj;
    vec4 light;
	int rtx_reflection_max_recursion;
} ubo;
layout(binding = 1, set = 0, rgba8) uniform image2D image;
layout(binding = 3, set = 0) buffer Vertices { vec4 vertexBuffer[]; };
layout(binding = 4, set = 0) buffer Indices { uint indexBuffer[]; };
layout(binding = 5, set = 0) readonly buffer Spheres { vec4 sphereBuffer[]; };

// Vertex
struct Vertex {
	vec3 pos;
	float roughness;
	vec3 normal;
	float reflector;
	vec4 color;
};
uint vertexStructSize = 3;
Vertex unpackVertex(uint index) {
	Vertex v;
	v.pos = vertexBuffer[vertexStructSize * index + 0].xyz;
	v.roughness = vertexBuffer[vertexStructSize * index + 0].w;
	v.normal = vertexBuffer[vertexStructSize * index + 1].xyz;
	v.reflector = vertexBuffer[vertexStructSize * index + 1].w;
	v.color = vertexBuffer[vertexStructSize * index + 2];
	return v;
}

// Sphere
struct Sphere {
	vec3 pos;
	float radius;
	vec4 color;
	float reflector;
	float roughness;
};
uint sphereStructSize = 4;
Sphere unpackSphere(uint index) {
	Sphere s;
	s.reflector = sphereBuffer[sphereStructSize * index + 1].z;
	s.roughness = sphereBuffer[sphereStructSize * index + 1].w;
	s.pos = sphereBuffer[sphereStructSize * index + 2].xyz;
	s.radius = sphereBuffer[sphereStructSize * index + 2].w;
	s.color = sphereBuffer[sphereStructSize * index + 3];
	return s;
}


#############################################################

#shader rgen

layout(location = 0) rayPayloadNV RayPayload ray;

// Max. number of recursion is passed via a specialization constant
// layout (constant_id = 0) const int MAX_RECURSION = 0;

void main() {
	const vec2 pixelCenter = vec2(gl_LaunchIDNV.xy) + vec2(0.5);
	const vec2 inUV = pixelCenter/vec2(gl_LaunchSizeNV.xy);
	const vec2 d = inUV * 2.0 - 1.0;

	const vec3 target = vec4(ubo.proj * vec4(d.x, d.y, 1, 1)).xyz;
	vec3 origin = vec4(ubo.view * vec4(0,0,0,1)).xyz;
	vec3 direction = vec4(ubo.view * vec4(normalize(target), 0)).xyz;
	
	vec3 finalColor = vec3(0.0);
	float reflection = 1.0;
	float max_distance = 10000.0;
	
	for (int i = 0; i < ubo.rtx_reflection_max_recursion; i++) {
		ray.reflector = 0.0;
		traceNV(topLevelAS, gl_RayFlagsOpaqueNV, 0xff, 0, 0, 0, origin, 0.001, direction, max_distance, 0);
		finalColor = mix(finalColor, ray.color, reflection);
		if (ray.reflector <= 0.0) break;
		reflection *= ray.reflector;
		if (reflection < 0.01) break;
		max_distance -= ray.distance;
		if (max_distance <= 0) break;
		origin = ray.origin;
		direction = ray.direction;
	}
	
	imageStore(image, ivec2(gl_LaunchIDNV.xy), vec4(finalColor, 0.0));
}


#############################################################

#shader rchit

// Ray Tracing
layout(location = 0) rayPayloadInNV RayPayload ray;
layout(location = 2) rayPayloadNV bool shadowed;
hitAttributeNV vec3 attribs;

void main() {
	// Hit Triangle vertices
	const vec3 barycentricCoords = vec3(1.0f - attribs.x - attribs.y, attribs.x, attribs.y);
	const ivec3 index = ivec3(indexBuffer[3 * gl_PrimitiveID], indexBuffer[3 * gl_PrimitiveID + 1], indexBuffer[3 * gl_PrimitiveID + 2]);
	const Vertex v0 = unpackVertex(index.x);
	const Vertex v1 = unpackVertex(index.y);
	const Vertex v2 = unpackVertex(index.z);
	
	// Hit World Position
	const vec3 hitPoint = gl_WorldRayOriginNV + gl_WorldRayDirectionNV * gl_HitTNV;
	// Interpolate Vertex data
	const float roughness = v0.roughness * barycentricCoords.x + v1.roughness * barycentricCoords.y + v2.roughness * barycentricCoords.z;
	const vec3 normal = normalize(v0.normal * barycentricCoords.x + v1.normal * barycentricCoords.y + v2.normal * barycentricCoords.z);
	const float reflector = v0.reflector * barycentricCoords.x + v1.reflector * barycentricCoords.y + v2.reflector * barycentricCoords.z;
	const vec4 color = normalize(v0.color * barycentricCoords.x + v1.color * barycentricCoords.y + v2.color * barycentricCoords.z);

	// Basic shading (with light angle)
	const vec3 lightVector = normalize(ubo.light.xyz - hitPoint);
	const float dot_product = max(dot(lightVector, normal), 0.5);
	ray.color = color.rgb * vec3(dot_product) * ubo.light.w;
	
	// Receive Shadows
	shadowed = true;  
	traceNV(topLevelAS, gl_RayFlagsTerminateOnFirstHitNV | gl_RayFlagsOpaqueNV | gl_RayFlagsSkipClosestHitShaderNV, 0xFF, 1, 0, 1, hitPoint, 0.001, lightVector, 10000.0, 2);
	if (shadowed) {
		ray.color *= 0.3;
	}
	
	// Reflections
	ray.distance = gl_RayTmaxNV;
	ray.origin = hitPoint + normal * 0.001f;
	ray.direction = reflect(gl_WorldRayDirectionNV, normal);
	ray.reflector = reflector;
}


#############################################################

#shader rmiss

layout(location = 0) rayPayloadInNV RayPayload ray;

void main() {
	ray.color = vec3(0.7, 0.8, 1.0);
}


#############################################################

#shader shadow.rmiss

layout(location = 2) rayPayloadInNV bool shadowed;

void main() {
	shadowed = false;
}


#############################################################

#shader sphere.rint

hitAttributeNV Sphere attribs;

void main() {
	// const Sphere sphere = unpackSphere(gl_InstanceCustomIndexNV);
	const Sphere sphere = unpackSphere(gl_PrimitiveID);
	const vec3 origin = gl_WorldRayOriginNV;
	const vec3 direction = gl_WorldRayDirectionNV;
	const float tMin = gl_RayTminNV;
	const float tMax = gl_RayTmaxNV;
	
	const vec3 oc = origin - sphere.pos;
	const float a = dot(direction, direction);
	const float b = dot(oc, direction);
	const float c = dot(oc, oc) - sphere.radius * sphere.radius;
	const float discriminant = b * b - a * c;

	if (discriminant >= 0) {
		const float t1 = (-b - sqrt(discriminant)) / a;
		const float t2 = (-b + sqrt(discriminant)) / a;

		if ((tMin <= t1 && t1 < tMax) || (tMin <= t2 && t2 < tMax)) {
			attribs = sphere;
			reportIntersectionNV((tMin <= t1 && t1 < tMax) ? t1 : t2, 0);
		}
	}
}


#############################################################

#shader sphere.rchit

layout(location = 0) rayPayloadInNV RayPayload ray;
layout(location = 2) rayPayloadNV bool shadowed;

hitAttributeNV Sphere sphere;

void main() {
	// Hit World Position
	const vec3 hitPoint = gl_WorldRayOriginNV + gl_WorldRayDirectionNV * gl_HitTNV;
	// Normal
	const vec3 normal = (hitPoint - sphere.pos) / sphere.radius;

	// Basic shading (with light angle)
	const vec3 lightVector = normalize(ubo.light.xyz - hitPoint);
	const float dot_product = max(dot(lightVector, normal), 0.5);
	ray.color = sphere.color.rgb * vec3(dot_product) * ubo.light.w;
	
	// Receive Shadows
	shadowed = true;  
	traceNV(topLevelAS, gl_RayFlagsTerminateOnFirstHitNV | gl_RayFlagsOpaqueNV | gl_RayFlagsSkipClosestHitShaderNV, 0xFF, 1, 0, 1, hitPoint, 0.001, lightVector, 10000.0, 2);
	if (shadowed) {
		ray.color *= 0.3;
	}
	
	// Reflections
	ray.distance = gl_RayTmaxNV;
	ray.origin = hitPoint + normal * 0.001f;
	ray.direction = reflect(gl_WorldRayDirectionNV, normal);
	ray.reflector = sphere.reflector;
}
