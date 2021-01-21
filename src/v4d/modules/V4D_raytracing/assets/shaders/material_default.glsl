#define RAY_TRACING
#include "v4d/modules/V4D_raytracing/glsl_includes/set0.glsl"
#include "noise.glsl"

#shader rcall

layout(location = CALL_DATA_LOCATION_MATERIAL) callableDataInEXT ProceduralMaterialCall mat;
layout(location = CALL_DATA_LOCATION_TEXTURE) callableDataEXT ProceduralTextureCall tex;

//https://blog.demofox.org/2017/01/09/raytracing-reflection-refraction-fresnel-total-internal-reflection-and-beers-law/
float FresnelReflectAmount(float n1, float n2, float reflectionAmount) {
	// Schlick aproximation
	float r0 = (n1-n2) / (n1+n2);
	r0 *= r0;
	float cosX = -dot(mat.rayPayload.normal.xyz, mat.rayPayload.rayDirection.xyz);
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
	return reflectionAmount + (1.0-reflectionAmount) * ret;
}

float Schlick(const float cosine, const float indexOfRefraction) {
	float r0 = (1 - indexOfRefraction) / (1 + indexOfRefraction);
	r0 *= r0;
	return r0 + (1 - r0) * pow(1 - cosine, 5);
}

// void Water(float indexOfRefraction) {
// 	const float vDotN = dot(mat.rayDirection.xyz, mat.normal.xyz);
// 	const vec3 outwardNormal = vDotN > 0 ? -mat.normal.xyz : mat.normal.xyz;
// 	const float niOverNt = vDotN > 0 ? indexOfRefraction : 1.0 / indexOfRefraction;
// 	const float cosine = vDotN > 0 ? indexOfRefraction * vDotN : -vDotN;
// 	const vec3 refracted = refract(mat.rayDirection.xyz, outwardNormal, niOverNt);
// 	const float reflectProb = (refracted != vec3(0))? Schlick(cosine, indexOfRefraction) : 1;
// 	if (RandomFloat(mat.randomSeed) < reflectProb) {
// 		mat.rayDirection = vec4(reflect(mat.rayDirection.xyz, mat.normal.xyz), float(camera.zfar));
// 	} else {
// 		mat.rayDirection = vec4(refracted, float(camera.zfar));
// 	}
// }

void main() {
	const Material material = GetGeometry(mat.rayPayload.entityInstanceIndex, mat.rayPayload.geometryIndex).material;
	
	// Prepare info for textures
	tex.materialPayload = mat;
	tex.normal = mat.rayPayload.normal.xyz;
	tex.albedo = mat.rayPayload.color * vec4(material.visibility.baseColor) / 255.0;
	tex.metallic = float(material.visibility.metallic) / 255.0;
	tex.roughness = float(material.visibility.roughness) / 255.0;
	tex.emission = tex.albedo.rgb * material.visibility.emission;
	float indexOfRefraction = float(material.visibility.indexOfRefraction) / 50.0;
	
	// Procedural textures
	for (int i = 0; i < 8; ++i) if (material.visibility.texFactors[i] > 0) {
		tex.factor = material.visibility.texFactors[i];
		executeCallableEXT(material.visibility.textures[i], CALL_DATA_LOCATION_TEXTURE);
	}
	// executeCallableEXT(2, CALL_DATA_LOCATION_TEXTURE); // checkerboard
	
	if (DebugMaterial(tex, mat.rayPayload.color)) return;
	
	// Info from ray
		// mat.rayPayload.rayDirection.xyz = current ray direction in object space
		// mat.rayPayload.normal.xyz = face normal in object space
		// mat.rayPayload.normal.w = total distance traveled by ray so far
		// mat.rayPayload.position.xyz = hit position in object space
		// mat.rayPayload.position.w = hit distance of this ray
	
	// Info from texture
		// tex.albedo.rgba
		// tex.emission.rgb
		// tex.normal.xyz = pertubed surface normal in object space
		// tex.metallic
		// tex.roughness
		// tex.height
	
	// To write as output for material
		// mat.emission.rgb
		// mat.reflectance.rgb
		// mat.bounceDirection.xyz = direction to bounce the next ray, in object space
		
	if (dot(tex.emission.rgb, tex.emission.rgb) > 0) {
		// Emissive
		mat.emission.rgb = tex.emission.rgb;
		mat.reflectance = vec3(0);
	} else {
		vec3 randomBounceDirection = normalize(mat.rayPayload.normal.xyz + RandomInUnitSphere(mat.rayPayload.randomSeed));
		bool isMetallic = tex.metallic>0;// RandomFloat(mat.rayPayload.randomSeed) < tex.metallic;
		if (isMetallic) {
			// Metallic
			mat.reflectance.rgb = tex.albedo.rgb;
			mat.bounceDirection.xyz = normalize(mix(reflect(mat.rayPayload.rayDirection.xyz, tex.normal.xyz), randomBounceDirection, tex.roughness));
		} else {
			// Dielectric
			mat.bounceDirection.xyz = randomBounceDirection;
			const float vDotN = -dot(mat.rayPayload.rayDirection.xyz, tex.normal.xyz);
			bool isTransparent = RandomFloat(mat.rayPayload.randomSeed) > tex.albedo.a;
			if (isTransparent) {
				// Transparent
				const vec3 outwardNormal = vDotN > 0 ? -tex.normal.xyz : tex.normal.xyz;
				const float niOverNt = vDotN > 0 ? indexOfRefraction : 1.0 / indexOfRefraction;
				const float cosine = vDotN > 0 ? indexOfRefraction * vDotN : -vDotN;
				const vec3 refracted = refract(mat.rayPayload.rayDirection.xyz, tex.normal.xyz, niOverNt);
				const float reflectProb = (refracted != vec3(0))? Schlick(cosine, indexOfRefraction) : 1;
				if (RandomFloat(mat.rayPayload.randomSeed) < reflectProb) {
					mat.bounceDirection.xyz = mix(reflect(mat.rayPayload.rayDirection.xyz, tex.normal.xyz), mat.bounceDirection.xyz, tex.roughness);
				} else {
					mat.bounceDirection.xyz = mix(refracted, mat.bounceDirection.xyz, tex.roughness);
				}
				mat.reflectance = tex.albedo.rgb;
			} else {
				// Opaque dielectric
				float fresnelAmount = 0.7;
				mat.rayPayload.color.rgb = tex.albedo.rgb;
				mat.reflectance = tex.albedo.rgb * FresnelReflectAmount(1.0, indexOfRefraction, fresnelAmount) * fresnelAmount;
				mat.bounceDirection = normalize(mix(randomBounceDirection, reflect(mat.rayPayload.rayDirection.xyz, tex.normal.xyz), fresnelAmount));
				mat.bounceShadowRays = tex.roughness;
				mat.rayPayload.normal.xyz = tex.normal.xyz;
			}
		}
	}
	
}
