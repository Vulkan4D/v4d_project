#define RAY_TRACING
#include "v4d/modules/V4D_raytracing/glsl_includes/set0.glsl"


#############################################################
#shader rgen

layout(set = 1, binding = 0, rgba16f) uniform image2D img_lit;
layout(set = 1, binding = 1, rg32f) uniform image2D img_depth;
layout(set = 1, binding = 2, rgba32f) uniform image2D img_pos;
layout(set = 1, binding = 3, rgba32f) uniform image2D img_geometry;
layout(set = 1, binding = 4, rgba16f) uniform image2D img_albedo;
layout(set = 1, binding = 5, rgba16f) uniform image2D img_normal;

layout(set = 1, binding = 6) uniform sampler2D img_lit_history;
layout(set = 1, binding = 7) uniform sampler2D img_depth_history;
layout(set = 1, binding = 8) uniform sampler2D img_pos_history;
layout(set = 1, binding = 9) uniform sampler2D img_geometry_history;
layout(set = 1, binding = 10) uniform sampler2D img_albedo_history;
layout(set = 1, binding = 11) uniform sampler2D img_normal_history;

layout(set = 1, binding = 12) buffer writeonly RayCast {
	uint64_t moduleVen;
	uint64_t moduleId;
	uint64_t objId;
	uint64_t raycastCustomData;
	vec4 localSpaceHitPositionAndDistance; // w component is distance
	vec4 localSpaceHitSurfaceNormal; // w component is unused
} rayCast;

layout(location = RAY_PAYLOAD_LOCATION_RENDERING) rayPayloadEXT RenderingPayload ray;
// layout(location = 1) rayPayloadEXT bool shadowed;

// #include "v4d/modules/V4D_raytracing/glsl_includes/core_pbr.glsl"

vec4 SampleBackground(vec3 direction) {
	return vec4(0.0,0.0,0.0,1);
}

vec3 Specular(vec3 rayOrigin, vec3 hitPosition, vec3 hitNormal, inout uint seed) {
	vec3 specularColor = vec3(0);
	// PBR lighting
	float distance = distance(rayOrigin, hitPosition);
	vec3 N = hitNormal;
	vec3 V = normalize(rayOrigin - hitPosition);
	for (int activeLightIndex = 0; activeLightIndex < MAX_ACTIVE_LIGHTS; activeLightIndex++) {
		LightSource light = lightSources[activeLightIndex];
		if (light.radius > 0.0) {
			// Calculate light radiance at distance
			float dist = length(light.position - hitPosition);
			vec3 radiance = light.color * light.intensity / (dist*dist);
			vec3 randomPointInLightSource = light.position + RandomInUnitSphere(seed) * light.radius;
			vec3 L = normalize(randomPointInLightSource - hitPosition);
			const float radianceThreshold = 1e-10;
			if (length(radiance) > radianceThreshold) {
				bool shadowed = false;
				if (HardShadows) {
					if (dot(L, N) > 0) {
						vec3 shadowRayStart = hitPosition + V*GetOptimalBounceStartDistance(distance); // starting shadow ray just outside the surface this way solves precision issues when casting shadows
						rayQueryEXT rayQuery;
						rayQueryInitializeEXT(rayQuery, topLevelAS, gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsOpaqueEXT | gl_RayFlagsSkipClosestHitShaderEXT, RAY_TRACE_MASK_SHADOWS, shadowRayStart, float(camera.znear), L, length(light.position - hitPosition) - light.radius);
						while(rayQueryProceedEXT(rayQuery)){}
						if (rayQueryGetIntersectionTypeEXT(rayQuery, true) != gl_RayQueryCommittedIntersectionNoneEXT) {
							shadowed = true;
						}
					}
				}
				if (!shadowed) {
					specularColor += clamp(dot(N, L), 0,1) * radiance;
				}
			}
		}
	}
	return specularColor;
}

void main() {
	const uint64_t startTime = clockARB();
	ivec2 renderSize = imageSize(img_lit);
	
	const ivec2 imgCoords = ivec2(gl_LaunchIDEXT.xy);
	const ivec2 pixelInMiddleOfScreen = ivec2(gl_LaunchSizeEXT.xy) / 2;
	const bool isMiddleOfScreen = (imgCoords == pixelInMiddleOfScreen);
	const vec2 pixelCenter = vec2(gl_LaunchIDEXT.xy) + vec2(0.5);
	const vec2 d = pixelCenter/vec2(gl_LaunchSizeEXT.xy) * 2.0 - 1.0;
	
	// const int numSamples = 9;
	const int numSamplesPerFrame = 1;
	// const int numFrames = numSamples - numSamplesPerFrame;

	float primaryRayDepth = 0;
	vec4 primaryRayPos = vec4(0);
	vec4 primaryRayAlbedo = vec4(0);
	vec3 primaryRayNormal = vec3(0);
	vec3 primaryRayLocalPos = vec3(0);
	float primaryRayDistance = 0;
	int primaryRayInstanceIndex = -1;
	int primaryRayGeometryIndex = -1;
	int primaryRayPrimitiveIndex = -1;
	
	vec4 finalColor = vec4(0);
	uint seed = InitRandomSeed(InitRandomSeed(gl_LaunchIDEXT.x, gl_LaunchIDEXT.y), camera.frameCount);
	for (int frameSampleIndex = 0; frameSampleIndex < numSamplesPerFrame; ++frameSampleIndex) {
		// uint frameIndex = camera.frameCount % numFrames;
		
		dmat4 projection = camera.projectionMatrix;
		// vec2 subSample = (vec2(RandomFloat(seed), RandomFloat(seed)) * 2.0 - 1.0) / vec2(renderSize.x, renderSize.y);
		// projection[2].x = subSample.x;
		// projection[2].y = subSample.y;
		
		vec3 rayOrigin = vec3(0);
		
		// uint sampleIndex = InitRandomSeed(InitRandomSeed(gl_LaunchIDEXT.x, gl_LaunchIDEXT.y), InitRandomSeed(frameIndex, frameSampleIndex)) % numSamples;
		// vec3 rayDirection = normalize(vec4(inverse(camera.multisampleProjectionMatrices[sampleIndex]) * dvec4(d.x, d.y, 1, 1)).xyz);
		vec3 rayDirection = normalize(vec4(inverse(projection) * dvec4(d.x, d.y, 1, 1)).xyz);
		
		float rayMinDistance = GetOptimalBounceStartDistance(0);
		float rayMaxDistance = float(camera.zfar);
		
		vec4 color = vec4(1,1,1,0);

		// Trace Rays
		InitRayPayload(ray);
		ray.seed = InitRandomSeed(InitRandomSeed(gl_LaunchIDEXT.x, gl_LaunchIDEXT.y), InitRandomSeed(camera.frameCount, frameSampleIndex));
		uint totalRaysTraced = 0;
		do {
			int nbBounces = ray.bounces;
			// Trace Ray
			traceRayEXT(topLevelAS, 0, RAY_TRACE_MASK_VISIBLE, RAY_SBT_OFFSET_RENDERING, 0, 0, rayOrigin, rayMinDistance, rayDirection, rayMaxDistance, RAY_PAYLOAD_LOCATION_RENDERING);
			++totalRaysTraced;
			// Write g-buffers for first ray
			if (frameSampleIndex == 0 && nbBounces == 0) {
				primaryRayPos = ray.position;
				primaryRayDistance = primaryRayPos.w;
				primaryRayDepth = primaryRayDistance==0?0:clamp(GetFragDepthFromViewSpacePosition(ray.position.xyz), 0, 1);
				primaryRayInstanceIndex = ray.entityInstanceIndex;
				primaryRayGeometryIndex = ray.geometryIndex;
				primaryRayPrimitiveIndex = ray.primitiveID;
				primaryRayLocalPos = ray.localPosition;
				primaryRayAlbedo = ray.color;
				primaryRayNormal = ray.normal;
				
				// Store raycast info
				if (isMiddleOfScreen) {
					if (ray.position.w > 0 && primaryRayInstanceIndex != -1 && primaryRayGeometryIndex != -1) {
						// write obj info in hit raycast
						RenderableEntityInstance entity = GetRenderableEntityInstance(primaryRayInstanceIndex);
						rayCast.moduleVen = entity.moduleVen;
						rayCast.moduleId = entity.moduleId;
						rayCast.objId = entity.objId;
						rayCast.raycastCustomData = ray.raycastCustomData;
						rayCast.localSpaceHitPositionAndDistance = vec4(ray.localPosition, ray.position.w);
						rayCast.localSpaceHitSurfaceNormal = vec4(normalize(inverse(GetModelNormalViewMatrix(primaryRayInstanceIndex, primaryRayGeometryIndex)) * ray.normal), 0);
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
						if (primaryRayDistance == 0) {
							imageStore(img_lit, imgCoords, vec4(vec3(0), 1));
						} else {
							imageStore(img_lit, imgCoords, ray.color);
						}
						return;
					case RENDER_MODE_ALBEDO:
						if (primaryRayDistance == 0) {
							imageStore(img_lit, imgCoords, vec4(vec3(0), 1));
						} else {
							imageStore(img_lit, imgCoords, ray.color);
						}
						return;
					case RENDER_MODE_EMISSION:
						if (primaryRayDistance == 0) {
							imageStore(img_lit, imgCoords, vec4(vec3(0), 1));
						} else {
							imageStore(img_lit, imgCoords, ray.color);
						}
						return;
					case RENDER_MODE_METALLIC:
						if (primaryRayDistance == 0) {
							imageStore(img_lit, imgCoords, vec4(1,0,1, 1));
						} else {
							imageStore(img_lit, imgCoords, ray.color);
						}
						return;
					case RENDER_MODE_ROUGNESS:
						if (primaryRayDistance == 0) {
							imageStore(img_lit, imgCoords, vec4(1,0,1, 1));
						} else {
							imageStore(img_lit, imgCoords, ray.color);
						}
						return;
					case RENDER_MODE_DEPTH:
						imageStore(img_lit, imgCoords, vec4(vec3(primaryRayDepth*pow(10, camera.renderDebugScaling*6)), 1));
						return;
					case RENDER_MODE_DISTANCE:
						imageStore(img_lit, imgCoords, vec4(vec3(primaryRayDistance/pow(10, camera.renderDebugScaling*4)), 1));
						return;
				}
			}
			// miss
			if (ray.position.w <= 0) {
				color.rgb *= mix(SampleBackground(rayDirection).rgb, vec3(1), color.a);
				color.a = 1;
				break;
			}
			color.rgb *= mix(ray.color.rgb, vec3(1), color.a);
			if (ray.specular) {
				color.rgb = mix(color.rgb*Specular(rayOrigin, ray.position.xyz, ray.normal, ray.seed), color.rgb, color.a);
			}
			color.a = min(1, max(ray.color.a, color.a));
			// all light has been absorbed
			if (color.a == 1) {
				break;
			}
			// no more bounce
			if (ray.bounceDirection.a <= 0) {
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
			rayDirection = normalize(ray.bounceDirection.xyz);
			rayMinDistance = GetOptimalBounceStartDistance(ray.totalDistance);
			rayMaxDistance = ray.bounceDirection.a;
		} while (totalRaysTraced < 1000);
		
		// Write lit image
		if (camera.renderMode == RENDER_MODE_BOUNCES) {
			if (frameSampleIndex == 0) {
				imageStore(img_lit, imgCoords, vec4(heatmap(float(ray.bounces)/(camera.renderDebugScaling*5)), 1));
			}
			return;
		} else if (camera.renderMode == RENDER_MODE_TIME) {
			if (frameSampleIndex == 0) {
				imageStore(img_lit, imgCoords, vec4(heatmap(float(double(clockARB() - startTime) / double(camera.renderDebugScaling*1000000.0))), 1));
			}
			return;
		}
		
		// finalColor += color / ((frameSampleIndex==0)? 2.0:(2.0*numSamplesPerFrame));
		// finalColor += color / numSamplesPerFrame;
		if (camera.accumulateFrames <= 0) {
			finalColor = mix(finalColor, color, 1.0 / (frameSampleIndex+1));
		} else {
			finalColor = mix(texture(img_lit_history, vec2(imgCoords)/renderSize), color, 1.0/camera.accumulateFrames);
			break;
		}
		
		
		// if (float(double(clockARB() - startTime)/10000.0) > 80.0) break;
	}
	
	// Spatiotemporal Reprojection & Denoising algorithm
	if (primaryRayInstanceIndex != -1 && primaryRayGeometryIndex != -1) {
		vec4 clipSpaceCoords = mat4(camera.projectionMatrix) * GetModelViewMatrix_history(primaryRayInstanceIndex, primaryRayGeometryIndex) * vec4(primaryRayLocalPos, 1);
		vec3 posNDC = clipSpaceCoords.xyz/clipSpaceCoords.w;
		vec2 reprojectedCoords = posNDC.xy / 2 + 0.5;
		if (reprojectedCoords.x >= 0 && reprojectedCoords.x <= 1.0 && reprojectedCoords.y >= 0 && reprojectedCoords.y <= 1.0) {
			vec4 geometryHistory = texture(img_geometry_history, reprojectedCoords);
			if (round(geometryHistory.w) == primaryRayInstanceIndex) {
				if (distance(geometryHistory.xyz, primaryRayLocalPos.xyz) < primaryRayDistance/100.0) {
					float dotN = dot(normalize(texture(img_normal_history, reprojectedCoords).xyz), normalize(primaryRayNormal.xyz));
					if (dotN > 0.5) {
						finalColor = mix(texture(img_lit_history, reprojectedCoords), finalColor, 1.0 / (dotN*dotN*dotN*dotN*50));
					}
				}
			}
		}
	}
	
	imageStore(img_lit, imgCoords, finalColor);
	imageStore(img_depth, imgCoords, vec4(primaryRayDepth, primaryRayDistance, 0,0));
	imageStore(img_pos, imgCoords, primaryRayPos);
	imageStore(img_geometry, imgCoords, vec4(primaryRayLocalPos, float(primaryRayInstanceIndex)));
	imageStore(img_albedo, imgCoords, primaryRayAlbedo);
	imageStore(img_normal, imgCoords, vec4(primaryRayNormal, 0));









	// vec3 litColor = vec3(0);
	// float opacity = 0;
	// int bounces = 0;
	
	// vec3 rayOrigin = vec3(0);
	// vec3 rayDirection = normalize(vec4(inverse(isMiddleOfScreen? camera.rawProjectionMatrix : camera.projectionMatrix) * dvec4(d.x, d.y, 1, 1)).xyz);
	// uint rayMask = RAY_TRACE_MASK_VISIBLE;
	// float rayMinDistance = float(camera.znear);
	// float rayMaxDistance = float(camera.zfar);

	// // Trace Primary Ray
	// InitRayPayload(ray);
	// do {
	// 	traceRayEXT(topLevelAS, 0, rayMask, RAY_SBT_OFFSET_RENDERING, 0, 0, rayOrigin, rayMinDistance, rayDirection, rayMaxDistance, RAY_PAYLOAD_LOCATION_RENDERING);
	// } while (ray.passthrough && ray.recursions++ < 100 && (rayMinDistance = ray.distance) > 0);
	
	// // Store raycast info
	// if (isMiddleOfScreen) {
	// 	if (ray.distance > 0 && ray.entityInstanceIndex != -1 && ray.geometryIndex != -1) {
	// 		// write obj info in hit raycast
	// 		RenderableEntityInstance entity = GetRenderableEntityInstance(ray.entityInstanceIndex);
	// 		rayCast.moduleVen = entity.moduleVen;
	// 		rayCast.moduleId = entity.moduleId;
	// 		rayCast.objId = entity.objId;
	// 		rayCast.raycastCustomData = ray.raycastCustomData;
	// 		rayCast.localSpaceHitPositionAndDistance = vec4((inverse(GetModelViewMatrix(ray.entityInstanceIndex, ray.geometryIndex)) * vec4(ray.position, 1)).xyz, ray.distance);
	// 		rayCast.localSpaceHitSurfaceNormal = vec4(normalize(inverse(GetModelNormalViewMatrix(ray.entityInstanceIndex, ray.geometryIndex)) * ray.normal), 0);
	// 	} else {
	// 		// write empty hit raycast
	// 		rayCast.moduleVen = 0;
	// 		rayCast.moduleId = 0;
	// 		rayCast.objId = 0;
	// 		rayCast.raycastCustomData = 0;
	// 		rayCast.localSpaceHitPositionAndDistance = vec4(0);
	// 		rayCast.localSpaceHitSurfaceNormal = vec4(0);
	// 	}
	// }
	
	// // Debug / Render Modes = NOTHING
	// if (camera.renderMode == RENDER_MODE_NOTHING) {
	// 	imageStore(img_lit, imgCoords, vec4(0));
	// 	imageStore(img_depth, imgCoords, vec4(0));
	// 	return;
	// }
	
	// // Store depth and distance
	// float primaryRayDistance = ray.distance;
	// float depth = ray.distance==0?0:clamp(GetFragDepthFromViewSpacePosition(ray.position), 0, 1);
	// imageStore(img_depth, imgCoords, vec4(depth, primaryRayDistance, 0,0));
	
	// // Other Render Modes
	// switch (camera.renderMode) {
	// 	case RENDER_MODE_STANDARD:
	// 		// Store background when primary ray missed
	// 		if (primaryRayDistance == 0) {
	// 			litColor = SampleBackground(rayDirection).rgb;
	// 			break;
	// 		}
	// 		// Fallthrough
	// 	case RENDER_MODE_BOUNCES:
	// 		if (primaryRayDistance == 0) break;
		
	// 		litColor = ApplyPBRShading(rayOrigin, ray.position, ray.albedo, ray.normal, /*bump*/vec3(0), ray.roughness, ray.metallic, ray.rim) + ray.emission;
	// 		float attenuation = 1;
		
	// 		vec3 rayAlbedo = vec3(1);
			
	// 		while (bounces++ < camera.maxBounces || camera.maxBounces == -1) { // camera.maxBounces(-1) = infinite bounces
				
	// 			// Fresnel effect
	// 			float NdotV = max(dot(ray.normal,normalize(rayOrigin - ray.position)), 0.000001);
	// 			float reflectionStrength = min(0.95, max(0, ray.metallic));
	// 			vec3 baseReflectivity = rayAlbedo * mix(fresnelSchlick(NdotV, mix(vec3(0.04), ray.albedo, reflectionStrength)), vec3(reflectionStrength*ray.albedo), max(0, ray.roughness));
	// 			float fresnelReflectionAmount = FresnelReflectAmount(1.0, ray.indexOfRefraction, ray.normal, rayDirection, reflectionStrength);
				
	// 			rayAlbedo *= ray.albedo;
	// 			vec3 rayNormal = ray.normal;
				
	// 			// Prepare next ray for either reflection or refraction
	// 			bool refraction = Refraction && ray.opacity < 1.0 && opacity < 0.99;
	// 			bool reflection = Reflections && reflectionStrength > 0.01;
	// 			vec3 refractionDirection = refract(rayDirection, rayNormal, ray.indexOfRefraction);
	// 			vec3 reflectionDirection = reflect(rayDirection, rayNormal);
	// 			if (refraction && refractionDirection == vec3(0)) {
	// 				refraction = false;
	// 				reflection = true;
	// 				reflectionStrength = 0.95;
	// 				reflectionDirection = reflect(rayDirection, -rayNormal);
	// 			}
	// 			opacity = min(1, opacity + max(0.01, ray.opacity));
	// 			if (refraction) {
	// 				rayOrigin = ray.position - rayNormal;
	// 				rayDirection = refractionDirection;
	// 				rayMinDistance = float(camera.znear);
	// 				rayMaxDistance = float(camera.zfar);
	// 				rayMask = 0xff;
	// 			} else if (reflection) {
	// 				rayOrigin = ray.position;
	// 				rayDirection = reflectionDirection;
	// 				rayMinDistance = GetOptimalBounceStartDistance(primaryRayDistance);
	// 				rayMaxDistance = float(camera.zfar);
	// 				rayMask = RAY_TRACE_MASK_REFLECTION;
	// 			} else break;
				
	// 			do {
	// 				traceRayEXT(topLevelAS, 0, rayMask, RAY_SBT_OFFSET_RENDERING, 0, 0, rayOrigin, rayMinDistance, rayDirection, rayMaxDistance, RAY_PAYLOAD_LOCATION_RENDERING);
	// 			} while (ray.passthrough && ray.recursions++ < 100 && (rayMinDistance = ray.distance) > 0);
	// 			vec3 color;
	// 			if (ray.distance == 0) {
	// 				color = SampleBackground(rayDirection).rgb;
	// 			} else {
	// 				color = ApplyPBRShading(rayOrigin, ray.position, ray.albedo, ray.normal, /*bump*/vec3(0), ray.roughness, ray.metallic, ray.rim) + ray.emission;
	// 			}
				
	// 			if (refraction) {
	// 				litColor = mix(color*litColor, rayAlbedo, opacity);
	// 				attenuation *= (1-opacity);
	// 			} else if (reflection) {
	// 				litColor = mix(litColor, color*baseReflectivity, fresnelReflectionAmount);
	// 				attenuation *= reflectionStrength;
	// 			}
				
	// 			if (attenuation < 0.01) break;
	// 			if (ray.distance == 0) break;
	// 		}
			
	// 		litColor *= attenuation;
	// 		break;
	// 	case RENDER_MODE_NORMALS:
	// 		litColor = vec3(ray.normal*camera.renderDebugScaling);
	// 		if (primaryRayDistance == 0) litColor = vec3(0);
	// 		opacity = 1;
	// 		break;
	// 	case RENDER_MODE_ALBEDO:
	// 		litColor = vec3(ray.albedo*camera.renderDebugScaling);
	// 		if (primaryRayDistance == 0) litColor = vec3(0);
	// 		opacity = 1;
	// 		break;
	// 	case RENDER_MODE_EMISSION:
	// 		litColor = vec3(ray.emission*camera.renderDebugScaling);
	// 		if (primaryRayDistance == 0) litColor = vec3(0);
	// 		opacity = 1;
	// 		break;
	// 	case RENDER_MODE_DEPTH:
	// 		litColor = vec3(depth*pow(10, camera.renderDebugScaling*6));
	// 		if (primaryRayDistance == 0) litColor = vec3(1,0,1);
	// 		opacity = 1;
	// 		break;
	// 	case RENDER_MODE_DISTANCE:
	// 		litColor = vec3(ray.distance/pow(10, camera.renderDebugScaling*4));
	// 		if (primaryRayDistance == 0) litColor = vec3(1,0,1);
	// 		opacity = 1;
	// 		break;
	// 	case RENDER_MODE_METALLIC:
	// 		litColor = vec3(ray.metallic*camera.renderDebugScaling);
	// 		if (primaryRayDistance == 0) litColor = vec3(1,0,1);
	// 		opacity = 1;
	// 		break;
	// 	case RENDER_MODE_ROUGNESS:
	// 		litColor = ray.roughness >=0 ? vec3(ray.roughness*camera.renderDebugScaling) : vec3(-ray.roughness*camera.renderDebugScaling,0,0);
	// 		if (primaryRayDistance == 0) litColor = vec3(1,0,1);
	// 		opacity = 1;
	// 		break;
	// 	case RENDER_MODE_TIME:
	// 		litColor = vec3(ray.indexOfRefraction*camera.renderDebugScaling/2);
	// 		if (primaryRayDistance == 0) litColor = vec3(1,0,1);
	// 		opacity = 1;
	// 		break;
	// }
	// if (camera.renderMode == RENDER_MODE_BOUNCES) {
	// 	litColor = bounces==0? vec3(0) : getHeatMap(float(bounces-1)/(camera.renderDebugScaling*5));
	// 	opacity = 1;
	// }
	
	// // Store final color
	// imageStore(img_lit, imgCoords, vec4(litColor, opacity));
}


#############################################################
#shader rmiss

layout(location = RAY_PAYLOAD_LOCATION_RENDERING) rayPayloadInEXT RenderingPayload ray;

void main() {
	ray.color = vec4(0);
	ray.position = vec4(0);
	ray.bounceDirection = vec4(0);
	ray.entityInstanceIndex = -1;
	ray.geometryIndex = -1;
	ray.primitiveID = -1;
	ray.raycastCustomData = 0;
}


#############################################################
#shader shadow.rmiss

layout(location = 1) rayPayloadInEXT bool shadowed;

void main() {
	shadowed = false;
}


#############################################################
#shader void.rmiss

void main() {}

