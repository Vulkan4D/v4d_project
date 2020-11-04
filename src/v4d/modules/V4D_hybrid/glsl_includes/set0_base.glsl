#ifndef GEOMETRY_BUFFERS_ACCESS
	#define GEOMETRY_BUFFERS_ACCESS
#endif

#include "v4d/modules/V4D_hybrid/camera_options.hh"

// Descriptor Set 0
layout(set = 0, binding = 0) uniform Camera {
	int width;
	int height;
	uint renderOptions;
	uint debugOptions;
	
	vec4 luminance;
	dvec3 worldPosition;
	double fov;
	dvec3 lookDirection; //TODO remove if not needed
	double znear;
	dvec3 viewUp; //TODO remove if not needed
	double zfar;
	dmat4 viewMatrix;
	dmat4 projectionMatrix;
	dmat4 historyViewMatrix;
	mat4 reprojectionMatrix;
	vec2 txaaOffset;
	
	float brightness;
	float contrast;
	float gamma;
	float time;
	
} camera;
layout(set = 0, binding = 1) GEOMETRY_BUFFERS_ACCESS buffer ObjectBuffer {dmat4 objectInstances[/* stride of 2 dmat4 */];};
layout(set = 0, binding = 2) GEOMETRY_BUFFERS_ACCESS buffer LightBuffer {float lightSources[/* stride of 8 float */];};
layout(set = 0, binding = 3) GEOMETRY_BUFFERS_ACCESS buffer ActiveLights {uint activeLights; uint lightIndices[];};
layout(set = 0, binding = 4) GEOMETRY_BUFFERS_ACCESS buffer GeometryBuffer {float geometries[/* stride of 64 float */];};
layout(set = 0, binding = 5) GEOMETRY_BUFFERS_ACCESS buffer IndexBuffer {uint indices[];};
layout(set = 0, binding = 6) GEOMETRY_BUFFERS_ACCESS buffer VertexBuffer {vec4 vertices[/* stride of 2 vec4 */];};

#if defined(RAY_TRACING) || defined(RAY_QUERY)
	layout(set = 0, binding = 7) uniform accelerationStructureEXT topLevelAS;
#endif

bool DebugWireframe = (camera.debugOptions & DEBUG_OPTION_WIREFRAME)!=0;
bool DebugPhysics = (camera.debugOptions & DEBUG_OPTION_PHYSICS)!=0;
bool TXAA = (camera.renderOptions & RENDER_OPTION_TXAA)!=0 && !DebugWireframe && !DebugPhysics;
bool HDR = (camera.renderOptions & RENDER_OPTION_HDR_TONE_MAPPING)!=0 && !DebugWireframe;
bool GammaCorrection = (camera.renderOptions & RENDER_OPTION_GAMMA_CORRECTION)!=0 && !DebugWireframe;
bool RayTracedVisibility = (camera.renderOptions & RENDER_OPTION_RAY_TRACED_VISIBILITY)!=0 && !DebugWireframe;
bool HardShadows = (camera.renderOptions & RENDER_OPTION_HARD_SHADOWS)!=0 && !DebugWireframe;

double GetTrueDistanceFromDepthBuffer(double depth) {
	if (depth == 1) return camera.zfar;
	return 2.0 * (camera.zfar * camera.znear) / (camera.znear + camera.zfar - (depth * 2.0 - 1.0) * (camera.znear - camera.zfar));
}

double GetDepthBufferFromTrueDistance(double dist) {
	if (dist == 0) return 0;
	return (((((2.0 * (camera.zfar * camera.znear)) / dist) - camera.znear - camera.zfar) / (camera.zfar - camera.znear)) + 1) / 2.0;
}

float GetFragDepthFromViewSpacePosition(vec3 viewSpacePos) {
	vec4 clipSpace = mat4(camera.projectionMatrix) * vec4(viewSpacePos, 1);
	return clipSpace.z / clipSpace.w;
}

// Object Utils

struct GeometryInstance {
	uint indexOffset;
	uint vertexOffset;
	uint objectIndex;
	uint material;
	
	mat4 modelTransform;
	mat4 modelViewTransform;
	mat3 normalViewTransform;
	
	vec3 custom3f;
	mat4 custom4x4f;
	
	//
	vec3 viewPosition;
};

const uint GEOMETRY_BUFFER_STRIDE = 64;

uint GetVertexOffset(uint geometryIndex) {
	return floatBitsToUint(geometries[geometryIndex*GEOMETRY_BUFFER_STRIDE + 1]);
}

uint GetObjectIndex(uint geometryIndex) {
	return floatBitsToUint(geometries[geometryIndex*GEOMETRY_BUFFER_STRIDE + 2]);
}

uint GetMaterial(uint geometryIndex) {
	return floatBitsToUint(geometries[geometryIndex*GEOMETRY_BUFFER_STRIDE + 3]);
}

GeometryInstance GetGeometryInstance(uint index) {
	GeometryInstance geometry;
	uint i = index*GEOMETRY_BUFFER_STRIDE;
	
	geometry.indexOffset = floatBitsToUint(geometries[i++]);
	geometry.vertexOffset = floatBitsToUint(geometries[i++]);
	geometry.objectIndex = floatBitsToUint(geometries[i++]);
	geometry.material = floatBitsToUint(geometries[i++]);
	
	geometry.modelTransform = mat4(
		vec4(geometries[i++], geometries[i++], geometries[i++], geometries[i++]),
		vec4(geometries[i++], geometries[i++], geometries[i++], geometries[i++]),
		vec4(geometries[i++], geometries[i++], geometries[i++], geometries[i++]),
		vec4(geometries[i++], geometries[i++], geometries[i++], geometries[i++])
	);
	geometry.modelViewTransform = mat4(
		vec4(geometries[i++], geometries[i++], geometries[i++], geometries[i++]),
		vec4(geometries[i++], geometries[i++], geometries[i++], geometries[i++]),
		vec4(geometries[i++], geometries[i++], geometries[i++], geometries[i++]),
		vec4(geometries[i++], geometries[i++], geometries[i++], geometries[i++])
	);
	geometry.normalViewTransform = mat3(
		vec3(geometries[i++], geometries[i++], geometries[i++]),
		vec3(geometries[i++], geometries[i++], geometries[i++]),
		vec3(geometries[i++], geometries[i++], geometries[i++])
	);
	
	geometry.custom3f = vec3(geometries[i++], geometries[i++], geometries[i++]);
	
	geometry.custom4x4f = mat4(
		vec4(geometries[i++], geometries[i++], geometries[i++], geometries[i++]),
		vec4(geometries[i++], geometries[i++], geometries[i++], geometries[i++]),
		vec4(geometries[i++], geometries[i++], geometries[i++], geometries[i++]),
		vec4(geometries[i++], geometries[i++], geometries[i++], geometries[i++])
	);
	
	//
	geometry.viewPosition = geometry.modelViewTransform[3].xyz;
	
	return geometry;
}

struct ProceduralGeometry {
	GeometryInstance geometryInstance;
	
	uint vertexOffset;
	uint objectIndex;
	uint material;
	
	vec3 aabbMin;
	vec3 aabbMax;
	vec4 color;
	float custom1;
};

ProceduralGeometry GetProceduralGeometry(uint geometryIndex) {
	ProceduralGeometry geom;
	
	geom.geometryInstance = GetGeometryInstance(geometryIndex);
	
	geom.vertexOffset = geom.geometryInstance.vertexOffset;
	geom.objectIndex = geom.geometryInstance.objectIndex;
	geom.material = geom.geometryInstance.material;
	
	geom.aabbMin = vertices[geom.vertexOffset*2].xyz;
	geom.aabbMax = vec3(vertices[geom.vertexOffset*2].w, vertices[geom.vertexOffset*2+1].xy);
	geom.color = UnpackColorFromFloat(vertices[geom.vertexOffset*2+1].z);
	geom.custom1 = vertices[geom.vertexOffset*2+1].w;
	
	return geom;
}

struct Vertex {
	vec3 pos;
	vec4 color;
	vec3 normal;
	vec2 uv;
	uint customData;
};

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

uvec4 GenerateCustomData(uint objectIndex, uint customType8, uint flags32, uint customData32_1, uint customData32_2) {
	return uvec4((customType8 << 24) | (objectIndex << 1) | 1, flags32, customData32_1, customData32_2);
}
