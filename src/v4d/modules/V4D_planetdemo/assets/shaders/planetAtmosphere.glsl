#include "core.glsl"
#include "v4d/modules/V4D_hybrid/glsl_includes/pl_fog_raster.glsl"

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

vec3 PlanetSpaceDir(vec3 dir) {
	return normalize(transpose(mat3(planetAtmosphere.modelViewMatrix)) * dir);
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

#include "noise.glsl"

layout(location = 0) in V2F v2f;

const int RAYMARCH_STEPS = 40; // min=24, low=40, high=64, max=100
const int RAYMARCH_LIGHT_STEPS = 2; // min=1, low=2, high=3, max=5
const float minStepSize = 100.0; // meters

#define RAYLEIGH_BETA vec3(7.0e-6, 8.0e-6, 9.0e-6)
#define MIE_BETA vec3(1e-7)
#define G 0.9 // sun glow

void main() {
	vec4 unpackedAtmosphereColor = UnpackVec4FromUint(planetAtmosphere.color);
	vec3 atmosphereColor = normalize(unpackedAtmosphereColor.rgb);
	float atmosphereAmbient = unpackedAtmosphereColor.a;
	float atmosphereHeight = planetAtmosphere.outerRadius - planetAtmosphere.innerRadius;
	float depthDistance = subpassLoad(in_img_gBuffer_2).w;
	
	if (depthDistance == 0) depthDistance = float(camera.zfar);
	depthDistance = max(minStepSize, depthDistance);
	vec3 p = -planetAtmosphere.modelViewMatrix[3].xyz; // equivalent to cameraPosition - spherePosition (or negative position of sphere in view space)
	vec3 viewDir = normalize(v2f.pos); // direction from camera to hit points (or direction to hit point in view space)
	float r = planetAtmosphere.outerRadius;
	float b = -dot(p,viewDir);
	float det = sqrt(b*b - dot(p,p) + r*r);
	
	float distStart = max(0.1, min(b - det, length(v2f.pos)));
	if (depthDistance <= distStart) discard;
	float distEnd = max(min(depthDistance, b + det), distStart + 0.01);
	float lightRayTravelDepthMax = float(GetDepthBufferFromTrueDistance(b + det));
	
	// Position on sphere
	vec3 rayStart = (viewDir * distStart) + p;
	vec3 rayEnd = (viewDir * distEnd) + p;
	float rayLength = distance(rayStart, rayEnd);
	float rayStartAltitude = length(rayStart) - r; // this will range from negative atmosphereHeight to 0.0
	float dist_epsilon = distStart * 0.0001;
	
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
	
	float rayleighHeight = atmosphereHeight/8;
	float mieHeight = atmosphereHeight/1;
	vec2 scaleHeight = vec2(rayleighHeight, mieHeight);
	bool allowMie = true;//distEnd > depthDistance;
	float gg = G * G;
	
	vec3 totalRayleigh = vec3(0);
	vec3 totalMie = vec3(0);
	vec3 atmColor = vec3(0);
	float maxDepth = 0;
	
	// vec4 aurora = vec4(0);
	// float auroraHeight = 100000;
	// float t = camera.time / 32.0;
	// vec3 polarPos = mix(rayStart, rayEnd, smoothstep(atmosphereHeight, atmosphereHeight-auroraHeight, (length(rayStart)-planetAtmosphere.innerRadius)));
	// // vec3 polarPos = mix(rayStart, rayEnd, 0.5);
	// vec3 polarDir = PlanetSpaceDir(normalize(polarPos));
	// float polar = abs(dot(polarDir, vec3(0,1,0)));
	// float auroraWidth = max(0.1, (FastSimplex(polarDir*2.0+vec3(48.5,0.8*t,t)))/2.0+0.5) * 0.1;
	// float auroraPosition = 0.9;
	// auroraPosition += FastSimplexFractal(polarDir*2.0+vec3(12.5,64.0,t/16.0), 2)/8.0;
	// auroraPosition += FastSimplexFractal(polarDir*vec3(5.0, 40.0, 5.0)+vec3(8.5,16.5,t/8.0), 2)/16.0;
	// auroraPosition += FastSimplexFractal(polarDir*vec3(15.0, 100.0, 15.0)+vec3(48.5,12.0,t), 4)/40.0;
	// vec3 auroraMainColor = vec3(0.0, 1.0, 0.0);
	// vec3 auroraSecondaryColor = mix(vec3(1.0, 0.1, 0.3), vec3(0.0, 0.2, 1.0), FastSimplexFractal(polarDir*3.0+vec3(9.5,24.0,t*3.0), 2)/2.0+0.5);
	// vec3 auroraFinalColor = normalize(mix(auroraMainColor, auroraSecondaryColor, pow(FastSimplexFractal(polarDir*2.0+vec3(28.5,2.0,t*2.0), 2)/2.0+0.5, 1.5)));
	
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
			vec3 posOnSphere = rayStart + viewDir * (rayDist + stepSize/2.0);
			float depth = r-length(posOnSphere);
			maxDepth = max(maxDepth, depth);
			
			float altitude = max(0.001, length(posOnSphere) - planetAtmosphere.innerRadius);
			vec2 density = exp(-altitude / scaleHeight) * stepSize;
			opticalDepth += density;
			
			// polarDir = PlanetSpaceDir(normalize(posOnSphere));
			// polar = abs(dot(polarDir, vec3(0,1,0)));
			// float aur = smoothstep(auroraPosition-auroraWidth/2.0, auroraPosition, polar) * smoothstep(auroraPosition+auroraWidth/2.0, auroraPosition, polar) * smoothstep(auroraHeight, auroraHeight/2.0, depth);
			// aurora += vec4(auroraFinalColor*aur, aur*maxDepth/atmosphereHeight)/float(rayMarchingSteps)/4.0;
			
			// step size for light ray
			float a = dot(lightDir, lightDir);
			float b = 2.0 * dot(lightDir, posOnSphere);
			float c = dot(posOnSphere, posOnSphere) - (r * r);
			float d = (b * b) - 4.0 * a * c;
			float lightRayStepSize = (-b + sqrt(d)) / (2.0 * a * float(RAYMARCH_LIGHT_STEPS));
			
			// RayMarch towards light source
			vec2 lightRayOpticalDepth = vec2(0);
			float lightRayDist = 0;
			float decay = 0;
			for (int l = 0; l < RAYMARCH_LIGHT_STEPS; ++l) {
				vec3 posLightRay = posOnSphere + lightDir * (lightRayDist + lightRayStepSize/2.0);
				float lightRayAltitude = max(0.001, length(posLightRay) - planetAtmosphere.innerRadius);
				vec2 lightRayDensity = exp(-lightRayAltitude / scaleHeight) * lightRayStepSize;
				
				vec3 posLightViewSpace = posLightRay-p;
				vec4 posLightClipSpace = mat4(camera.projectionMatrix) * vec4(posLightViewSpace, 1);
				vec3 posLightNDC = posLightClipSpace.xyz/posLightClipSpace.w;
				vec3 posLightScreenSpace = posLightNDC * vec3(0.5, 0.5, 0) + vec3(0.5, 0.5, -posLightViewSpace.z);
				
				if (rayStartAltitude < -dist_epsilon && RAYMARCH_LIGHT_STEPS > 1 && posLightScreenSpace.x > 0 && posLightScreenSpace.x < 1 && posLightScreenSpace.y > 0 && posLightScreenSpace.y < 1 && posLightScreenSpace.z > 0) {
					float depth;
					if (RayTracedVisibility) {
						depth = texture(tex_img_depth, posLightScreenSpace.xy).r;
					} else {
						depth = texture(tex_img_rasterDepth, posLightScreenSpace.xy).r;
					}
					if (depth > lightRayTravelDepthMax) decay += 1.0/float(RAYMARCH_LIGHT_STEPS);
				}
				
				lightRayOpticalDepth += lightRayDensity;
				lightRayDist += lightRayStepSize;
			}
			decay *= clamp(-rayStartAltitude/atmosphereHeight*8.0, 0.0, 1.0);
			
			vec3 attenuation = exp(-((MIE_BETA * (opticalDepth.y + lightRayOpticalDepth.y)) + (RAYLEIGH_BETA * (opticalDepth.x + lightRayOpticalDepth.x)))) * planetAtmosphere.densityFactor;
			totalRayleigh += density.x * attenuation * (1.0-decay/3.0);
			totalMie += density.y * attenuation * (1.0-decay);
			
			rayDist += stepSize;
		}

		atmColor += vec3(
			(
				rayleighPhase * RAYLEIGH_BETA * (planetAtmosphere.densityFactor*2) * totalRayleigh // rayleigh color
				+ miePhase * MIE_BETA * (planetAtmosphere.densityFactor*2) * totalMie // mie
				+ opticalDepth.x * (atmosphereColor/10000000.0*planetAtmosphere.densityFactor*atmosphereAmbient) // ambient
			) * lightIntensity * atmosphereColor
		);
	}
	
	float alpha = clamp(maxDepth/atmosphereHeight, 0, 0.5);
	out_img_lit = vec4(atmColor, alpha);
	// float alpha = clamp(maxDepth/atmosphereHeight + aurora.a, 0, 0.5);
	// out_img_lit = vec4(mix(aurora.rgb, atmColor, clamp(length(atmColor)+0.5, 0, 1)), alpha);
}
