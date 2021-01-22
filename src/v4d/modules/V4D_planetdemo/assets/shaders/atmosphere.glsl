#define RAY_TRACING
#include "v4d/modules/V4D_raytracing/glsl_includes/set0.glsl"

struct AtmosphereAttr {
	vec3 position; // in local space
	float radius;
	float t2;
};


#############################################################
#shader rint

hitAttributeEXT AtmosphereAttr atmosphereAttr;

void main() {
	const vec3 aabb_min = GetProceduralVertexAABB_min(gl_PrimitiveID);
	const vec3 aabb_max = GetProceduralVertexAABB_max(gl_PrimitiveID);
	const vec3 spherePosition = (aabb_max + aabb_min) / 2;
	const float sphereRadius = (aabb_max.x - aabb_min.x) / 2;
	const vec3 origin = gl_ObjectRayOriginEXT;
	const vec3 direction = gl_ObjectRayDirectionEXT;
	const float tMin = gl_RayTminEXT;
	const float tMax = gl_RayTmaxEXT;
	const vec3 oc = origin - spherePosition;
	const float a = dot(direction, direction);
	const float b = dot(oc, direction);
	const float c = dot(oc, oc) - sphereRadius*sphereRadius;
	const float discriminant = b * b - a * c;

	if (discriminant >= 0) {
		const float discriminantSqrt = sqrt(discriminant);
		const float t1 = (-b - discriminantSqrt) / a;
		const float t2 = (-b + discriminantSqrt) / a;
		vec3 hitPoint = origin + direction * t1;
		
		atmosphereAttr.position = spherePosition;
		atmosphereAttr.radius = sphereRadius;
		atmosphereAttr.t2 = t2;
		
		// Outside of sphere
		if (tMin <= t1 && t1 < tMax) {
			reportIntersectionEXT(t1, 0);
		}
		
		// Inside of sphere
		if (t1 <= tMin && t2 >= tMin) {
			reportIntersectionEXT(tMin, 1);
		}
	}
}


#############################################################
#shader visibility.rahit

layout(location = RAY_PAYLOAD_LOCATION_VISIBILITY) rayPayloadInEXT VisibilityPayload ray;

void main() {
	// if (camera.renderMode != RENDER_MODE_STANDARD) ignoreIntersectionEXT;
	
	// if (ray.entityInstanceIndex == gl_InstanceCustomIndexEXT) {
	// 	ignoreIntersectionEXT;
	// }
}


#############################################################
#shader visibility.rchit

#include "v4d/modules/V4D_raytracing/glsl_includes/set1_rendering.glsl"

hitAttributeEXT AtmosphereAttr atmosphereAttr;

layout(location = RAY_PAYLOAD_LOCATION_VISIBILITY) rayPayloadInEXT VisibilityPayload ray;

#define RAYLEIGH_BETA vec3(3.8e-6, 13.5e-6, 33.1e-6)
#define MIE_BETA vec3(21e-6)
#define G 0.9 // sun glow

// Graphics Quality configuration
const int MAX_RAYMARCH_STEPS = 24;
const int RAYMARCH_LIGHT_STEPS = 3;
const float sunLuminosityThreshold = 0.01;

// planet-specific stuff
const float planetSolidRadius = 8000000;
const float visibilityDistance = 1000000;
const float minStepSize = visibilityDistance / 1000.0;
const float innerRadius = planetSolidRadius - 5000;
const float outerRadius = 8200000;
const float atmosphereThickness = outerRadius - innerRadius;
const float rayleighHeight = 12000;
const float mieHeight = 5000;
const vec2 scaleHeight = vec2(rayleighHeight, mieHeight);
const float gg = G * G;

void main() {
	// trace for geometries within the atmosphere
	uint traceMask = RAY_TRACED_ENTITY_DEFAULT|RAY_TRACED_ENTITY_TERRAIN|RAY_TRACED_ENTITY_LIGHT ;//|RAY_TRACED_ENTITY_LIGHT ;// RAY_TRACE_MASK_VISIBLE & ~RAY_TRACED_ENTITY_ATMOSPHERE;
	if (ray.bounceMask > 0) {
		traceMask &= ray.bounceMask;
	}
	traceRayEXT(topLevelAS, 0, traceMask, RAY_SBT_OFFSET_VISIBILITY, 0, 0, gl_WorldRayOriginEXT, gl_HitTEXT, gl_WorldRayDirectionEXT, float(camera.zfar), RAY_PAYLOAD_LOCATION_VISIBILITY);
	VisibilityPayload hitRay = ray;
	const float hitDistance = hitRay.position.w==0? float(camera.zfar) : hitRay.position.w;
	bool hasHitSomethingWithinAtmosphere = hitDistance < atmosphereAttr.t2;
	
	const vec3 startPoint = gl_ObjectRayOriginEXT + gl_ObjectRayDirectionEXT * gl_HitTEXT;
	const vec3 endPoint = gl_ObjectRayOriginEXT + gl_ObjectRayDirectionEXT * min(atmosphereAttr.t2, hitDistance);
	const mat3 objectToViewSpaceDir = GetModelNormalViewMatrix();
	const mat4 objectToViewSpacePos = GetModelViewMatrix();
	const float rayDepth = distance(startPoint, endPoint);
	
	if (rayDepth < minStepSize) return;
	
	// Ray-marching configuration
	vec3 rayMarchDirObjectSpace = gl_ObjectRayDirectionEXT;
	vec3 rayMarchDirViewSpace = gl_WorldRayDirectionEXT;
	int rayMarchingSteps = MAX_RAYMARCH_STEPS;
	float stepSize = rayDepth / float(rayMarchingSteps);
	if (stepSize < minStepSize) {
		rayMarchingSteps = int(ceil(rayDepth / minStepSize));
		stepSize = rayDepth / float(rayMarchingSteps);
	}
	
	// Init accumulation variables
	vec3 atmColor = vec3(0);
	float maxDepth = 0;
	vec4 fog = vec4(0);
	
	// Loop through sun lights
	for (int activeLightIndex = 0; activeLightIndex < MAX_ACTIVE_LIGHTS; activeLightIndex++) {
		LightSource light = lightSources[activeLightIndex];
		if (light.radius > 10000000.0) { // 10'000 km
			vec3 lightPosViewSpace = light.position;
			vec3 lightDirViewSpace = normalize(lightPosViewSpace);
			vec3 lightPosObjectSpace = (inverse(objectToViewSpacePos) * vec4(lightPosViewSpace, 1)).xyz;
			vec3 lightDirObjectSpace = normalize(lightPosObjectSpace);
			float luminosity = light.intensity * (1.0 / dot(lightPosObjectSpace, lightPosObjectSpace));
			if (luminosity > sunLuminosityThreshold) {
				
				// Cache some values related to that light before raymarching in the atmosphere
				vec3 lightIntensity = light.color * luminosity;
				float mu = dot(rayMarchDirViewSpace, lightDirViewSpace);
				float mumu = mu * mu;
				float rayleighPhase = 3.0 / (50.2654824574 /* (16 * pi) */) * (1.0 + mumu);
				float miePhase = 3.0 / (25.1327412287 /* (8 * pi) */) * ((1.0 - gg) * (mumu + 1.0)) / (pow(1.0 + gg - 2.0 * mu * G, 1.5) * (2.0 + gg));
				
				// Init accumulation variables
				vec3 totalRayleigh = vec3(0);
				vec3 totalMie = vec3(0);
				vec2 opticalDepth = vec2(0);
	
				// Ray-March
				for (float dist = 0; dist < rayDepth; dist += stepSize) {
					vec3 rayPosObjectSpace = startPoint + rayMarchDirObjectSpace * dist;
					float rayAltitude = length(rayPosObjectSpace);
					float rayAltitudeAboveInnerRadius = rayAltitude - innerRadius;
					// float rayAltitudeFactor = clamp(rayAltitudeAboveInnerRadius / atmosphereThickness, 0, 1); // 0.0 = deepest,  1.0 = top of atmosphere
					// float rayDepthFactor = 1.0 - rayAltitudeFactor; // 1.0 = deepest,  0.0 = top of atmosphere

					vec2 density = exp(-max(0.001, rayAltitudeAboveInnerRadius) / scaleHeight) * stepSize;
					opticalDepth += density;
					
					maxDepth = max(maxDepth, outerRadius - rayAltitude);
					
					// light
					vec3 rayPosViewSpace = (objectToViewSpacePos * vec4(rayPosObjectSpace, 1)).xyz;
					vec3 lightPosViewSpaceRelativeToRayPos = lightPosViewSpace - rayPosViewSpace; // relative to current ray position in atmosphere in view space
					vec3 lightPosObjectCurrentRaySpace = lightPosObjectSpace - rayPosObjectSpace;
					vec3 lightDirObjectCurrentRaySpace = normalize(lightPosObjectCurrentRaySpace);

					// step size for light ray
					float a = dot(lightDirObjectCurrentRaySpace, lightDirObjectCurrentRaySpace);
					float b = 2.0 * dot(lightDirObjectCurrentRaySpace, rayPosObjectSpace);
					float c = dot(rayPosObjectSpace, rayPosObjectSpace) - (outerRadius * outerRadius);
					float d = (b * b) - 4.0 * a * c;
					int rayMarchLightSteps = RAYMARCH_LIGHT_STEPS;
					float lightRayDepth = (sqrt(d) - b);
					float lightRayStepSize
						// = (-b + sqrt(d)) / (2.0 * a * float(rayMarchLightSteps));
						= lightRayDepth / float(rayMarchLightSteps);
					if (lightRayStepSize < minStepSize) {
						rayMarchLightSteps = 1;
						lightRayStepSize = lightRayDepth;
					}
					
					// RayMarch towards light source
					vec2 lightRayOpticalDepth = vec2(0);
					float lightRayDist = 0;
					for (float lightRayDist = 0; lightRayDist < lightRayDepth; lightRayDist += lightRayStepSize) {
						
						vec3 posLightRayObjectSpace = rayPosObjectSpace + lightDirObjectCurrentRaySpace * (lightRayDist + lightRayStepSize/2.0);
						// vec3 posLightRayViewSpace = rayPosViewSpace + lightDirViewSpace * (lightRayDist + lightRayStepSize/2.0);
						float lightRayAltitude = length(posLightRayObjectSpace);
						float lightRayAltitudeAboveInnerRadius = lightRayAltitude - innerRadius;
						// float lightRayAltitudeFactor = clamp(lightRayAltitudeAboveInnerRadius / atmosphereThickness, 0, 1); // 0.0 = deepest,  1.0 = top of atmosphere
						// float lightRayDepthFactor = 1.0 - lightRayAltitudeFactor; // 1.0 = deepest,  0.0 = top of atmosphere
						
						vec2 lightRayDensity = exp(-max(0.001, lightRayAltitudeAboveInnerRadius) / scaleHeight) * lightRayStepSize;
						lightRayOpticalDepth += lightRayDensity;
					}
				
					vec3 attenuation = exp(-((MIE_BETA * (opticalDepth.y + lightRayOpticalDepth.y)) + (RAYLEIGH_BETA * (opticalDepth.x + lightRayOpticalDepth.x))));
					totalRayleigh += density.x * attenuation;
					totalMie += density.y * attenuation;
					
					// // Ray-Traced Sun shafts
					// // if (RandomFloat(hitRay.randomSeed) < (1.0/rayMarchingSteps)) {
					// 	traceRayEXT(topLevelAS, 0, traceMask, RAY_SBT_OFFSET_VISIBILITY, 0, 0, rayPosViewSpace, GetOptimalBounceStartDistance(hitRay.normal.w), normalize(lightPosViewSpaceRelativeToRayPos), length(lightPosViewSpaceRelativeToRayPos) - light.radius, RAY_PAYLOAD_LOCATION_VISIBILITY);
					// 	if (ray.position.w > 0) {
					// 		totalRayleigh = vec3(0);
					// 		totalMie = vec3(0);
					// 	}
					// // }
					
					fog += vec4(vec3(
						clamp(
							rayleighPhase * RAYLEIGH_BETA * density.x * attenuation // rayleigh color
						, 0, 1) * lightIntensity
					), (outerRadius - rayAltitude) / atmosphereThickness * stepSize / visibilityDistance);
				}

				atmColor += vec3(
					clamp(
						rayleighPhase * RAYLEIGH_BETA * totalRayleigh // rayleigh color
						+ miePhase * MIE_BETA * totalMie // mie
					, 0, 1) * lightIntensity
				);
			}
		}
	}
	
	ray = hitRay;
	if (hasHitSomethingWithinAtmosphere) 
		ray.fog = max(ray.fog, vec4(fog.rgb, mix(0, clamp(fog.a, 0, 1), clamp(pow(smoothstep(minStepSize, minStepSize*100, hitDistance), 0.5),0,1))));
	else
		ray.emission += atmColor;
}

