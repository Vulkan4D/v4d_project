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
	vec4 color = vec4(0);
	
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
	vec3 reflectance = vec3(1);
	
	// Trace Rays
	InitRayPayload(ray);
	uint rayTraceMask = RAY_TRACED_ENTITY_DEFAULT|RAY_TRACED_ENTITY_TERRAIN|RAY_TRACED_ENTITY_TERRAIN_NEGATE|RAY_TRACED_ENTITY_LIQUID|RAY_TRACED_ENTITY_ATMOSPHERE|RAY_TRACED_ENTITY_FOG|RAY_TRACED_ENTITY_LIGHT;
	do {
		uint nbBounces = ray.bounces;
		reflectance = ray.reflectance;
		ray.emission = vec3(0);
		ray.bounceShadowRays = -1;
		ray.atmosphereId = -1;
		// Trace Ray
		traceRayEXT(topLevelAS, 0, rayTraceMask, RAY_SBT_OFFSET_VISIBILITY, 0, RAY_MISS_OFFSET_STANDARD, rayOrigin, rayMinDistance, rayDirection, rayMaxDistance, RAY_PAYLOAD_LOCATION_VISIBILITY);
		++ray.bounces;
		ray.normal.w += ray.position.w;
		
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
						imageStore(img_lit, imgCoords, vec4(ray.emission, 1));
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
		
		// Material
		if (hasHitGeometry) {
			mat.rayPayload = ray;
			mat.emission.rgb = ray.emission;
			
			mat.reflectance.rgb = vec3(0);
			mat.bounceDirection.xyz = vec3(0);
			mat.bounceShadowRays = -1;
			
			// Execute material callable
			Material material = GetGeometry(ray.entityInstanceIndex, ray.geometryIndex).material;
			executeCallableEXT(material.visibility.rcall_material, CALL_DATA_LOCATION_MATERIAL);
			
			ray.emission.rgb = mat.emission.rgb;
			ray.reflectance.rgb *= clamp(mat.reflectance.rgb, 0.0, 1.0);
			ray.rayDirection.xyz = mat.bounceDirection.xyz;
			ray.bounceShadowRays = mat.bounceShadowRays;
			
			ray.color.rgb = mat.rayPayload.color.rgb;
			ray.normal.xyz = mat.rayPayload.normal.xyz;
			ray.bounceMask = mat.rayPayload.bounceMask;
		}
		
		// // divide the emissive brightness by the square of distance ONLY for bounced rays, not for primary rays.
		// if (dot(ray.emission.rgb, ray.emission.rgb) > 0 && ray.bounces > 1 && ray.position.w > 0) {
		// 	ray.emission /= (ray.position.w * ray.position.w);
		// }
		
		// V4D's custom rendering equation
		color.rgb += ray.emission * reflectance;
		
		// Too much fog, we don't see further anyways, stop here
		if (ray.fog.a > 0.95) {
			break;
		}
		
		// Miss
		if (!hasHitGeometry) {
			break;
		}
		
		// Debug
		switch (camera.renderMode) {
			case RENDER_MODE_NORMALS: 
				vec4 debugColor = vec4(normalize(GetModelNormalViewMatrix(ray.entityInstanceIndex, ray.geometryIndex) * ray.normal.xyz) * camera.renderDebugScaling, 1);
				imageStore(img_lit, imgCoords, debugColor);
				return;
			case RENDER_MODE_ALBEDO:
			case RENDER_MODE_EMISSION: 
			case RENDER_MODE_METALLIC:
			case RENDER_MODE_ROUGNESS: 
				imageStore(img_lit, imgCoords, ray.color);
				return;
		}
		
		// Shadow rays
		if (ray.bounceShadowRays != -1) {
			vec3 hitPositionViewSpace = rayOrigin + rayDirection * ray.position.w;
			vec3 surfaceNormalViewSpace = normalize(GetModelNormalViewMatrix(ray.entityInstanceIndex, ray.geometryIndex) * ray.normal.xyz);
			float totalRayDistance = ray.normal.w;
			VisibilityPayload _ray = ray;
			for (int activeLightIndex = 0; activeLightIndex < MAX_ACTIVE_LIGHTS; activeLightIndex++) {
				LightSource light = lightSources[activeLightIndex];
				if (light.radius > 0) {
					vec3 lightPosRelativeToHitPositionViewSpace = light.position - hitPositionViewSpace;
					vec3 randomDirTowardsLightSourceSphere = SoftShadows? normalize(lightPosRelativeToHitPositionViewSpace + RandomInUnitSphere(ray.randomSeed) * light.radius) : normalize(lightPosRelativeToHitPositionViewSpace);
					float distanceSqr = dot(lightPosRelativeToHitPositionViewSpace, lightPosRelativeToHitPositionViewSpace);
					float intensity = light.intensity / distanceSqr;
					if (intensity > 0.001 && dot(surfaceNormalViewSpace, randomDirTowardsLightSourceSphere) > 0) {
						uint rayTraceShadowMask = RAY_TRACED_ENTITY_DEFAULT|RAY_TRACED_ENTITY_TERRAIN|RAY_TRACED_ENTITY_TERRAIN_NEGATE|RAY_TRACED_ENTITY_FOG;
						float opacity = 0;
						vec3 shadowRayStartPosition = hitPositionViewSpace;
						float shadowRayTravelDistance = totalRayDistance;
						do {
							InitRayPayload(ray);
							ray.bounceMask = rayTraceShadowMask;
							traceRayEXT(topLevelAS, 0, rayTraceShadowMask, RAY_SBT_OFFSET_VISIBILITY, 0, RAY_MISS_OFFSET_VOID, shadowRayStartPosition, GetOptimalBounceStartDistance(shadowRayTravelDistance), randomDirTowardsLightSourceSphere, float(camera.zfar), RAY_PAYLOAD_LOCATION_VISIBILITY);
							if (ray.position.w != 0 && ray.entityInstanceIndex != -1 && ray.geometryIndex != -1) {
								opacity += max(0.1, ray.color.a);
								shadowRayStartPosition += randomDirTowardsLightSourceSphere * ray.position.w;
								shadowRayTravelDistance += ray.position.w;
							}
						} while (ray.position.w != 0 && ray.entityInstanceIndex != -1 && ray.geometryIndex != -1 && opacity < 1.0);
						if (opacity < 1.0) {
							float specular = pow(dot(randomDirTowardsLightSourceSphere, surfaceNormalViewSpace), (1.0 - ray.bounceShadowRays)*5 + 1);
							color.rgb += _ray.color.rgb * light.color * intensity * specular * (1.0 - clamp(opacity, 0, 1)) * reflectance;
						}
					}
				}
			}
			ray = _ray;
		}
		
		// No reflection
		if (dot(ray.reflectance, ray.reflectance) == 0 || dot(ray.rayDirection.xyz, ray.rayDirection.xyz) == 0) break;
		
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
		rayDirection = normalize(GetModelNormalViewMatrix(ray.entityInstanceIndex, ray.geometryIndex) * normalize(ray.rayDirection.xyz));
		rayMinDistance = GetOptimalBounceStartDistance(ray.normal.w);
		rayMaxDistance = float(camera.zfar);
		if (ray.bounceMask > 0) {
			rayTraceMask = ray.bounceMask;
		}

	} while (ray.bounces < 1000);

	// Write debug image for bounces or time
	if (camera.renderMode == RENDER_MODE_BOUNCES) {
		imageStore(img_lit, imgCoords, vec4(heatmap(float(ray.bounces)/(camera.renderDebugScaling*5)), 1));
		return;
	} else if (camera.renderMode == RENDER_MODE_TIME) {
		imageStore(img_lit, imgCoords, vec4(heatmap(float(double(clockARB() - startTime) / double(camera.renderDebugScaling*1000000.0))), 1));
		return;
	}
	
	// Apply fog
	if (ray.fog.a > 0) color.rgb = mix(color.rgb, ray.fog.rgb * normalize(min(vec3(0.0001), reflectance)), clamp(ray.fog.a, 0, 1.0));
	
	// Accumulation mode
	if (camera.accumulateFrames > 0) {
		color = mix(texture(img_lit_history, (vec2(imgCoords)+0.5)/renderSize), color, 1.0/camera.accumulateFrames);
	}
	
	// Spatiotemporal Reprojection & Denoising algorithms
	if (camera.accumulateFrames <= 0 && camera.denoise > 0) {
		// V4D custom denoiser algo
		if (primaryRayInstanceIndex != -1 && primaryRayGeometryIndex != -1) {
			// Material material = GetGeometry(primaryRayInstanceIndex, primaryRayGeometryIndex).material;
			// if (material.visibility.roughness > 0) {
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
									color = mix(historyColor, color, 1.0 / max(1, camera.denoise /* * (float(material.visibility.roughness)/255) */ ));
									break;
								}
							}
						}
					}
				}
			// }
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
	ray.color = vec4(0);
	
	ray.emission = vec3(0);
	if (ray.bounces == 0) {
		//TODO sample galaxy
		// ray.emission = vec3(0);
	}
}


#############################################################
#shader void.rmiss

void main() {}

