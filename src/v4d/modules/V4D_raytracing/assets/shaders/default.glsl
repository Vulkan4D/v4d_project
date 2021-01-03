#define RAY_TRACING
#include "v4d/modules/V4D_raytracing/glsl_includes/set0.glsl"


#############################################################
#shader rendering.rchit

// #define DISABLE_SHADOWS
// #include "v4d/modules/V4D_raytracing/glsl_includes/core_pbr.glsl"

hitAttributeEXT vec3 hitAttribs;

layout(location = RAY_PAYLOAD_LOCATION_RENDERING) rayPayloadInEXT RenderingPayload ray;

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
	
	float indexOfRefraction = float(material.indexOfRefraction) / 50.0;
	
	// Emission
	if (material.emission > 0) {
		ray.color = vec4(color.rgb * material.emission, color.a);
	} else {
		// // Rim
		// vec4 rim = vec4(material.rim) / 255.0;
		
		// PBR Textures
		tex.albedo = color.rgb;
		tex.opacity = color.a;
		tex.normal = normal;
		tex.metallic = float(material.metallic) / 255.0;
		tex.roughness = float(material.roughness) / 255.0;
		tex.localHitPosition = gl_ObjectRayOriginEXT + gl_ObjectRayDirectionEXT * gl_HitTEXT;
		tex.distance = gl_HitTEXT; //TODO use total distance instead, from new ray payload param
		for (int i = 0; i < 8; ++i) if (material.texFactors[i] > 0) {
			tex.factor = material.texFactors[i];
			executeCallableEXT(material.textures[i], 0);
		}
		color.rgb = tex.albedo.rgb;
		color.a = tex.opacity;
		float metallic = tex.metallic;
		float roughness = tex.roughness;
		normal = DoubleSidedNormals(normalize(GetModelNormalViewMatrix() * tex.normal));

		ray.color = color;
		if (metallic > 0) {
			ray.color.a = 1.0 - FresnelReflectAmount(1.0, indexOfRefraction, normal, gl_WorldRayDirectionEXT, metallic);
			ScatterMetallic(ray, roughness, gl_WorldRayDirectionEXT, normal);
		} else if (color.a < 1) {
			ScatterDieletric(ray, indexOfRefraction, gl_WorldRayDirectionEXT, normal);
		} else {
			ScatterLambertian(ray, roughness, normal);
		}
	}
	
	DebugRay(ray, color.rgb, normal, material.emission, tex.metallic, tex.roughness);
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
