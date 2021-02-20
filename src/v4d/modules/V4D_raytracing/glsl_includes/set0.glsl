#ifdef RAY_TRACING
	#extension GL_EXT_ray_tracing : enable
	#extension GL_EXT_ray_query : enable
#endif

#extension GL_EXT_buffer_reference : enable
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int8 : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int16 : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int32 : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : enable

#extension GL_ARB_enhanced_layouts : enable
#extension GL_ARB_shader_clock : enable

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
	vec2 historyTxaaOffset;
	
	float brightness;
	float contrast;
	float gamma;
	float time;
	float bounceTimeBudget;
	
	uint renderMode;
	float renderDebugScaling;
	int maxBounces; // -1 = infinite bounces
	uint frameCount;
	int accumulateFrames;
	float denoise;
	
	vec3 gravityVector;
	
} camera;

float GetOptimalBounceStartDistance(float distance) {
	return max(float(camera.znear), float(camera.znear)*distance);
}

// Ray-Tracing descriptor sets
#ifdef RAY_TRACING
	// Top-Level Acceleration Structure
	layout(set = 0, binding = 1) uniform accelerationStructureEXT topLevelAS;
#endif

// RenderableEntityInstances
#if !defined(NO_GLOBAL_BUFFERS) && (defined(SHADER_RGEN) || defined(SHADER_RCHIT) || defined(SHADER_RAHIT) || defined(SHADER_RINT) || defined(SHADER_VERT) || defined(SHADER_FRAG) || defined(SHADER_RCALL))
	layout(buffer_reference, std430, buffer_reference_align = 2) buffer Indices16 { uint16_t indices16[]; };
	layout(buffer_reference, std430, buffer_reference_align = 4) buffer Indices32 { uint32_t indices32[]; };
	layout(buffer_reference, std430, buffer_reference_align = 4) buffer VertexPositions { float vertexPositions[]; };
	layout(buffer_reference, std430, buffer_reference_align = 4) buffer ProceduralVerticesAABB { float proceduralVerticesAABB[]; };
	layout(buffer_reference, std430, buffer_reference_align = 4) buffer VertexNormals { float vertexNormals[]; };
	layout(buffer_reference, std430, buffer_reference_align = 4) buffer VertexColorsU8 { u8vec4 vertexColorsU8[]; };
	layout(buffer_reference, std430, buffer_reference_align = 8) buffer VertexColorsU16 { u16vec4 vertexColorsU16[]; };
	layout(buffer_reference, std430, buffer_reference_align = 16) buffer VertexColorsF32 { vec4 vertexColorsF32[]; };
	layout(buffer_reference, std430, buffer_reference_align = 8) buffer VertexUVs { vec2 vertexUVs[]; };
	
	struct Material_visibility {
		uint32_t rcall_material;
		u8vec4 baseColor;
		uint8_t metallic;
		uint8_t roughness;
		uint8_t indexOfRefraction;
		uint8_t extra;
		float emission;
		uint8_t textures[8];
		uint8_t texFactors[8];
	};
	struct Material_spectral {
		uint32_t rcall_emit;
		uint32_t rcall_absorb;
		float blackBodyRadiator;
		float temperature;
		uint8_t elements[8];
		uint8_t elementRatios[8];
	};
	struct Material_collider {
		uint32_t rcall_collider;
		u8vec4 props;
		float extra1;
		float extra2;
	};
	struct Material_sound {
		uint32_t rcall_props;
		uint32_t rcall_gen;
		u8vec4 props;
		u8vec4 gen;
	};
	struct Material {
		Material_visibility visibility;
		Material_spectral spectral;
		Material_collider collider;
		Material_sound sound;
	};
	
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
	
	struct RenderableEntityInstance {
		mat4 modelViewTransform;
		mat4 modelViewTransform_history;
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

	RenderableEntityInstance GetRenderableEntityInstance(uint entityInstanceIndex) {
		return renderableEntityInstances[entityInstanceIndex];
	}
	GeometryInfo GetGeometry(uint entityInstanceIndex, uint geometryIndex) {
		return Geometries(GetRenderableEntityInstance(entityInstanceIndex).geometries).geometries[geometryIndex];
	}
	mat4 GetModelViewMatrix(uint entityInstanceIndex, uint geometryIndex) {
		return GetRenderableEntityInstance(entityInstanceIndex).modelViewTransform * GetGeometry(entityInstanceIndex, geometryIndex).transform;
	}
	mat4 GetModelViewMatrix_history(uint entityInstanceIndex, uint geometryIndex) {
		return GetRenderableEntityInstance(entityInstanceIndex).modelViewTransform_history * GetGeometry(entityInstanceIndex, geometryIndex).transform;
	}
	mat3 GetModelNormalViewMatrix(uint entityInstanceIndex, uint geometryIndex) {
		return transpose(inverse(mat3(GetRenderableEntityInstance(entityInstanceIndex).modelViewTransform * GetGeometry(entityInstanceIndex, geometryIndex).transform)));
	}
		
	#if !(defined(SHADER_RGEN) || defined(SHADER_RCALL))
		RenderableEntityInstance GetRenderableEntityInstance() {
			return renderableEntityInstances[INSTANCE_CUSTOM_INDEX_VALUE];
		}
		GeometryInfo GetGeometry() {
			return Geometries(GetRenderableEntityInstance().geometries).geometries[GEOMETRY_INDEX_VALUE];
		}
		mat4 GetModelViewMatrix() {
			return GetRenderableEntityInstance().modelViewTransform * mat4(transpose(mat3x4(GetGeometry().transform)));
		}
		mat4 GetModelViewMatrix_history() {
			return GetRenderableEntityInstance().modelViewTransform_history * mat4(transpose(mat3x4(GetGeometry().transform)));
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
		vec3 DoubleSidedNormals(in vec3 localNormal) {
			return -sign(dot(localNormal, gl_ObjectRayDirectionEXT)) * localNormal;
		}
		vec3 DoubleSidedNormals(in vec3 localNormal, in float bias) {
			return -sign(dot(localNormal, gl_ObjectRayDirectionEXT)-bias) * localNormal;
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
// bool TXAA = (camera.renderOptions & RENDER_OPTION_TXAA)!=0 && camera.renderMode == RENDER_MODE_STANDARD && !DebugPhysics && !DebugWireframe;
bool HDR = (camera.renderOptions & RENDER_OPTION_HDR_TONE_MAPPING)!=0 && camera.renderMode == RENDER_MODE_STANDARD;
bool GammaCorrection = (camera.renderOptions & RENDER_OPTION_GAMMA_CORRECTION)!=0 && camera.renderMode == RENDER_MODE_STANDARD;
bool _RenderStandard = camera.renderMode == RENDER_MODE_STANDARD || camera.renderMode == RENDER_MODE_BOUNCES || camera.renderMode == RENDER_MODE_TIME;
bool SoftShadows = ((camera.renderOptions & RENDER_OPTION_SOFT_SHADOWS)!=0 && _RenderStandard) || camera.accumulateFrames >= 0;
bool PathTracing = ((camera.renderOptions & RENDER_OPTION_PATH_TRACING)!=0 && _RenderStandard) || camera.accumulateFrames >= 0;


#ifdef RAY_TRACING
	
	#define RAY_PAYLOAD_LOCATION_VISIBILITY 0
	#define RAY_PAYLOAD_LOCATION_SPECTRAL 1
	#define RAY_PAYLOAD_LOCATION_EXTRA 2
	
	#define RAY_PAYLOAD_LOCATION_COLLISION 0
	
	#define RAY_SBT_OFFSET_VISIBILITY 0
	#define RAY_SBT_OFFSET_SPECTRAL 1
	#define RAY_SBT_OFFSET_EXTRA 2
	
	#define RAY_SBT_OFFSET_COLLISION 0
	
	#define RAY_MISS_OFFSET_STANDARD 0
	#define RAY_MISS_OFFSET_VOID 1
	
	#define RAY_MISS_OFFSET_COLLISION 0
	
	#define CALL_DATA_LOCATION_MATERIAL 0
	#define CALL_DATA_LOCATION_TEXTURE 1
	
	struct VisibilityPayload {
		vec4 color; // rgb = color, a = opacity (straight from vertex data)
		vec4 position; // xyz = local position straight from vertex data, w = hitDistance
		vec4 normal; // xyz = normal (local space), w = totalRayTravelDistance
 		vec2 uv; // local UVs straight from vertex data
		vec4 rayDirection; // xyz = ray direction in object space, w = maxDistance for next ray
		uint bounces;
		int32_t entityInstanceIndex;
		int32_t geometryIndex;
		int32_t primitiveID;
		uint extra;
		uint64_t customData;
		uint randomSeed;
		
		vec3 emission;
		vec3 reflectance;
		float bounceShadowRays; // -1 for no shadow rays, or Roughness value between 0.0 and 1.0
		vec4 fog;
		uint bounceMask;
		int atmosphereId;
	};

	void InitRayPayload(inout VisibilityPayload ray) {
		ray.color = vec4(0);
		ray.position = vec4(0);
		ray.normal = vec4(0);
		ray.rayDirection = vec4(0);
		ray.bounces = 0;
		ray.entityInstanceIndex = -1;
		ray.geometryIndex = -1;
		ray.primitiveID = -1;
		ray.extra = 0;
		ray.customData = 0;
		ray.randomSeed = InitRandomSeed(InitRandomSeed(gl_LaunchIDEXT.x, gl_LaunchIDEXT.y), camera.frameCount);
		
		ray.emission = vec3(0);
		ray.reflectance = vec3(1);
		ray.bounceShadowRays = -1;
		ray.fog = vec4(0);
		ray.bounceMask = 0;
		ray.atmosphereId = -1;
	}
	
	#if defined(SHADER_RCHIT) || defined(SHADER_RAHIT)
		void WriteRayPayload(inout VisibilityPayload ray) {
			ray.color = vec4(0,0,0, 1);
			ray.position = vec4(gl_ObjectRayOriginEXT + gl_ObjectRayDirectionEXT * gl_HitTEXT, gl_HitTEXT);
			ray.normal.xyz = vec3(0);
			ray.rayDirection = vec4(gl_ObjectRayDirectionEXT, 0);
			ray.entityInstanceIndex = INSTANCE_CUSTOM_INDEX_VALUE;
			ray.geometryIndex = GEOMETRY_INDEX_VALUE;
			ray.primitiveID = PRIMITIVE_ID_VALUE;
			ray.customData = GetCustomData();
		}
	#endif
	
	
	struct ProceduralMaterialCall {
		VisibilityPayload rayPayload;
		vec3 reflectance;
		vec3 emission;
		vec3 bounceDirection;
		float bounceShadowRays;
	};

	struct ProceduralTextureCall {
		ProceduralMaterialCall materialPayload;
		vec4 albedo;
		vec3 emission;
		vec3 normal;
		float metallic;
		float roughness;
		float height;
		float factor;
	};

	bool DebugMaterial(in ProceduralTextureCall tex, inout vec4 debugColor) {
		switch (camera.renderMode) {
			case RENDER_MODE_ALBEDO: {
				debugColor = vec4(tex.albedo.rgb*camera.renderDebugScaling, 1);
				return true;
			}
			case RENDER_MODE_EMISSION: {
				debugColor = vec4(tex.emission.rgb*camera.renderDebugScaling, 1);
				return true;
			}
			case RENDER_MODE_METALLIC: {
				debugColor = vec4(vec3(tex.metallic*camera.renderDebugScaling), 1);
				return true;
			}
			case RENDER_MODE_ROUGNESS: {
				debugColor = vec4(vec3(tex.roughness*camera.renderDebugScaling), 1);
				return true;
			}
		}
		return false;
	}
	
#endif


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
