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



#ifdef RAY_TRACING
	
	#define RAY_PAYLOAD_LOCATION_VISIBILITY 0
	#define RAY_PAYLOAD_LOCATION_SPECTRAL 1
	#define RAY_PAYLOAD_LOCATION_EXTRA 2
	
	#define RAY_SBT_OFFSET_VISIBILITY 0
	#define RAY_SBT_OFFSET_SPECTRAL 1
	#define RAY_SBT_OFFSET_EXTRA 2
	
	#define CALL_DATA_LOCATION_MATERIAL 0
	#define CALL_DATA_LOCATION_TEXTURE 1
	
	struct VisibilityPayload {
		vec4 color; // rgb = color, a = opacity (straight from vertex data)
		vec4 position; // xyz = local position straight from vertex data, w = hitDistance
		vec4 normal; // xyz = normal (local space), w = totalRayTravelDistance
 		vec2 uv; // local UVs straight from vertex data
		vec4 bounceDirection; // xyz = bounce direction in local space, w = maxDistance for bounced ray
		uint bounces;
		int32_t entityInstanceIndex;
		int32_t geometryIndex;
		int32_t primitiveID;
		uint extra;
		uint64_t customData;
		uint randomSeed;
	};

	void InitRayPayload(inout VisibilityPayload ray) {
		ray.color = vec4(0);
		ray.position = vec4(0);
		ray.normal = vec4(0);
		ray.bounceDirection = vec4(0);
		ray.bounces = 0;
		ray.entityInstanceIndex = -1;
		ray.geometryIndex = -1;
		ray.primitiveID = -1;
		ray.extra = 0;
		ray.customData = 0;
		ray.randomSeed = InitRandomSeed(InitRandomSeed(gl_LaunchIDEXT.x, gl_LaunchIDEXT.y), camera.frameCount);
	}
	
	#if defined(SHADER_RCHIT) || defined(SHADER_RAHIT)
		void WriteRayPayload(inout VisibilityPayload ray) {
			ray.color = vec4(0,0,0, 1);
			ray.position = vec4(gl_ObjectRayOriginEXT + gl_ObjectRayDirectionEXT * gl_HitTEXT, gl_HitTEXT);
			ray.normal = vec4(0, 0, 0, ray.normal.w + gl_HitTEXT);
			ray.bounceDirection = vec4(0);
			ray.entityInstanceIndex = INSTANCE_CUSTOM_INDEX_VALUE;
			ray.geometryIndex = GEOMETRY_INDEX_VALUE;
			ray.primitiveID = PRIMITIVE_ID_VALUE;
			ray.customData = GetCustomData();
		}
	#endif
	
	
	struct ProceduralTextureCall {
		vec4 color; // rgb = color, a = opacity (straight from vertex data)
		vec4 position; // xyz = local position straight from vertex data, w = hitDistance
		vec4 normal; // xyz = normal (local space), w = totalRayTravelDistance
		vec2 uv;
		float metallic;
		float roughness;
		float height;
		float emission;
		float factor;
	};

	struct ProceduralMaterialCall {
		vec4 color; // rgb = color, a = opacity (straight from vertex data)
		vec4 position; // xyz = local position straight from vertex data, w = hitDistance
		vec4 normal; // xyz = normal (local space), w = totalRayTravelDistance
		vec2 uv; // straight from vertex data or custom stuff from rchit shader
		uint entityInstanceIndex;
		uint geometryIndex;
		uint primitiveIndex;
		uint extra;
		uint64_t customData;
		vec4 bounceDirection; // xyz = bounce direction in local space, w = maxDistance for bounced ray
	};

	void DebugMaterial(inout ProceduralMaterialCall mat, in ProceduralTextureCall tex) {
		switch (camera.renderMode) {
			case RENDER_MODE_ALBEDO: {
				mat.color = vec4(tex.color.rgb*camera.renderDebugScaling, 1);
				break;
			}
			case RENDER_MODE_EMISSION: {
				vec3 debugColor = vec3(tex.color.rgb*tex.emission*camera.renderDebugScaling);
				mat.color = vec4(debugColor, 1);
				break;
			}
			case RENDER_MODE_METALLIC: {
				vec3 debugColor = vec3(tex.metallic*camera.renderDebugScaling);
				mat.color = vec4(debugColor, 1);
				break;
			}
			case RENDER_MODE_ROUGNESS: {
				vec3 debugColor = tex.roughness >=0 ? vec3(tex.roughness*camera.renderDebugScaling) : vec3(-tex.roughness*camera.renderDebugScaling,0,0);
				mat.color = vec4(debugColor, 1);
				break;
			}
		}
	}
	
	// 	void DebugRay(inout VisibilityPayload ray, vec3 albedo, vec3 normal, float emission, float metallic, float roughness) {
	// 		// Other Render Modes
	// 		switch (camera.renderMode) {
	// 			case RENDER_MODE_NORMALS: {
	// 				vec3 debugColor = vec3(normal*camera.renderDebugScaling);
	// 				ray.color = vec4(debugColor, 1);
	// 				ray.bounceDirection = vec4(0);
	// 				break;
	// 			}
	// 			case RENDER_MODE_ALBEDO: {
	// 				vec3 debugColor = vec3(albedo*camera.renderDebugScaling);
	// 				ray.color = vec4(debugColor, 1);
	// 				ray.bounceDirection = vec4(0);
	// 				break;
	// 			}
	// 			case RENDER_MODE_EMISSION: {
	// 				vec3 debugColor = vec3(albedo*emission*camera.renderDebugScaling);
	// 				ray.color = vec4(debugColor, 1);
	// 				ray.bounceDirection = vec4(0);
	// 				break;
	// 			}
	// 			case RENDER_MODE_METALLIC: {
	// 				vec3 debugColor = vec3(metallic*camera.renderDebugScaling);
	// 				ray.color = vec4(debugColor, 1);
	// 				ray.bounceDirection = vec4(0);
	// 				break;
	// 			}
	// 			case RENDER_MODE_ROUGNESS: {
	// 				vec3 debugColor = roughness >=0 ? vec3(roughness*camera.renderDebugScaling) : vec3(-roughness*camera.renderDebugScaling,0,0);
	// 				ray.color = vec4(debugColor, 1);
	// 				ray.bounceDirection = vec4(0);
	// 				break;
	// 			}
	// 		}
	// 	}
		
	// 	//https://blog.demofox.org/2017/01/09/raytracing-reflection-refraction-fresnel-total-internal-reflection-and-beers-law/
	// 	float FresnelReflectAmount(float n1, float n2, vec3 normal, vec3 incident, float reflectivity) {
	// 		// Schlick aproximation
	// 		float r0 = (n1-n2) / (n1+n2);
	// 		r0 *= r0;
	// 		float cosX = -dot(normal, incident);
	// 		if (n1 > n2) {
	// 			float n = n1/n2;
	// 			float sinT2 = n*n*(1.0-cosX*cosX);
	// 			// Total internal reflection
	// 			if (sinT2 > 1.0)
	// 				return 1.0;
	// 			cosX = sqrt(1.0-sinT2);
	// 		}
	// 		float x = 1.0-cosX;
	// 		float ret = r0+(1.0-r0)*x*x*x*x*x;
	// 		// adjust reflect multiplier for object reflectivity
	// 		return reflectivity + (1.0-reflectivity) * ret;
	// 	}

	// 	float Schlick(const float cosine, const float indexOfRefraction) {
	// 		float r0 = (1 - indexOfRefraction) / (1 + indexOfRefraction);
	// 		r0 *= r0;
	// 		return r0 + (1 - r0) * pow(1 - cosine, 5);
	// 	}

	// 	void ScatterMetallic(inout VisibilityPayload ray, const float roughness, const vec3 direction, const vec3 normal) {
	// 		ray.bounceDirection = vec4(normalize(reflect(direction, normal) + roughness*roughness*RandomInUnitSphere(ray.seed)), float(camera.zfar));
	// 		ray.normal = normal;
	// 		++ray.bounces;
	// 	}
		
	// 	void ScatterDieletric(inout VisibilityPayload ray, const float indexOfRefraction, const vec3 direction, const vec3 normal) {
	// 		const float dot = dot(direction, normal);
	// 		const vec3 outwardNormal = dot > 0 ? -normal : normal;
	// 		const float niOverNt = dot > 0 ? indexOfRefraction : 1 / indexOfRefraction;
	// 		const float cosine = dot > 0 ? indexOfRefraction * dot : -dot;
	// 		const vec3 refracted = refract(direction, outwardNormal, niOverNt);
	// 		const float reflectProb = refracted != vec3(0) ? Schlick(cosine, indexOfRefraction) : 1;
			
	// 		if (RandomFloat(ray.seed) < reflectProb) {
	// 			ray.bounceDirection = vec4(reflect(direction, normal), float(camera.zfar));
	// 		} else {
	// 			ray.bounceDirection = vec4(refracted, float(camera.zfar));
	// 		}
	// 		ray.normal = normal;
	// 		++ray.bounces;
	// 	}
		
	// 	void ScatterLambertian(inout VisibilityPayload ray, const float roughness, const vec3 normal) {
	// 		ray.color.a = roughness;
	// 		ray.normal = normal;
	// 		ray.specular = 1.0;
	// 		// ray.bounceDirection = vec4(normalize(mix(normal, normal+RandomInUnitSphere(ray.seed), roughness*roughness)), float(camera.zfar));
	// 		++ray.bounces;
	// 	}

	// #endif
	
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
