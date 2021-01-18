#define RAY_TRACING
#include "v4d/modules/V4D_raytracing/glsl_includes/set0.glsl"


#############################################################
#shader rgen

#include "v4d/modules/V4D_raytracing/glsl_includes/set1_rendering.glsl"

layout(location = RAY_PAYLOAD_LOCATION_VISIBILITY) rayPayloadEXT VisibilityPayload ray;
layout(location = CALL_DATA_LOCATION_MATERIAL) callableDataEXT ProceduralMaterialCall mat;

vec2 GetSubSample(uint frameIndex, ivec2 renderSize) {
	uint seed = InitRandomSeed(frameIndex, InitRandomSeed(gl_LaunchIDEXT.x, gl_LaunchIDEXT.y));
	vec2 subPixel = vec2(RandomFloat(seed), RandomFloat(seed));
	return (subPixel * 2.0 - 1.0) / vec2(renderSize.x, renderSize.y);
}

void ExecuteMaterial(inout VisibilityPayload ray, inout vec4 dstColor) {
	Material material = GetGeometry(ray.entityInstanceIndex, ray.geometryIndex).material;
	
	mat.color = ray.color;
	mat.position = ray.position;
	mat.normal = ray.normal;
	mat.entityInstanceIndex = ray.entityInstanceIndex;
	mat.geometryIndex = ray.geometryIndex;
	mat.randomSeed = ray.randomSeed;
	mat.rayDirection = ray.rayDirection;
	
	// Apply material
	executeCallableEXT(material.visibility.rcall_material, CALL_DATA_LOCATION_MATERIAL);
	ray.rayDirection = mat.rayDirection;
	ray.randomSeed = mat.randomSeed;
	
	// Debug
	switch (camera.renderMode) {
		case RENDER_MODE_NORMALS: 
			dstColor = vec4(normalize(GetModelNormalViewMatrix(mat.entityInstanceIndex, mat.geometryIndex) * mat.normal.xyz) * camera.renderDebugScaling, 1);
			return;
		case RENDER_MODE_ALBEDO:
		case RENDER_MODE_EMISSION: 
		case RENDER_MODE_METALLIC:
		case RENDER_MODE_ROUGNESS: 
			dstColor = mat.color;
			return;
	}
	
	// Apply rendering equation
	float cos_theta = dot(mat.rayDirection.xyz, mat.normal.xyz);
	float p = 2.0 * 3.14159265359;
	dstColor.rgb = (mat.emission*mat.color.rgb/max(1,ray.position.w*ray.position.w) + mat.color.rgb*dstColor.rgb*cos_theta/p);
}

void main() {
	const uint64_t startTime = clockARB();
	ivec2 renderSize = imageSize(img_lit);
	const ivec2 imgCoords = ivec2(gl_LaunchIDEXT.xy);
	const ivec2 pixelInMiddleOfScreen = ivec2(gl_LaunchSizeEXT.xy) / 2;
	const bool isMiddleOfScreen = (imgCoords == pixelInMiddleOfScreen);
	const vec2 pixelCenter = vec2(gl_LaunchIDEXT.xy) + vec2(0.5);
	const vec2 d = pixelCenter/vec2(gl_LaunchSizeEXT.xy) * 2.0 - 1.0;

	float primaryRayDepth = 0;
	vec4 primaryRayViewPos = vec4(0);
	vec4 primaryRayAlbedo = vec4(0);
	vec3 primaryRayViewSpaceNormal = vec3(0);
	vec3 primaryRayLocalPos = vec3(0);
	float primaryRayDistance = 0;
	int primaryRayInstanceIndex = -1;
	int primaryRayGeometryIndex = -1;
	int primaryRayPrimitiveIndex = -1;
	vec4 color = vec4(1,1,1,0);
	
	dmat4 projection = camera.projectionMatrix;
	if (camera.accumulateFrames > 0) {
		vec2 subSample = GetSubSample(camera.frameCount, renderSize) * 0.85;
		projection[2].x = subSample.x;
		projection[2].y = subSample.y;
	} else if (camera.denoise > 0) {
		vec2 subSample = GetSubSample(camera.frameCount, renderSize) * 0.65;
		projection[2].x = subSample.x;
		projection[2].y = subSample.y;
	}
	vec3 rayOrigin = vec3(0);
	vec3 rayDirection = normalize(vec4(inverse(projection) * dvec4(d.x, d.y, 1, 1)).xyz);
	float rayMinDistance = GetOptimalBounceStartDistance(0);
	float rayMaxDistance = float(camera.zfar);
	
	// Trace Rays
	InitRayPayload(ray);
	do {
		uint nbBounces = ray.bounces;
		uint rayTraceMask = RAY_TRACED_ENTITY_DEFAULT|RAY_TRACED_ENTITY_TERRAIN|RAY_TRACED_ENTITY_TERRAIN_NEGATE|RAY_TRACED_ENTITY_LIQUID|RAY_TRACED_ENTITY_ATMOSPHERE|RAY_TRACED_ENTITY_FOG;
		if (nbBounces == 0) {
			rayTraceMask |= RAY_TRACED_ENTITY_LIGHT;
		}
		// Trace Ray
		traceRayEXT(topLevelAS, 0, rayTraceMask, RAY_SBT_OFFSET_VISIBILITY, 0, 0, rayOrigin, rayMinDistance, rayDirection, rayMaxDistance, RAY_PAYLOAD_LOCATION_VISIBILITY);
		++ray.bounces;
		
		bool hasHitGeometry = ray.position.w > 0 && ray.entityInstanceIndex != -1 && ray.geometryIndex != -1;
		
		if (nbBounces == 0) {
			primaryRayInstanceIndex = ray.entityInstanceIndex;
			primaryRayGeometryIndex = ray.geometryIndex;
			primaryRayDistance = ray.position.w;
			primaryRayPrimitiveIndex = ray.primitiveID;
			if (hasHitGeometry) {
				primaryRayViewPos.xyz = (GetModelViewMatrix(primaryRayInstanceIndex, primaryRayGeometryIndex) * vec4(ray.position.xyz, 1)).xyz;
				primaryRayViewPos.w = primaryRayDistance;
				primaryRayViewSpaceNormal = normalize(GetModelNormalViewMatrix(primaryRayInstanceIndex, primaryRayGeometryIndex) * ray.normal.xyz);
			}
			primaryRayDepth = primaryRayDistance==0?0:clamp(GetFragDepthFromViewSpacePosition(primaryRayViewPos.xyz), 0, 1);
			primaryRayLocalPos = ray.position.xyz;
			primaryRayAlbedo = ray.color;
			
			// Store raycast info
			if (isMiddleOfScreen) {
				if (hasHitGeometry) {
					// write obj info in hit raycast
					RenderableEntityInstance entity = GetRenderableEntityInstance(primaryRayInstanceIndex);
					rayCast.moduleVen = entity.moduleVen;
					rayCast.moduleId = entity.moduleId;
					rayCast.objId = entity.objId;
					rayCast.raycastCustomData = ray.customData;
					rayCast.localSpaceHitPositionAndDistance = ray.position;
					rayCast.localSpaceHitSurfaceNormal = vec4(ray.normal.xyz, 0);
				} else {
					// write empty hit raycast
					rayCast.moduleVen = 0;
					rayCast.moduleId = 0;
					rayCast.objId = 0;
					rayCast.raycastCustomData = 0;
					rayCast.localSpaceHitPositionAndDistance = vec4(0);
					rayCast.localSpaceHitSurfaceNormal = vec4(0);
				}
			}
			
			// Debug / Render Modes
			switch (camera.renderMode) {
				case RENDER_MODE_NOTHING:
					imageStore(img_lit, imgCoords, vec4(0));
					return;
				case RENDER_MODE_NORMALS:
					if (!hasHitGeometry) {
						imageStore(img_lit, imgCoords, vec4(vec3(0), 1));
						return;
					}
					break;
				case RENDER_MODE_ALBEDO:
					if (!hasHitGeometry) {
						imageStore(img_lit, imgCoords, vec4(vec3(0), 1));
						return;
					}
					break;
				case RENDER_MODE_EMISSION:
					if (!hasHitGeometry) {
						imageStore(img_lit, imgCoords, vec4(vec3(0), 1));
						return;
					}
					break;
				case RENDER_MODE_METALLIC:
					if (!hasHitGeometry) {
						imageStore(img_lit, imgCoords, vec4(1,0,1, 1));
						return;
					}
					break;
				case RENDER_MODE_ROUGNESS:
					if (!hasHitGeometry) {
						imageStore(img_lit, imgCoords, vec4(1,0,1, 1));
						return;
					}
					break;
				case RENDER_MODE_DEPTH:
					imageStore(img_lit, imgCoords, vec4(vec3(primaryRayDepth*pow(10, camera.renderDebugScaling*6)), 1));
					return;
				case RENDER_MODE_DISTANCE:
					imageStore(img_lit, imgCoords, vec4(vec3(primaryRayDistance/pow(10, camera.renderDebugScaling*4)), 1));
					return;
			}
		}
		
		// Miss
		if (!hasHitGeometry) {
			color.rgb = mix(mix(ray.color.rgb, color.rgb*ray.fog.rgb, ray.fog.a), vec3(1), color.a);
			break;
		}
		
		// Material
		ExecuteMaterial(ray, color);
		
		// Fog
		color.rgb = mix(color.rgb, color.rgb*ray.fog.rgb, ray.fog.a);
		ray.fog = vec4(0);
	
		// Debug / Render Modes
		switch (camera.renderMode) {
			case RENDER_MODE_NORMALS:
			case RENDER_MODE_ALBEDO:
			case RENDER_MODE_EMISSION:
			case RENDER_MODE_METALLIC:
			case RENDER_MODE_ROUGNESS:
				imageStore(img_lit, imgCoords, color);
				return;
		}
		
		// // all light has been absorbed
		// if (color.a >= 1.0) {
		// 	break;
		// }
		
		// no more bounce
		if (ray.rayDirection.w <= 0) {
			break;
		}
		
		// maximum bounces reached
		if (ray.bounces > camera.maxBounces && camera.maxBounces >= 0) {
			break;
		}
		
		// maximum time budget exceeded for bounces
		if (camera.bounceTimeBudget > 0.0 && float(double(clockARB() - startTime)/1000000.0) > camera.bounceTimeBudget) {
			break;
		}
		
		// Next bounce
		rayOrigin += rayDirection * ray.position.w;
		rayDirection = normalize(GetModelNormalViewMatrix(ray.entityInstanceIndex, ray.geometryIndex) * ray.rayDirection.xyz);
		rayMinDistance = GetOptimalBounceStartDistance(ray.normal.w);
		rayMaxDistance = ray.rayDirection.w;

	} while (ray.bounces < 1000);

	// Write lit image
	if (camera.renderMode == RENDER_MODE_BOUNCES) {
		imageStore(img_lit, imgCoords, vec4(heatmap(float(ray.bounces)/(camera.renderDebugScaling*5)), 1));
		return;
	} else if (camera.renderMode == RENDER_MODE_TIME) {
		imageStore(img_lit, imgCoords, vec4(heatmap(float(double(clockARB() - startTime) / double(camera.renderDebugScaling*1000000.0))), 1));
		return;
	}
	
	// Accumulation mode
	if (camera.accumulateFrames > 0) {
		color = mix(texture(img_lit_history, (vec2(imgCoords)+0.5)/renderSize), color, 1.0/camera.accumulateFrames);
	}
	
	// Spatiotemporal Reprojection & Denoising algorithms
	if (camera.accumulateFrames <= 0 && camera.denoise > 0) {
		// V4D custom denoiser algo
		if (primaryRayInstanceIndex != -1 && primaryRayGeometryIndex != -1) {
			Material material = GetGeometry(primaryRayInstanceIndex, primaryRayGeometryIndex).material;
			if (material.visibility.roughness > 0) {
				vec4 clipSpaceCoords = mat4(projection) * GetModelViewMatrix_history(primaryRayInstanceIndex, primaryRayGeometryIndex) * vec4(primaryRayLocalPos, 1);
				vec3 posNDC = clipSpaceCoords.xyz/clipSpaceCoords.w;
				vec2 reprojectedCoords = posNDC.xy / 2 + 0.5;
				if (reprojectedCoords.x >= 0 && reprojectedCoords.x <= 1.0 && reprojectedCoords.y >= 0 && reprojectedCoords.y <= 1.0) {
					const int NBSAMPLES = 9;
					const vec2 samples[NBSAMPLES] = {
						vec2( 0,  0) / vec2(renderSize),
						vec2( 1,  0) / vec2(renderSize),
						vec2( 0,  1) / vec2(renderSize),
						vec2(-1,  0) / vec2(renderSize),
						vec2( 0, -1) / vec2(renderSize),
						vec2( 1,  1) / vec2(renderSize),
						vec2(-1,  1) / vec2(renderSize),
						vec2( 1, -1) / vec2(renderSize),
						vec2(-1, -1) / vec2(renderSize),
					};
					vec4 geometryHistory;
					for (int i = 0; i < NBSAMPLES; ++i) {
						const vec2 offset = samples[i];
						geometryHistory = texture(img_geometry_history, reprojectedCoords + offset);
						if (round(geometryHistory.w) == primaryRayInstanceIndex) {
							if (distance(geometryHistory.xyz, primaryRayLocalPos.xyz) < primaryRayDistance) {
								vec4 historyColor = texture(img_lit_history, reprojectedCoords + offset);
								if (dot(normalize(historyColor), normalize(color)) > 0.8 || distance(historyColor, color) < 3) {
									color = mix(historyColor, color, 1.0 / max(1, camera.denoise * (float(material.visibility.roughness)/255)));
								}
								break;
							}
						}
					}
				}
			}
		}
	}
	
	imageStore(img_lit, imgCoords, color);
	imageStore(img_depth, imgCoords, vec4(primaryRayDepth, primaryRayDistance, 0,0));
	imageStore(img_pos, imgCoords, primaryRayViewPos);
	imageStore(img_geometry, imgCoords, vec4(primaryRayLocalPos, float(primaryRayInstanceIndex)));
	imageStore(img_albedo, imgCoords, primaryRayAlbedo);
	imageStore(img_normal, imgCoords, vec4(primaryRayViewSpaceNormal, 0));
}


#############################################################
#shader rmiss

layout(location = RAY_PAYLOAD_LOCATION_VISIBILITY) rayPayloadInEXT VisibilityPayload ray;

void main() {
	ray.position = vec4(0);
	ray.normal = vec4(0);
	ray.rayDirection = vec4(0);
	ray.entityInstanceIndex = -1;
	ray.geometryIndex = -1;
	ray.primitiveID = -1;
	ray.extra = 0;
	ray.customData = 0;
	
	ray.color = vec4(0); //TODO sample galaxy
}


#############################################################
#shader void.rmiss

void main() {}

