#define RAY_TRACING
#include "v4d/modules/V4D_raytracing/glsl_includes/set0.glsl"


struct SphereAttr {
	vec3 insideFaceNormal; // already in view-space
	float radius;
	float t2;
};


#############################################################
#shader rint

hitAttributeEXT SphereAttr sphereAttr;

void main() {
	vec3 aabb_min = GetProceduralVertexAABB_min(gl_PrimitiveID);
	vec3 aabb_max = GetProceduralVertexAABB_max(gl_PrimitiveID);
	vec3 spherePosition = (GetModelViewMatrix() * vec4((aabb_max + aabb_min)/2, 1)).xyz;
	float sphereRadius = aabb_max.x;
	
	const vec3 origin = gl_WorldRayOriginEXT;
	const vec3 direction = gl_WorldRayDirectionEXT;
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
		vec3 endPoint = gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * t2;
		sphereAttr.insideFaceNormal = normalize(spherePosition - endPoint);
		sphereAttr.radius = sphereRadius;
		sphereAttr.t2 = t2;
		
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
	if (camera.renderMode != RENDER_MODE_STANDARD) ignoreIntersectionEXT;
	// if (ray.entityInstanceIndex == gl_InstanceCustomIndexEXT) {
	// 	ignoreIntersectionEXT;
	// }
}


#############################################################
#shader visibility.rchit

#include "v4d/modules/V4D_raytracing/glsl_includes/set1_rendering.glsl"

hitAttributeEXT SphereAttr sphereAttr;

layout(location = RAY_PAYLOAD_LOCATION_VISIBILITY) rayPayloadInEXT VisibilityPayload ray;

#define RAYLEIGH_BETA vec3(7.0e-6, 8.0e-6, 9.0e-6)
#define MIE_BETA vec3(1e-7)
#define G 0.9 // sun glow

// Graphics Quality configuration
const int RAYMARCH_STEPS = 40; // min=24, low=40, high=64, max=100
const int RAYMARCH_LIGHT_STEPS = 2; // min=1, low=2, high=3, max=5
const float minStepSize = 1.0; // meters
const float sunLuminosityThreshold = 0.01;

// planet-specific stuff
const float planetTerrainHeightVariation = 10000;
const float planetSolidRadius = 8000000;
const float innerRadius = planetSolidRadius - planetTerrainHeightVariation;
const float outerRadius = 8200000;
const float atmosphereHeight = outerRadius - innerRadius;

void main() {
	vec3 hitPoint = gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * gl_HitTEXT;
	vec4 color = HasVertexColor()? GetVertexColor(gl_PrimitiveID) : vec4(1,1,1,0);
	vec3 atmosphereColor = normalize(color.rgb);
	float densityFactor = color.a;
	
	// trace for geometries within the atmosphere
	int traceMask = RAY_TRACED_ENTITY_DEFAULT|RAY_TRACED_ENTITY_TERRAIN|RAY_TRACED_ENTITY_LIGHT;
	traceRayEXT(topLevelAS, 0, traceMask, RAY_SBT_OFFSET_VISIBILITY, 0, 0, gl_WorldRayOriginEXT, gl_HitTEXT, gl_WorldRayDirectionEXT, sphereAttr.t2, RAY_PAYLOAD_LOCATION_VISIBILITY);
	// if (densityFactor < 0.0001) {
	// 	return;
	// }
	// if (ray.position.w > 0 && ray.position.w < minStepSize/2) {
	// 	return;
	// }
	
	float depthDistance = (ray.position.w==0)? float(camera.zfar) : max(minStepSize, ray.position.w);
	float distStart = gl_HitTEXT;
	float distEnd = max(min(depthDistance, sphereAttr.t2), distStart + float(camera.znear));
	float lightRayTravelDepthMax = float(GetDepthBufferFromTrueDistance(sphereAttr.t2));
	vec3 centerPosition = -GetModelViewMatrix()[3].xyz; // equivalent to cameraPosition - spherePosition (or negative position of sphere in view space)
	vec3 viewDir = gl_WorldRayDirectionEXT; // direction from camera to hit point (or direction to hit point in view space)
	
	// Position on sphere
	vec3 rayStart = (viewDir * distStart) + centerPosition;
	vec3 rayEnd = (viewDir * distEnd) + centerPosition;
	float rayLength = distance(rayStart, rayEnd);
	float rayStartAltitude = length(rayStart) - outerRadius; // this will range from negative atmosphereHeight to 0.0
	float dist_epsilon = distStart * 0.0001;
	
	// Ray-marching configuration
	int rayMarchingSteps = RAYMARCH_STEPS;
	float stepSize = rayLength / float(rayMarchingSteps);
	if (stepSize < minStepSize) {
		rayMarchingSteps = int(ceil(rayLength / minStepSize));
		stepSize = rayLength / float(rayMarchingSteps);
	}
	
	// Color configuration
	float rayleighHeight = atmosphereHeight/8;
	float mieHeight = atmosphereHeight/1;
	vec2 scaleHeight = vec2(rayleighHeight, mieHeight);
	bool allowMie = true;//distEnd > depthDistance;
	float gg = G * G;
	
	// Init accumulation variables
	vec3 totalRayleigh = vec3(0);
	vec3 totalMie = vec3(0);
	vec3 atmColor = vec3(0);
	float maxDepth = 0;
	float ambientFactor = 0;
	
	// Loop through sun lights
	for (int activeLightIndex = 0; activeLightIndex < MAX_ACTIVE_LIGHTS; activeLightIndex++) {
		LightSource light = lightSources[activeLightIndex];
		if (light.radius > 10000000.0) { // 10'000 km
			vec3 lightPos = light.position - hitPoint; // relative to hit point on atmosphere
			float luminosity = light.intensity * (1.0 / dot(lightPos, lightPos));
			if (luminosity > sunLuminosityThreshold) {
				
				// Cache some values related to that light before raymarching in the atmosphere
				vec3 lightDir = normalize(lightPos);
				vec3 lightIntensity = light.color * luminosity * 4;
				float mu = dot(viewDir, lightDir);
				float mumu = mu * mu;
				float rayleighPhase = 3.0 / (50.2654824574 /* (16 * pi) */) * (1.0 + mumu);
				float miePhase = allowMie ? 3.0 / (25.1327412287 /* (8 * pi) */) * ((1.0 - gg) * (mumu + 1.0)) / (pow(1.0 + gg - 2.0 * mu * G, 1.5) * (2.0 + gg)) : 0.0;
				
				// RayMarch atmosphere
				vec2 opticalDepth = vec2(0);
				float rayDist = 0;
				for (int i = 0; i < rayMarchingSteps; ++i) {
					vec3 posOnSphere = rayStart + viewDir * (rayDist + stepSize/2.0);
					float depth = outerRadius - length(posOnSphere);
					maxDepth = max(maxDepth, depth);
					
					float altitude = max(0.001, length(posOnSphere) - innerRadius);
					vec2 density = exp(-altitude / scaleHeight / densityFactor) * stepSize;
					opticalDepth += density;
					
					// step size for light ray
					float a = dot(lightDir, lightDir);
					float b = 2.0 * dot(lightDir, posOnSphere);
					float c = dot(posOnSphere, posOnSphere) - (outerRadius * outerRadius);
					float d = (b * b) - 4.0 * a * c;
					float lightRayStepSize = (-b + sqrt(d)) / (2.0 * a * float(RAYMARCH_LIGHT_STEPS));
					ambientFactor += max(0.0, dot(lightDir, normalize(posOnSphere))/rayMarchingSteps);
					
					// RayMarch towards light source
					vec2 lightRayOpticalDepth = vec2(0);
					float lightRayDist = 0;
					float decay = 0;
					for (int l = 0; l < RAYMARCH_LIGHT_STEPS; ++l) {
						vec3 posLightRay = posOnSphere + lightDir * (lightRayDist + lightRayStepSize/2.0);
						float lightRayAltitude = max(0.001, length(posLightRay) - innerRadius);
						vec2 lightRayDensity = exp(-lightRayAltitude / scaleHeight * densityFactor) * lightRayStepSize;
						
						// Sun shafts
						vec3 posLightViewSpace = posLightRay - centerPosition;
						vec4 posLightClipSpace = mat4(camera.projectionMatrix) * vec4(posLightViewSpace, 1);
						vec3 posLightNDC = posLightClipSpace.xyz/posLightClipSpace.w;
						vec3 posLightScreenSpace = posLightNDC * vec3(0.5, 0.5, 0) + vec3(0.5, 0.5, -posLightViewSpace.z);
						if (rayStartAltitude < -dist_epsilon && RAYMARCH_LIGHT_STEPS > 1 && posLightScreenSpace.x > 0 && posLightScreenSpace.x < 1 && posLightScreenSpace.y > 0 && posLightScreenSpace.y < 1 && posLightScreenSpace.z > 0) {
							float depth = texture(img_depth_history, posLightScreenSpace.xy).r;
							if (depth > lightRayTravelDepthMax) 
								decay += 1.0/float(RAYMARCH_LIGHT_STEPS);
						}
						
						lightRayOpticalDepth += lightRayDensity;
						lightRayDist += lightRayStepSize;
					}
					decay *= clamp(-rayStartAltitude/atmosphereHeight, 0.0, 1.0);
					
					vec3 attenuation = exp(-((MIE_BETA * (opticalDepth.y + lightRayOpticalDepth.y)) + (RAYLEIGH_BETA * (opticalDepth.x + lightRayOpticalDepth.x))));
					totalRayleigh += density.x * attenuation * (1.0-decay/3.0);
					totalMie += density.y * attenuation * (1.0-decay);
					
					rayDist += stepSize;
				}

				atmColor += vec3(
					(
						rayleighPhase * RAYLEIGH_BETA * totalRayleigh // rayleigh colorx
						+ miePhase * MIE_BETA * totalMie // mie
					) * lightIntensity * atmosphereColor / rayMarchingSteps
				);
				
			}
		}
	}
	
	float alpha = clamp(maxDepth / atmosphereHeight * pow(densityFactor, 1.0/8), 0, 1);
	ray.color.rgb = mix(ray.color.rgb*min(1.0, max(ambientFactor, length(atmColor))), atmColor, alpha);
	// ray.specular *= 1.0 - alpha;
}

