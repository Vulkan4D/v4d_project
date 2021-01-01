#define RAY_TRACING
#include "v4d/modules/V4D_raytracing/glsl_includes/set0.glsl"


#############################################################
#shader rendering.rchit

hitAttributeEXT vec3 hitAttribs;

layout(location = 0) rayPayloadInEXT RayTracingPayload ray;

struct ProceduralTextureCall {
	vec3 albedo;
	vec3 normal;
	float metallic;
	float roughness;
	float opacity;
	// input-only
	vec3 localHitPosition;
	float distance;
	float factor;
};
layout(location = 0) callableDataEXT ProceduralTextureCall tex;

void main() {
	uint i0 = GetIndex(0);
	uint i1 = GetIndex(1);
	uint i2 = GetIndex(2);
	
	// Interpolate fragment
	vec3 barycentricCoords = vec3(1.0f - hitAttribs.x - hitAttribs.y, hitAttribs.x, hitAttribs.y);
	// vec3 pos = (
	// 	+ GetVertexPosition(i0) * barycentricCoords.x
	// 	+ GetVertexPosition(i1) * barycentricCoords.y
	// 	+ GetVertexPosition(i2) * barycentricCoords.z
	// );
	vec3 normal = normalize(
		+ GetVertexNormal(i0) * barycentricCoords.x
		+ GetVertexNormal(i1) * barycentricCoords.y
		+ GetVertexNormal(i2) * barycentricCoords.z
	);
	// vec2 uv = HasVertexUV()? (
	// 	+ GetVertexUV(i0) * barycentricCoords.x
	// 	+ GetVertexUV(i1) * barycentricCoords.y
	// 	+ GetVertexUV(i2) * barycentricCoords.z
	// ) : vec2(0);
	
	// Material
	Material material = GetGeometry().material;
	
	WriteRayPayload(ray);
	
	// BaseColor
	vec4 color = vec4(material.baseColor) / 255.0;
	if (HasVertexColor()) {
		color *= 
			+ GetVertexColor(i0) * barycentricCoords.x
			+ GetVertexColor(i1) * barycentricCoords.y
			+ GetVertexColor(i2) * barycentricCoords.z
		;
	}
	ray.albedo = color.rgb;
	ray.opacity = color.a;
	
	ray.indexOfRefraction = float(material.indexOfRefraction) / 50.0;
	
	// Emission
	if (material.emission > 0) {
		ray.metallic = 0;
		ray.roughness = 0;
		ray.emission = ray.albedo * material.emission;
	} else {
		// Rim
		ray.rim = vec4(material.rim) / 255.0;
		
		// PBR Textures
		tex.albedo = ray.albedo;
		tex.normal = normal;
		tex.metallic = float(material.metallic) / 255.0;
		tex.roughness = float(material.roughness) / 255.0;
		tex.opacity = ray.opacity;
		tex.localHitPosition = gl_ObjectRayOriginEXT + gl_ObjectRayDirectionEXT * gl_HitTEXT;
		tex.distance = gl_HitTEXT; //TODO use total distance instead, from new ray payload param
		for (int i = 0; i < 8; ++i) if (material.texFactors[i] > 0) {
			tex.factor = material.texFactors[i];
			executeCallableEXT(material.textures[i], 0);
		}
		ray.albedo = tex.albedo;
		normal = tex.normal;
		ray.metallic = tex.metallic;
		ray.roughness = tex.roughness;
		ray.opacity = tex.opacity;
	}
	
	// Normal
	ray.normal = DoubleSidedNormals(normalize(GetModelNormalViewMatrix() * normal));
}


#############################################################
#shader rendering.rahit

void main(){}


#############################################################
#shader depth.rchit

void main(){}


#############################################################
#shader spectral.rchit

void main(){}
