// PBR rendering

// settings
const float radianceThreshold = 1e-10;

const float PI = 3.1415926543;

// PBR formulas
float distributionGGX(float NdotH, float roughness) {
	float a = roughness*roughness;
	float a2 = a*a;
	float denom = NdotH*NdotH * (a2 - 1.0) + 1.0;
	denom = PI * denom*denom;
	return a2 / max(denom, 0.0000001);
}
float geometrySmith(float NdotV, float NdotL, float roughness) {
	float r = roughness + 1.0;
	float k = (r*r) / 8.0;
	float ggx1 = NdotV / (NdotV * (1.0 - k) + k);
	float ggx2 = NdotL / (NdotL * (1.0 - k) + k);
	return ggx1 * ggx2;
}
vec3 fresnelSchlick(float HdotV, vec3 baseReflectivity) {
	// baseReflectivity in range 0 to 1
	// returns range baseReflectivity to 1
	// increases as HdotV decreases (more reflectivity when surface viewed at larger angles)
	return baseReflectivity + (1.0 - baseReflectivity) * pow(1.0 - HdotV, 5.0);
}

//https://blog.demofox.org/2017/01/09/raytracing-reflection-refraction-fresnel-total-internal-reflection-and-beers-law/
float FresnelReflectAmount(float n1, float n2, vec3 normal, vec3 incident, float reflectivity) {
	// Schlick aproximation
	float r0 = (n1-n2) / (n1+n2);
	r0 *= r0;
	float cosX = -dot(normal, incident);
	if (n1 > n2) {
		float n = n1/n2;
		float sinT2 = n*n*(1.0-cosX*cosX);
		// Total internal reflection
		if (sinT2 > 1.0)
			return 1.0;
		cosX = sqrt(1.0-sinT2);
	}
	float x = 1.0-cosX;
	float ret = r0+(1.0-r0)*x*x*x*x*x;
	// adjust reflect multiplier for object reflectivity
	return reflectivity + (1.0-reflectivity) * ret;
}

vec3 ApplyPBRShading(vec3 origin, vec3 hitPoint, vec3 albedo, vec3 normal, vec3 bump, float roughness, float metallic, vec4 rim) {
	if (roughness == 0.0) return albedo;
	
	vec3 color = vec3(0);
	
	// PBR lighting
	float distance = distance(origin, hitPoint);
	vec3 N = normal;
	vec3 V = normalize(origin - hitPoint);
	// calculate reflectance at normal incidence; if dia-electric (like plastic) use baseReflectivity of 0.4 and if it's metal, use the albedo color as baseReflectivity
	vec3 baseReflectivity = mix(vec3(0.04), albedo, max(0,metallic));
	
	for (int activeLightIndex = 0; activeLightIndex < MAX_ACTIVE_LIGHTS; activeLightIndex++) {
		LightSource light = lightSources[activeLightIndex];
	
		if (light.radius > 0.0) {
			
			// Calculate light radiance at distance
			float dist = length(light.position - hitPoint);
			vec3 radiance = light.color * light.intensity * (1.0 / (dist*dist));
			vec3 L = normalize(light.position - hitPoint);
			
			if (length(radiance) > radianceThreshold) {
				#ifndef DISABLE_SHADOWS
				if (HardShadows) {
					shadowed = true;
					if (dot(L, normal) > 0) {
						vec3 shadowRayStart = hitPoint + bump + V*GetOptimalBounceStartDistance(distance); // starting shadow ray just outside the surface this way solves precision issues when casting shadows
						traceRayEXT(topLevelAS, gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsOpaqueEXT | gl_RayFlagsSkipClosestHitShaderEXT, RAY_TRACE_MASK_SHADOWS, 0, 0, 1, shadowRayStart, float(camera.znear), L, length(light.position - hitPoint) - light.radius, 1);
					}
				}
				if (!HardShadows || !shadowed)
				#endif
				{
					// cook-torrance BRDF
					vec3 H = normalize(V + L);
					float NdotV = max(dot(N,V), 0.000001);
					float NdotL = max(dot(N,L), 0.000001);
					float HdotV = max(dot(H,V), 0.0);
					float NdotH = max(dot(N,H), 0.0);
					//
					float D = distributionGGX(NdotH, roughness); // larger the more micro-facets aligned to H (normal distribution function)
					float G = geometrySmith(NdotV, NdotL, roughness); // smaller the more micro-facets shadowed by other micro-facets
					vec3 F = fresnelSchlick(HdotV, baseReflectivity); // proportion of specular reflectance
					vec3 FV = fresnelSchlick(NdotV, baseReflectivity); // proportion of specular reflectance
					vec3 FL = fresnelSchlick(NdotL, baseReflectivity); // proportion of specular reflectance
					//
					vec3 specular = D * G * F;
					specular /= 4.0 * NdotV * NdotL;
					// for energy conservation, the diffuse and specular light can't be above 1.0 (unless the surface emits light); to preserve this relationship the diffuse component (kD) should equal to 1.0 - kS.
					// vec3 kD = vec3(1.0)-F;
					vec3 kD = (vec3(1.0)-FV) * (vec3(1.0)-FL) * 1.05;
					// multiply kD by the inverse metalness such that only non-metals have diffuse lighting, or a linear blend if partly metal (pure metals have no diffuse light).
					kD *= 1.0 - max(0, metallic);
					// Final lit color
					color += (kD * albedo / PI + specular) * radiance * max(dot(N,L) + max(0, metallic/-5), 0);
					
					// Sub-Surface Scattering (simple rim for now)
					if (rim.a > 0) {
						color += rim.rgb * rim.a * radiance * (pow(1.0 - NdotV, 2+rim.a*2+NdotL) * NdotL);
					}
				}
			}
		}
	}
	
	return color;
}
