#version 460
#extension GL_NV_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : enable

layout(binding = 2, set = 0) uniform UBO {
	mat4 view;
	mat4 proj;
    vec4 light;
} ubo;

struct RayPayload {
	vec3 color;
	vec3 origin;
	vec3 direction;
	float reflector;
	float distance;
};

#############################################################

#shader rgen

layout(binding = 0, set = 0) uniform accelerationStructureNV topLevelAS;
layout(binding = 1, set = 0, rgba8) uniform image2D image;

layout(location = 0) rayPayloadNV RayPayload ray;

// Max. number of recursion is passed via a specialization constant
layout (constant_id = 0) const int MAX_RECURSION = 0;

void main() {
	const vec2 pixelCenter = vec2(gl_LaunchIDNV.xy) + vec2(0.5);
	const vec2 inUV = pixelCenter/vec2(gl_LaunchSizeNV.xy);
	vec2 d = inUV * 2.0 - 1.0;

	vec3 target = vec4(ubo.proj * vec4(d.x, d.y, 1, 1)).xyz;
	vec3 origin = vec4(ubo.view * vec4(0,0,0,1)).xyz;
	vec3 direction = vec4(ubo.view * vec4(normalize(target), 0)).xyz;
	
	vec3 finalColor = vec3(0.0);
	float reflection = 1.0;
	float max_distance = 10000.0;
	
	for (int i = 0; i < MAX_RECURSION; i++) {
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

struct Vertex {
	vec3 pos;
	float reflector;
	vec4 color;
	vec3 normal;
	float roughness;
};
uint vertexSize = 3;

// Ray Tracing
layout(location = 0) rayPayloadInNV RayPayload ray;
layout(location = 2) rayPayloadNV bool shadowed;
hitAttributeNV vec3 attribs;

layout(binding = 0, set = 0) uniform accelerationStructureNV topLevelAS;

// Storage Buffers
layout(binding = 3, set = 0) buffer Vertices { vec4 v[]; } vertexBuffer;
layout(binding = 4, set = 0) buffer Indices { uint i[]; } indexBuffer;

Vertex unpackVertex(uint index) {
	Vertex v;
	v.pos = vertexBuffer.v[vertexSize * index + 0].xyz;
	v.reflector = vertexBuffer.v[vertexSize * index + 0].w;
	v.color = vertexBuffer.v[vertexSize * index + 1];
	v.normal = vertexBuffer.v[vertexSize * index + 2].xyz;
	v.roughness = vertexBuffer.v[vertexSize * index + 2].w;
	return v;
}

void main() {
	// Hit Triangle vertices
	const vec3 barycentricCoords = vec3(1.0f - attribs.x - attribs.y, attribs.x, attribs.y);
	ivec3 index = ivec3(indexBuffer.i[3 * gl_PrimitiveID], indexBuffer.i[3 * gl_PrimitiveID + 1], indexBuffer.i[3 * gl_PrimitiveID + 2]);
	Vertex v0 = unpackVertex(index.x);
	Vertex v1 = unpackVertex(index.y);
	Vertex v2 = unpackVertex(index.z);
	
	// Hit World Position
	vec3 hitWorldPos = gl_WorldRayOriginNV + gl_WorldRayDirectionNV * gl_HitTNV;
	
	// Interpolate Vertex data
	vec3 normal = normalize(v0.normal * barycentricCoords.x + v1.normal * barycentricCoords.y + v2.normal * barycentricCoords.z);
	vec4 color = normalize(v0.color * barycentricCoords.x + v1.color * barycentricCoords.y + v2.color * barycentricCoords.z);
	float reflector = v0.reflector * barycentricCoords.x + v1.reflector * barycentricCoords.y + v2.reflector * barycentricCoords.z;

	// Basic shading (with light angle)
	vec3 lightVector = normalize(ubo.light.xyz - hitWorldPos);
	float dot_product = max(dot(lightVector, normal), 0.5);
	ray.color = color.rgb * vec3(dot_product) * ubo.light.w;
	
	// Shadow casting
	shadowed = true;  
	traceNV(topLevelAS, gl_RayFlagsTerminateOnFirstHitNV | gl_RayFlagsOpaqueNV | gl_RayFlagsSkipClosestHitShaderNV, 0xFF, 1, 0, 1, hitWorldPos, 0.001, lightVector, 10000.0, 2);
	if (shadowed) {
		ray.color *= 0.3;
	}
	
	// Reflections
	ray.distance = gl_RayTmaxNV;
	ray.origin = hitWorldPos + normal * 0.001f;
	ray.direction = reflect(gl_WorldRayDirectionNV, normal);
	ray.reflector = reflector;
}


#############################################################

#shader rmiss

layout(location = 0) rayPayloadInNV RayPayload ray;

void main() {
	ray.color = vec3(0.0, 0.0, 0.0);
}


#############################################################

#shader shadow.rmiss

layout(location = 2) rayPayloadInNV bool shadowed;

void main() {
	shadowed = false;
}

