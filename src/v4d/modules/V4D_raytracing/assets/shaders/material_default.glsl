#define RAY_TRACING
#include "v4d/modules/V4D_raytracing/glsl_includes/set0.glsl"
#include "noise.glsl"

#shader rcall

layout(location = CALL_DATA_LOCATION_MATERIAL) callableDataInEXT ProceduralMaterialCall mat;
layout(location = CALL_DATA_LOCATION_TEXTURE) callableDataEXT ProceduralTextureCall tex;

//https://blog.demofox.org/2017/01/09/raytracing-reflection-refraction-fresnel-total-internal-reflection-and-beers-law/
float FresnelReflectAmount(float n1, float n2, float metallic) {
	// Schlick aproximation
	float r0 = (n1-n2) / (n1+n2);
	r0 *= r0;
	float cosX = -dot(mat.normal.xyz, mat.rayDirection.xyz);
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
	return metallic + (1.0-metallic) * ret;
}

float Schlick(const float cosine, const float indexOfRefraction) {
	float r0 = (1 - indexOfRefraction) / (1 + indexOfRefraction);
	r0 *= r0;
	return r0 + (1 - r0) * pow(1 - cosine, 5);
}

void ScatterMetallic(const float roughness) {
	mat.rayDirection = vec4(normalize(reflect(mat.rayDirection.xyz, mat.normal.xyz) + roughness*roughness*RandomInUnitSphere(mat.randomSeed)), float(camera.zfar));
}

void ScatterDieletric(const float indexOfRefraction) {
	const float dot = dot(mat.rayDirection.xyz, mat.normal.xyz);
	const vec3 outwardNormal = dot > 0 ? -mat.normal.xyz : mat.normal.xyz;
	const float niOverNt = dot > 0 ? indexOfRefraction : 1 / indexOfRefraction;
	const float cosine = dot > 0 ? indexOfRefraction * dot : -dot;
	const vec3 refracted = refract(mat.rayDirection.xyz, outwardNormal, niOverNt);
	const float reflectProb = refracted != vec3(0) ? Schlick(cosine, indexOfRefraction) : 1;
	
	if (RandomFloat(mat.randomSeed) < reflectProb) {
		mat.rayDirection = vec4(reflect(mat.rayDirection.xyz, mat.normal.xyz), float(camera.zfar));
	} else {
		mat.rayDirection = vec4(refracted, float(camera.zfar));
	}
}

void ScatterLambertian(const float roughness) {
	mat.color.a = roughness;
	// mat.rayDirection = vec4(normalize(mix(mat.normal.xyz, mat.normal.xyz+RandomInUnitSphere(mat.randomSeed), roughness*roughness)), float(camera.zfar));
}

// vec3 Specular(vec3 rayOrigin, vec3 hitPosition, vec3 hitNormal, inout uint seed) {
// 	vec3 specularColor = vec3(0);
// 	// PBR lighting
// 	float distance = distance(rayOrigin, hitPosition);
// 	vec3 N = hitNormal;
// 	vec3 V = normalize(rayOrigin - hitPosition);
// 	for (int activeLightIndex = 0; activeLightIndex < MAX_ACTIVE_LIGHTS; activeLightIndex++) {
// 		LightSource light = lightSources[activeLightIndex];
// 		if (light.radius > 0.0) {
// 			// Calculate light radiance at distance
// 			float dist = length(light.position - hitPosition);
// 			vec3 radiance = light.color * light.intensity / (dist*dist);
// 			vec3 randomPointInLightSource = light.position + RandomInUnitSphere(seed) * light.radius;
// 			vec3 L = normalize(randomPointInLightSource - hitPosition);
// 			const float radianceThreshold = 1e-10;
// 			if (length(radiance) > radianceThreshold) {
// 				bool shadowed = false;
// 				if (HardShadows) {
// 					if (dot(L, N) > 0) {
// 						vec3 shadowRayStart = hitPosition + V*GetOptimalBounceStartDistance(distance); // starting shadow ray just outside the surface this way solves precision issues when casting shadows
// 						rayQueryEXT rayQuery;
// 						rayQueryInitializeEXT(rayQuery, topLevelAS, gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsOpaqueEXT | gl_RayFlagsSkipClosestHitShaderEXT, RAY_TRACE_MASK_SHADOWS, shadowRayStart, float(camera.znear), L, length(light.position - hitPosition) - light.radius);
// 						while(rayQueryProceedEXT(rayQuery)){}
// 						if (rayQueryGetIntersectionTypeEXT(rayQuery, true) != gl_RayQueryCommittedIntersectionNoneEXT) {
// 							shadowed = true;
// 						}
// 					}
// 				}
// 				if (!shadowed) {
// 					specularColor += clamp(dot(N, L), 0,1) * radiance;
// 				}
// 			}
// 		}
// 	}
// 	return specularColor;
// }

void main() {
	const Material material = GetGeometry(mat.entityInstanceIndex, mat.geometryIndex).material;
	
	// Mat -> Tex
	tex.position = mat.position;
	tex.normal = mat.normal;
	tex.color = mat.color * vec4(material.visibility.baseColor) / 255.0;
	tex.metallic = float(material.visibility.metallic) / 255.0;
	tex.roughness = float(material.visibility.roughness) / 255.0;
	tex.emission = material.visibility.emission;
	
	// Procedural textures
	for (int i = 0; i < 8; ++i) if (material.visibility.texFactors[i] > 0) {
		tex.factor = material.visibility.texFactors[i];
		executeCallableEXT(material.visibility.textures[i], CALL_DATA_LOCATION_TEXTURE);
	}
	// executeCallableEXT(1, CALL_DATA_LOCATION_TEXTURE);
	
	// Tex -> Mat
	mat.color.rgb = tex.color.rgb + tex.color.rgb*tex.emission;
	mat.color.a = tex.roughness * (1.0 - tex.metallic) * 0.001 + tex.emission;
	mat.normal.xyz = tex.normal.xyz;
	
	float indexOfRefraction = 1.45;// float(material.visibility.metallic) / 50.0;
	
	// Scatter
	const float dot = dot(mat.rayDirection.xyz, mat.normal.xyz);
	const vec3 outwardNormal = dot > 0 ? -mat.normal.xyz : mat.normal.xyz;
	const float niOverNt = dot > 0 ? indexOfRefraction : 1 / indexOfRefraction;
	const float cosine = dot > 0 ? indexOfRefraction * dot : -dot;
	const vec3 refracted = mix(refract(mat.rayDirection.xyz, outwardNormal, niOverNt), normalize(mat.normal.xyz+RandomInUnitSphere(mat.randomSeed)), tex.roughness * tex.color.a);
	const float reflectProb = ((refracted != vec3(0) ? Schlick(cosine, indexOfRefraction) : 1) + tex.metallic) + (tex.color.a*2-1);
	if (RandomFloat(mat.randomSeed) < reflectProb) {
		mat.rayDirection = vec4(reflect(mat.rayDirection.xyz, mat.normal.xyz) + tex.roughness*tex.roughness*(mat.normal.xyz+RandomInUnitSphere(mat.randomSeed)), float(camera.zfar));
	} else {
		mat.rayDirection = vec4(refracted, float(camera.zfar));
	}
	
	if (DebugMaterial(mat, tex)) return;
}
