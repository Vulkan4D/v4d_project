#include "core.glsl"
#include "Camera.glsl"

const int NB_SUNS = 3;

layout(std430, push_constant) uniform PlanetAtmosphere{
	mat4 modelViewMatrix;
	vec4 suns[NB_SUNS];
	uint color;
	float innerRadius;
	float outerRadius;
	float densityFactor;
} planetAtmosphere;

struct Sun {
	vec3 dir;
	vec3 intensity;
	bool valid;
};

Sun UnpackSunFromVec4(vec4 in_sun) {
	Sun sun;
	sun.dir = normalize(in_sun.xyz);
	sun.intensity = UnpackVec3FromFloat(in_sun.w) * length(in_sun.xyz);
	sun.valid = length(in_sun.xyz) > 0.001;
	return sun;
}

struct V2F {
	vec3 pos;
};

vec3 ViewSpaceNormal(vec3 normal) {
	return normalize(transpose(inverse(mat3(planetAtmosphere.modelViewMatrix))) * normal);
}

##################################################################
#shader vert

layout(location = 0) in vec4 pos;

layout(location = 0) out V2F v2f;

void main() {
	gl_Position = mat4(camera.projectionMatrix) * planetAtmosphere.modelViewMatrix * vec4(pos.xyz, 1);
	v2f.pos = (planetAtmosphere.modelViewMatrix * vec4(pos.xyz, 1)).xyz;
}

##################################################################
#shader frag

layout(set = 1, input_attachment_index = 1, binding = 0) uniform highp subpassInput gBuffer_position;
layout(location = 0) in V2F v2f;

layout(location = 0) out vec4 color;

const int RAYMARCH_STEPS = 64; // minimum of 3
const int RAYMARCH_LIGHT_STEPS = 2; // minimum of 2
const float minStepSize = 100.0; // meters

#define RAYLEIGH_BETA vec3(9.0e-6, 11.0e-6, 17.0e-6)
#define MIE_BETA vec3(1e-7)
// #define AMBIENT_BETA vec3(0.6,0.2,0.05)/200000
#define AMBIENT_BETA vec3(0)
#define G 0.7 /* blob around the sun */

void main() {
	vec4 unpackedAtmosphereColor = UnpackVec4FromUint(planetAtmosphere.color);
	vec3 atmosphereColor = normalize(unpackedAtmosphereColor.rgb);
	float atmosphereAmbient = unpackedAtmosphereColor.a;
	float atmosphereHeight = planetAtmosphere.outerRadius - planetAtmosphere.innerRadius;
	float depthDistance = subpassLoad(gBuffer_position).w;
	if (depthDistance == 0) depthDistance = float(camera.zfar);
	depthDistance = max(minStepSize, depthDistance);
	vec3 p = -planetAtmosphere.modelViewMatrix[3].xyz; // equivalent to cameraPosition - spherePosition (or negative position of sphere in view space)
	vec3 viewDir = normalize(v2f.pos); // direction from camera to hit points (or direction to hit point in view space)
	float r = planetAtmosphere.outerRadius;
	float b = -dot(p,viewDir);
	float det = sqrt(b*b - dot(p,p) + r*r);
	
	float distStart = max(0.1, min(b - det, length(v2f.pos)));
	float distEnd = max(min(depthDistance, b + det), distStart + 0.01);
	// Position on sphere
	vec3 rayStart = (viewDir * distStart) + p;
	vec3 rayEnd = (viewDir * distEnd) + p;
	float rayLength = distance(rayStart, rayEnd);
	
	int rayMarchingSteps = RAYMARCH_STEPS;
	float stepSize = rayLength / float(rayMarchingSteps);
	if (stepSize < minStepSize) {
		rayMarchingSteps = int(ceil(rayLength / minStepSize));
		stepSize = rayLength / float(rayMarchingSteps);
	}
	
	// Unpack suns
	Sun suns[NB_SUNS];
	for (int i = 0; i < NB_SUNS; ++i) {
		suns[i] = UnpackSunFromVec4(planetAtmosphere.suns[i]);
	}
	
	float rayleighHeight = atmosphereHeight * 0.07 * planetAtmosphere.densityFactor;
	float mieHeight = atmosphereHeight / 10.0;
	vec2 scaleHeight = vec2(rayleighHeight, mieHeight);
	bool allowMie = distEnd > depthDistance;
	float gg = G * G;
	
	vec3 totalRayleigh = vec3(0);
	vec3 totalMie = vec3(0);
	vec3 atmColor = vec3(0);
	float atmosphereDensity = 0;

	for (int sunIndex = 0; sunIndex < NB_SUNS; ++sunIndex) if (suns[sunIndex].valid) {
		// Calculate Rayleigh and Mie phases
		vec3 lightDir = suns[sunIndex].dir;
		vec3 lightIntensity = suns[sunIndex].intensity * 4;
		float mu = dot(viewDir, lightDir);
		float mumu = mu * mu;
		float rayleighPhase = 3.0 / (50.2654824574 /* (16 * pi) */) * (1.0 + mumu);
		float miePhase = allowMie ? 3.0 / (25.1327412287 /* (8 * pi) */) * ((1.0 - gg) * (mumu + 1.0)) / (pow(1.0 + gg - 2.0 * mu * G, 1.5) * (2.0 + gg)) : 0.0;
		
		// RayMarch atmosphere
		vec2 opticalDepth = vec2(0);
		float rayDist = 0;
		for (int i = 0; i < rayMarchingSteps; ++i) {
			vec3 posOnSphere = rayStart + viewDir * (rayDist + stepSize / 2.0);
			float altitude = length(posOnSphere) - planetAtmosphere.innerRadius;
			vec2 density = exp(-altitude / scaleHeight) * stepSize;
			opticalDepth += density;
			
			// step size for light ray
			float a = dot(lightDir, lightDir);
			float b = 2.0 * dot(lightDir, posOnSphere);
			float c = dot(posOnSphere, posOnSphere) - (r * r);
			float d = (b * b) - 4.0 * a * c;
			float lightRayStepSize = (-b + sqrt(d)) / (2.0 * a * float(RAYMARCH_LIGHT_STEPS));
			
			// RayMarch towards light source
			vec2 lightRayOpticalDepth = vec2(0);
			float lightRayDist = 0;
			for (int l = 0; l < RAYMARCH_LIGHT_STEPS; ++l) {
				vec3 posLightRay = posOnSphere + lightDir * (lightRayDist + lightRayStepSize / 2.0);
				float lightRayAltitude = length(posLightRay) - planetAtmosphere.innerRadius;
				lightRayOpticalDepth += exp(-lightRayAltitude / scaleHeight) * lightRayStepSize;
				lightRayDist += lightRayStepSize;
			}
			
			vec3 attenuation = exp(-((MIE_BETA * (opticalDepth.y + lightRayOpticalDepth.y)) + (RAYLEIGH_BETA * (opticalDepth.x + lightRayOpticalDepth.x)))) * planetAtmosphere.densityFactor * 2;
			totalRayleigh += density.x * attenuation;
			totalMie += density.y * attenuation;
			
			rayDist += stepSize;
		}

		vec3 opacity = exp(-((MIE_BETA * opticalDepth.y) + (RAYLEIGH_BETA * opticalDepth.x)));
		atmColor += vec3(
			(
				rayleighPhase * RAYLEIGH_BETA * (planetAtmosphere.densityFactor*2) * totalRayleigh // rayleigh color
				+ miePhase * MIE_BETA * (planetAtmosphere.densityFactor*2) * totalMie // mie
				+ opticalDepth.x * (atmosphereColor/10000000.0*planetAtmosphere.densityFactor*atmosphereAmbient) // ambient
			) * lightIntensity * atmosphereColor
		);
		atmosphereDensity += clamp(pow(length(opticalDepth), 0.01), 0, 1);
	}
	
	color = vec4(atmColor, max(0.0, min(1.0, atmosphereDensity / 2.0)));
}
