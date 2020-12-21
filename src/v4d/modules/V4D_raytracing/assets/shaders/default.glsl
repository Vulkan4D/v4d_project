#define RAY_TRACING
#include "v4d/modules/V4D_raytracing/glsl_includes/set0.glsl"


#############################################################
#shader rchit

hitAttributeEXT vec3 hitAttribs;

layout(location = 0) rayPayloadInEXT RayTracingPayload ray;

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
	ray.albedo = vec3(material.baseColor.rgb) / 255.0;
	if (HasVertexColor()) {
		ray.albedo *= 
			+ GetVertexColor(i0).rgb * barycentricCoords.x
			+ GetVertexColor(i1).rgb * barycentricCoords.y
			+ GetVertexColor(i2).rgb * barycentricCoords.z
		;
	}
	
	// Glass
	ray.opacity = float(material.baseColor.a) / 255.0;
	ray.indexOfRefraction = float(material.indexOfRefraction) / 50.0;
	
	// Normal
	ray.normal = DoubleSidedNormals(normalize(GetModelNormalViewMatrix() * normal));
	
	// Emission
	if (material.emission > 0) {
		ray.metallic = 0;
		ray.roughness = 0;
		ray.emission = ray.albedo * material.emission;
	} else {
		// PBR
		ray.metallic = float(material.metallic) / 255.0;
		ray.roughness = float(material.roughness) / 255.0;
		// Rim
		ray.rim = vec4(material.rim) / 255.0;
	}
	
	//TODO maps
	// material.normalMap
	// material.albedoMap
	// material.metallicMap
	// material.roughnessMap
}
