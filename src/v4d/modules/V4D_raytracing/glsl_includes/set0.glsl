#ifdef RAY_TRACING
	#extension GL_EXT_ray_tracing : enable
#endif

#extension GL_EXT_buffer_reference : enable
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : enable

#include "core.glsl"
#include "v4d/modules/V4D_raytracing/camera_options.hh"

// Camera
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

// Ray-Tracing descriptor sets
#ifdef RAY_TRACING

	// Top-Level Acceleration Structure
	layout(set = 0, binding = 1) uniform accelerationStructureEXT topLevelAS;

	// RenderableEntityInstances
	#if defined(SHADER_RCHIT) || defined(SHADER_RAHIT) || defined(SHADER_RINT)
		layout(buffer_reference, std430, buffer_reference_align = 4) buffer Indices { uint indices[]; };
		layout(buffer_reference, std430, buffer_reference_align = 4) buffer VertexPositions { float vertexPositions[]; };
		layout(buffer_reference, std430, buffer_reference_align = 4) buffer ProceduralVerticesAABB { float proceduralVerticesAABB[]; };
		layout(buffer_reference, std430, buffer_reference_align = 4) buffer VertexNormals { float vertexNormals[]; };
		layout(buffer_reference, std430, buffer_reference_align = 16) buffer VertexColors { vec4 vertexColors[]; };
		layout(buffer_reference, std430, buffer_reference_align = 8) buffer VertexUVs { vec2 vertexUVs[]; };
		layout(buffer_reference, std430, buffer_reference_align = 16) buffer ModelTransform {
			dmat4 worldTransform;
			mat4 modelView;
			mat4 normalView;
		};
		
		struct RenderableEntityInstance {
			uint64_t moduleVen;
			uint64_t moduleId;
			uint64_t objId;
			uint64_t customData;
			uint64_t indices;
			uint64_t vertexPositions;
			uint64_t vertexNormals;
			uint64_t vertexColors;
			uint64_t vertexUVs;
			uint64_t modelTransform;
		};
		
		layout(set = 0, binding = 2) buffer RenderableEntityInstances { RenderableEntityInstance renderableEntityInstances[]; };
		
		uint GetIndex(uint n) {
			return Indices(renderableEntityInstances[gl_InstanceCustomIndexEXT].indices).indices[3 * gl_PrimitiveID + n];
		}
		vec3 GetVertexPosition(uint index) {
			VertexPositions vertexPositions = VertexPositions(renderableEntityInstances[gl_InstanceCustomIndexEXT].vertexPositions);
			return vec3(vertexPositions.vertexPositions[index*3], vertexPositions.vertexPositions[index*3+1], vertexPositions.vertexPositions[index*3+2]);
		}
		vec3 GetProceduralVertexAABB_min(uint index) {
			ProceduralVerticesAABB proceduralVerticesAABB = ProceduralVerticesAABB(renderableEntityInstances[gl_InstanceCustomIndexEXT].vertexPositions);
			return vec3(proceduralVerticesAABB.proceduralVerticesAABB[index*6], proceduralVerticesAABB.proceduralVerticesAABB[index*6+1], proceduralVerticesAABB.proceduralVerticesAABB[index*6+2]);
		}
		vec3 GetProceduralVertexAABB_max(uint index) {
			ProceduralVerticesAABB proceduralVerticesAABB = ProceduralVerticesAABB(renderableEntityInstances[gl_InstanceCustomIndexEXT].vertexPositions);
			return vec3(proceduralVerticesAABB.proceduralVerticesAABB[index*6+3], proceduralVerticesAABB.proceduralVerticesAABB[index*6+4], proceduralVerticesAABB.proceduralVerticesAABB[index*6+5]);
		}
		vec3 GetVertexNormal(uint index) {
			VertexNormals vertexNormals = VertexNormals(renderableEntityInstances[gl_InstanceCustomIndexEXT].vertexNormals);
			return vec3(vertexNormals.vertexNormals[index*3], vertexNormals.vertexNormals[index*3+1], vertexNormals.vertexNormals[index*3+2]);
		}
		bool HasVertexColor() {
			return renderableEntityInstances[gl_InstanceCustomIndexEXT].vertexColors != 0;
		}
		vec4 GetVertexColor(uint index) {
			return VertexColors(renderableEntityInstances[gl_InstanceCustomIndexEXT].vertexColors).vertexColors[index];
		}
		bool HasVertexUV() {
			return renderableEntityInstances[gl_InstanceCustomIndexEXT].vertexUVs != 0;
		}
		vec2 GetVertexUV(uint index) {
			return VertexUVs(renderableEntityInstances[gl_InstanceCustomIndexEXT].vertexUVs).vertexUVs[index];
		}
		mat4 GetModelViewMatrix() {
			return ModelTransform(renderableEntityInstances[gl_InstanceCustomIndexEXT].modelTransform).modelView;
		}
		mat3 GetModelNormalViewMatrix() {
			return mat3(ModelTransform(renderableEntityInstances[gl_InstanceCustomIndexEXT].modelTransform).normalView);
		}
	#endif

	struct LightSource {
		vec3 position;
		float radius;
		vec3 color;
		float reach;
	};
	layout(set = 0, binding = 3) buffer LightSources { LightSource lightSources[]; };
	
#endif

// Test normal texture
layout(set = 0, binding = 4) uniform sampler2D tex_img_metalNormal;



// Render Options
bool DebugPhysics = (camera.debugOptions & DEBUG_OPTION_PHYSICS)!=0;
bool DebugNormals = (camera.debugOptions & DEBUG_OPTION_NORMALS)!=0;
bool TXAA = (camera.renderOptions & RENDER_OPTION_TXAA)!=0 && !DebugPhysics;
bool HDR = (camera.renderOptions & RENDER_OPTION_HDR_TONE_MAPPING)!=0;
bool GammaCorrection = (camera.renderOptions & RENDER_OPTION_GAMMA_CORRECTION)!=0;
bool HardShadows = (camera.renderOptions & RENDER_OPTION_HARD_SHADOWS)!=0;
bool Reflections = (camera.renderOptions & RENDER_OPTION_REFLECTIONS)!=0;


#ifdef RAY_TRACING

	// 64 bytes
	struct RayTracingPayload {
		vec3 albedo;
		vec3 normal;
		vec3 emission;
		vec3 position;
		float refractionIndex;
		float metallic;
		float roughness;
		float distance;
	};
	
#endif


// double GetTrueDistanceFromDepthBuffer(double depth) {
// 	if (depth == 1) return camera.zfar;
// 	return 2.0 * (camera.zfar * camera.znear) / (camera.znear + camera.zfar - (depth * 2.0 - 1.0) * (camera.znear - camera.zfar));
// }

// double GetDepthBufferFromTrueDistance(double dist) {
// 	if (dist == 0) return 0;
// 	return (((((2.0 * (camera.zfar * camera.znear)) / dist) - camera.znear - camera.zfar) / (camera.zfar - camera.znear)) + 1) / 2.0;
// }

float GetFragDepthFromViewSpacePosition(vec3 viewSpacePos) {
	vec4 clipSpace = mat4(camera.projectionMatrix) * vec4(viewSpacePos, 1);
	return clipSpace.z / clipSpace.w;
}
