#ifdef RAY_TRACING
	#extension GL_EXT_ray_tracing : enable
#endif

#extension GL_EXT_buffer_reference : enable
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int8 : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int16 : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int32 : enable
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
	dmat4 rawProjectionMatrix;
	dmat4 projectionMatrix;
	dmat4 historyViewMatrix;
	mat4 reprojectionMatrix;
	vec2 txaaOffset;
	
	float brightness;
	float contrast;
	float gamma;
	float time;
	
	uint renderMode;
	float renderDebugScaling;
	int maxBounces; // -1 = infinite bounces
	uint frameCount;
	
	vec3 gravityVector;
	
} camera;

// Ray-Tracing descriptor sets
#ifdef RAY_TRACING
	// Top-Level Acceleration Structure
	layout(set = 0, binding = 1) uniform accelerationStructureEXT topLevelAS;
#endif

// RenderableEntityInstances
#if !defined(NO_GLOBAL_BUFFERS) && (defined(SHADER_RGEN) || defined(SHADER_RCHIT) || defined(SHADER_RAHIT) || defined(SHADER_RINT) || defined(SHADER_VERT) || defined(SHADER_FRAG))
	layout(buffer_reference, std430, buffer_reference_align = 2) buffer Indices16 { uint16_t indices16[]; };
	layout(buffer_reference, std430, buffer_reference_align = 4) buffer Indices32 { uint32_t indices32[]; };
	layout(buffer_reference, std430, buffer_reference_align = 4) buffer VertexPositions { float vertexPositions[]; };
	layout(buffer_reference, std430, buffer_reference_align = 4) buffer ProceduralVerticesAABB { float proceduralVerticesAABB[]; };
	layout(buffer_reference, std430, buffer_reference_align = 4) buffer VertexNormals { float vertexNormals[]; };
	layout(buffer_reference, std430, buffer_reference_align = 4) buffer VertexColorsU8 { u8vec4 vertexColorsU8[]; };
	layout(buffer_reference, std430, buffer_reference_align = 8) buffer VertexColorsU16 { u16vec4 vertexColorsU16[]; };
	layout(buffer_reference, std430, buffer_reference_align = 16) buffer VertexColorsF32 { vec4 vertexColorsF32[]; };
	layout(buffer_reference, std430, buffer_reference_align = 8) buffer VertexUVs { vec2 vertexUVs[]; };
	// 32 bytes
	struct Material {
		uint8_t type;
		uint8_t metallic;
		uint8_t roughness;
		uint8_t indexOfRefraction;
		u8vec4 baseColor;
		u8vec4 rim;
		float emission;
		uint8_t textures[8];
		uint8_t texFactors[8];
	};
	// 160 bytes
	struct GeometryInfo {
		mat4 transform;
		uint64_t indices16;
		uint64_t indices32;
		uint64_t vertexPositions;
		uint64_t vertexNormals;
		uint64_t vertexColorsU8;
		uint64_t vertexColorsU16;
		uint64_t vertexColorsF32;
		uint64_t vertexUVs;
		uint64_t customData;
		uint64_t extra;
		Material material;
	};
	layout(buffer_reference, std430, buffer_reference_align = 16) buffer Geometries {
		GeometryInfo geometries[];
	};
	// 96 bytes
	struct RenderableEntityInstance {
		mat4 modelViewTransform;
		uint64_t moduleVen;
		uint64_t moduleId;
		uint64_t objId;
		uint64_t geometries;
	};
	
	layout(set = 0, binding = 2) buffer RenderableEntityInstances { RenderableEntityInstance renderableEntityInstances[]; };
	
	#if defined(SHADER_VERT) || defined(SHADER_FRAG)
		layout(std430, push_constant) uniform RasterPushConstant{
			vec4 wireframeColor;
			int instanceCustomIndexValue;
			uint geometryIndex;
		};
		#define INSTANCE_CUSTOM_INDEX_VALUE instanceCustomIndexValue
		#define GEOMETRY_INDEX_VALUE geometryIndex
		#if defined(SHADER_VERT)
			#define PRIMITIVE_ID_VALUE gl_VertexIndex/3
		#else
			#define PRIMITIVE_ID_VALUE gl_PrimitiveID /*TODO must test and fix this*/
		#endif
	#else
		#if defined(SHADER_RCHIT) || defined(SHADER_RAHIT) || defined(SHADER_RINT)
			#define INSTANCE_CUSTOM_INDEX_VALUE gl_InstanceCustomIndexEXT
			#define GEOMETRY_INDEX_VALUE gl_GeometryIndexEXT
			#define PRIMITIVE_ID_VALUE gl_PrimitiveID
		#endif
	#endif

	#if defined(SHADER_RGEN)
		RenderableEntityInstance GetRenderableEntityInstance(uint entityInstanceIndex) {
			return renderableEntityInstances[entityInstanceIndex];
		}
		GeometryInfo GetGeometry(uint entityInstanceIndex, uint geometryIndex) {
			return Geometries(GetRenderableEntityInstance(entityInstanceIndex).geometries).geometries[geometryIndex];
		}
		mat4 GetModelViewMatrix(uint entityInstanceIndex, uint geometryIndex) {
			return GetRenderableEntityInstance(entityInstanceIndex).modelViewTransform * GetGeometry(entityInstanceIndex, geometryIndex).transform;
		}
		mat3 GetModelNormalViewMatrix(uint entityInstanceIndex, uint geometryIndex) {
			return transpose(inverse(mat3(GetRenderableEntityInstance(entityInstanceIndex).modelViewTransform * GetGeometry(entityInstanceIndex, geometryIndex).transform)));
		}
	#else
		RenderableEntityInstance GetRenderableEntityInstance() {
			return renderableEntityInstances[INSTANCE_CUSTOM_INDEX_VALUE];
		}
		GeometryInfo GetGeometry() {
			return Geometries(GetRenderableEntityInstance().geometries).geometries[GEOMETRY_INDEX_VALUE];
		}
		mat4 GetModelViewMatrix() {
			return GetRenderableEntityInstance().modelViewTransform * mat4(transpose(mat3x4(GetGeometry().transform)));
		}
		mat3 GetModelNormalViewMatrix() {
			return transpose(inverse(mat3(GetRenderableEntityInstance().modelViewTransform * mat4(transpose(mat3x4(GetGeometry().transform))))));
		}
		uint GetIndex(uint n) {
			return GetGeometry().indices16 != 0? 
				uint(Indices16(GetGeometry().indices16).indices16[3 * PRIMITIVE_ID_VALUE + n])
				: (GetGeometry().indices32 != 0? 
					Indices32(GetGeometry().indices32).indices32[3 * PRIMITIVE_ID_VALUE + n]
					: (3 * PRIMITIVE_ID_VALUE + n) /*TODO must test and fix this*/
				)
			;
		}
		vec3 GetVertexPosition(uint index) {
			VertexPositions vertexPositions = VertexPositions(GetGeometry().vertexPositions);
			return vec3(vertexPositions.vertexPositions[index*3], vertexPositions.vertexPositions[index*3+1], vertexPositions.vertexPositions[index*3+2]);
		}
		vec3 GetProceduralVertexAABB_min(uint index) {
			if (GetGeometry().vertexPositions == 0) return vec3(0);
			ProceduralVerticesAABB proceduralVerticesAABB = ProceduralVerticesAABB(GetGeometry().vertexPositions);
			return vec3(proceduralVerticesAABB.proceduralVerticesAABB[index*6], proceduralVerticesAABB.proceduralVerticesAABB[index*6+1], proceduralVerticesAABB.proceduralVerticesAABB[index*6+2]);
		}
		vec3 GetProceduralVertexAABB_max(uint index) {
			if (GetGeometry().vertexPositions == 0) return vec3(0);
			ProceduralVerticesAABB proceduralVerticesAABB = ProceduralVerticesAABB(GetGeometry().vertexPositions);
			return vec3(proceduralVerticesAABB.proceduralVerticesAABB[index*6+3], proceduralVerticesAABB.proceduralVerticesAABB[index*6+4], proceduralVerticesAABB.proceduralVerticesAABB[index*6+5]);
		}
		vec3 GetVertexNormal(uint index) {
			if (GetGeometry().vertexNormals == 0) return vec3(0);
			VertexNormals vertexNormals = VertexNormals(GetGeometry().vertexNormals);
			return vec3(vertexNormals.vertexNormals[index*3], vertexNormals.vertexNormals[index*3+1], vertexNormals.vertexNormals[index*3+2]);
		}
		bool HasVertexColorU8() {
			return GetGeometry().vertexColorsU8 != 0;
		}
		u8vec4 GetVertexColorU8(uint index) {
			return VertexColorsU8(GetGeometry().vertexColorsU8).vertexColorsU8[index];
		}
		bool HasVertexColorU16() {
			return GetGeometry().vertexColorsU16 != 0;
		}
		u16vec4 GetVertexColorU16(uint index) {
			return VertexColorsU16(GetGeometry().vertexColorsU16).vertexColorsU16[index];
		}
		bool HasVertexColorF32() {
			return GetGeometry().vertexColorsF32 != 0;
		}
		vec4 GetVertexColorF32(uint index) {
			return VertexColorsF32(GetGeometry().vertexColorsF32).vertexColorsF32[index];
		}
		bool HasVertexColor() {
			return HasVertexColorU8() || HasVertexColorU16() || HasVertexColorF32();
		}
		vec4 GetVertexColor(uint index) {
			return HasVertexColorF32()? GetVertexColorF32(index) : (HasVertexColorU16()? vec4(GetVertexColorU16(index))/65535 : ( HasVertexColorU8()? vec4(GetVertexColorU8(index))/255 : vec4(0) ));
		}
		bool HasVertexUV() {
			return GetGeometry().vertexUVs != 0;
		}
		vec2 GetVertexUV(uint index) {
			return VertexUVs(GetGeometry().vertexUVs).vertexUVs[index];
		}
		uint64_t GetCustomData() {
			return GetGeometry().customData;
		}
	#endif
	
	#if defined(SHADER_RCHIT) || defined(SHADER_RAHIT) || defined(SHADER_RINT)
		vec3 DoubleSidedNormals(in vec3 viewSpaceNormal) {
			return -sign(dot(viewSpaceNormal, gl_WorldRayDirectionEXT)) * viewSpaceNormal;
		}
	#endif
#endif

struct LightSource {
	vec3 position;
	float radius;
	vec3 color;
	float intensity;
};
layout(set = 0, binding = 3) buffer LightSources { LightSource lightSources[]; };

// Test normal texture
layout(set = 0, binding = 4) uniform sampler2D tex_img_metalNormal;



// Render Options
bool DebugWireframe = (camera.debugOptions & DEBUG_OPTION_WIREFRAME)!=0;
bool DebugPhysics = (camera.debugOptions & DEBUG_OPTION_PHYSICS)!=0;
bool TXAA = (camera.renderOptions & RENDER_OPTION_TXAA)!=0 && camera.renderMode == RENDER_MODE_STANDARD && !DebugPhysics && !DebugWireframe;
bool HDR = (camera.renderOptions & RENDER_OPTION_HDR_TONE_MAPPING)!=0 && camera.renderMode == RENDER_MODE_STANDARD;
bool GammaCorrection = (camera.renderOptions & RENDER_OPTION_GAMMA_CORRECTION)!=0 && camera.renderMode == RENDER_MODE_STANDARD;
bool HardShadows = (camera.renderOptions & RENDER_OPTION_HARD_SHADOWS)!=0 && camera.renderMode == RENDER_MODE_STANDARD;
bool Reflections = (camera.renderOptions & RENDER_OPTION_REFLECTIONS)!=0 && (camera.renderMode == RENDER_MODE_STANDARD || camera.renderMode == RENDER_MODE_BOUNCES);
bool Refraction = (camera.renderOptions & RENDER_OPTION_REFRACTION)!=0 && (camera.renderMode == RENDER_MODE_STANDARD || camera.renderMode == RENDER_MODE_BOUNCES);



#ifdef RAY_TRACING

	#define RAY_PAYLOAD_MAX_IGNORED_INSTANCES 16
	
	#define RAY_PAYLOAD_LOCATION_RENDERING 0
	#define RAY_PAYLOAD_LOCATION_DEPTH 1
	#define RAY_PAYLOAD_LOCATION_SPECTRAL 2
	#define RAY_PAYLOAD_LOCATION_EXTRA 3
	
	#define RAY_SBT_OFFSET_RENDERING 0
	#define RAY_SBT_OFFSET_DEPTH 1
	#define RAY_SBT_OFFSET_SPECTRAL 2
	#define RAY_SBT_OFFSET_EXTRA 3

	struct RayTracingPayload {
		// mandatory
		vec3 albedo;
		vec3 normal;
		float metallic;
		float roughness;
		// optional
		vec3 emission;
		vec3 position;
		float indexOfRefraction;
		float distance;
		int entityInstanceIndex;
		int primitiveID;
		float opacity;
		uint64_t raycastCustomData;
		// float nextRayStartOffset;
		// float nextRayEndOffset;
		int geometryIndex;
		vec4 rim;
		bool passthrough;
		int recursions;
	};

	void InitRayPayload(inout RayTracingPayload ray) {
		// ray.nextRayStartOffset = 0;
		// ray.nextRayEndOffset = 0;
		ray.position = vec3(0);
		ray.emission = vec3(0);
		ray.opacity = 1.0;
		ray.indexOfRefraction = 0.0;
		ray.distance = 0;
		ray.entityInstanceIndex = -1;
		ray.primitiveID = -1;
		ray.geometryIndex = -1;
		ray.raycastCustomData = 0;
		ray.rim = vec4(0);
		ray.passthrough = false;
		ray.recursions = 0;
	}
	
	#if defined(SHADER_RCHIT) || defined(SHADER_RAHIT)
		void WriteRayPayload(inout RayTracingPayload ray) {
			// ray.nextRayStartOffset = 0;
			// ray.nextRayEndOffset = 0;
			ray.position = gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * gl_HitTEXT;
			ray.emission = vec3(0);
			ray.opacity = 1.0;
			ray.indexOfRefraction = 0.0;
			ray.distance = gl_HitTEXT;
			ray.entityInstanceIndex = INSTANCE_CUSTOM_INDEX_VALUE;
			ray.primitiveID = PRIMITIVE_ID_VALUE;
			ray.geometryIndex = GEOMETRY_INDEX_VALUE;
			ray.raycastCustomData = GetCustomData();
			ray.rim = vec4(0);
			ray.passthrough = false;
		}
	#endif
	
#endif


float GetOptimalBounceStartDistance(float distance) {
	return max(float(camera.znear)*2, float(camera.znear)*distance);
}

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
