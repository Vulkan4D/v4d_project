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
	
	WriteRayPayload(ray);
	
	uint64_t material = GetGeometry().material;
	float mat_metallic = float(uint8_t(material)) / 255.0;
	float mat_roughness = float(uint8_t(material>>8)) / 255.0;
	float mat_albedo_r = float(uint8_t(material>>16)) / 255.0;
	float mat_albedo_g = float(uint8_t(material>>24)) / 255.0;
	float mat_albedo_b = float(uint8_t(material>>32)) / 255.0;
	float mat_emission_r = float(uint8_t(material>>40)) / 255.0;
	float mat_emission_g = float(uint8_t(material>>48)) / 255.0;
	float mat_emission_b = float(uint8_t(material>>56)) / 255.0;
	if (HasVertexColor()) {
		ray.albedo = (
			+ GetVertexColor(i0).rgb * barycentricCoords.x
			+ GetVertexColor(i1).rgb * barycentricCoords.y
			+ GetVertexColor(i2).rgb * barycentricCoords.z
		) * vec3(mat_albedo_r, mat_albedo_g, mat_albedo_b);
	} else {
		ray.albedo = vec3(mat_albedo_r, mat_albedo_g, mat_albedo_b);
	}
	ray.emission = vec3(mat_emission_r, mat_emission_g, mat_emission_b) * 10.0;
	ray.normal = DoubleSidedNormals(normalize(GetModelNormalViewMatrix() * normal));
	if (length(ray.emission) > 0) {
		ray.metallic = 0;
		ray.roughness = 0;
	} else {
		ray.metallic = mat_metallic;
		ray.roughness = mat_roughness;
	}
}
