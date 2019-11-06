#version 460
#extension GL_NV_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : enable

layout(binding = 2, set = 0) uniform UBO {
	mat4 view;
	mat4 proj;
    vec4 light;
} ubo;


#############################################################

#shader rgen

layout(binding = 0, set = 0) uniform accelerationStructureNV topLevelAS;
layout(binding = 1, set = 0, rgba8) uniform image2D image;

layout(location = 0) rayPayloadNV vec3 hitValue;

void main() {
	const vec2 pixelCenter = vec2(gl_LaunchIDNV.xy) + vec2(0.5);
	const vec2 inUV = pixelCenter/vec2(gl_LaunchSizeNV.xy);
	vec2 d = inUV * 2.0 - 1.0;

	vec4 origin = ubo.view * vec4(0,0,0,1);
	vec4 target = ubo.proj * vec4(d.x, d.y, 1, 1) ;
	vec4 direction = ubo.view*vec4(normalize(target.xyz), 0) ;

	uint rayFlags = gl_RayFlagsOpaqueNV;
	uint cullMask = 0xff;
	float tmin = 0.001;
	float tmax = 10000.0;

	traceNV(topLevelAS, rayFlags, cullMask, 0, 0, 0, origin.xyz, tmin, direction.xyz, tmax, 0);

	imageStore(image, ivec2(gl_LaunchIDNV.xy), vec4(hitValue, 0.0));
}


#############################################################

#shader rchit

struct Vertex {
	vec3 pos;
	float reflectiveness;
	vec4 color;
	vec3 normal;
	float roughness;
};
uint vertexSize = 3;

// Ray Tracing
layout(location = 0) rayPayloadInNV vec3 hitValue;
layout(location = 2) rayPayloadNV bool shadowed;
hitAttributeNV vec3 attribs;

layout(binding = 0, set = 0) uniform accelerationStructureNV topLevelAS;

// Storage Buffers
layout(binding = 3, set = 0) buffer Vertices { vec4 v[]; } vertexBuffer;
layout(binding = 4, set = 0) buffer Indices { uint i[]; } indexBuffer;

Vertex unpackVertex(uint index) {
	Vertex v;
	v.pos = vertexBuffer.v[vertexSize * index + 0].xyz;
	v.reflectiveness = vertexBuffer.v[vertexSize * index + 0].w;
	v.color = vertexBuffer.v[vertexSize * index + 1];
	v.normal = vertexBuffer.v[vertexSize * index + 2].xyz;
	v.roughness = vertexBuffer.v[vertexSize * index + 2].w;
	return v;
}

void main() {
	const vec3 barycentricCoords = vec3(1.0f - attribs.x - attribs.y, attribs.x, attribs.y);
	ivec3 index = ivec3(indexBuffer.i[3 * gl_PrimitiveID], indexBuffer.i[3 * gl_PrimitiveID + 1], indexBuffer.i[3 * gl_PrimitiveID + 2]);
	Vertex v0 = unpackVertex(index.x);
	Vertex v1 = unpackVertex(index.y);
	Vertex v2 = unpackVertex(index.z);
	
	// Interpolate normal
	vec3 normal = normalize(v0.normal * barycentricCoords.x + v1.normal * barycentricCoords.y + v2.normal * barycentricCoords.z);

	// Interpolate color
	vec4 color = normalize(v0.color * barycentricCoords.x + v1.color * barycentricCoords.y + v2.color * barycentricCoords.z);

	// Basic lighting
	vec3 lightVector = normalize(ubo.light.xyz);
	float dot_product = max(dot(lightVector, normal), 0.5);
	hitValue = color.rgb * vec3(dot_product) * ubo.light.w;
	
	// Shadow casting
	float tmin = 0.001;
	float tmax = 100.0;
	vec3 origin = gl_WorldRayOriginNV + gl_WorldRayDirectionNV * gl_HitTNV;
	shadowed = true;  
	// Offset indices to match shadow hit/miss index
	traceNV(topLevelAS, gl_RayFlagsTerminateOnFirstHitNV | gl_RayFlagsOpaqueNV | gl_RayFlagsSkipClosestHitShaderNV, 0xFF, 1, 0, 1, origin, tmin, lightVector, tmax, 2);
	if (shadowed) {
		hitValue *= 0.3;
	}
}


#############################################################

#shader rmiss

layout(location = 0) rayPayloadInNV vec3 hitValue;

void main() {
	hitValue = vec3(0.0, 0.0, 0.0);
}


#############################################################

#shader shadow.rmiss

layout(location = 2) rayPayloadInNV bool shadowed;

void main() {
	shadowed = false;
}

